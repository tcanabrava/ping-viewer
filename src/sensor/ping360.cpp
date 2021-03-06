#include "ping360.h"

#include <algorithm>
#include <functional>

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QStringList>
#include <QThread>
#include <QUrl>

#include "hexvalidator.h"
#include "link/seriallink.h"
#include "networkmanager.h"
#include "networktool.h"
#include "notificationmanager.h"
#include "settingsmanager.h"

Q_LOGGING_CATEGORY(PING_PROTOCOL_PING360, "ping.protocol.ping360")

// firmware constants
const uint16_t Ping360::_firmwareMaxNumberOfPoints = 1200;
const uint16_t Ping360::_firmwareMaxTransmitDuration = 500;
const uint16_t Ping360::_firmwareMinTransmitDuration = 5;
const uint16_t Ping360::_firmwareMinSamplePeriod = 80;
// The firmware defaults at boot
const uint8_t Ping360::_firmwareDefaultGainSetting = 0;
const uint16_t Ping360::_firmwareDefaultAngle = 0;
const uint16_t Ping360::_firmwareDefaultTransmitDuration = 32;
const uint16_t Ping360::_firmwareDefaultSamplePeriod = 80;
const uint16_t Ping360::_firmwareDefaultTransmitFrequency = 740;
const uint16_t Ping360::_firmwareDefaultNumberOfSamples = 1024;

// The default transmit frequency to operate with
const uint16_t Ping360::_viewerDefaultTransmitFrequency = 750;
const uint16_t Ping360::_viewerDefaultNumberOfSamples = _firmwareMaxNumberOfPoints;

Ping360::Ping360()
    :PingSensor()
{
    // QVector crashs when constructed in initialization list
    _data = QVector<double>(_maxNumberOfPoints, 0);

    setControlPanel({"qrc:/Ping360ControlPanel.qml"});
    setSensorVisualizer({"qrc:/Ping360Visualizer.qml"});

    connect(this, &Sensor::connectionOpen, this, &Ping360::startPreConfigurationProcess);

    // Add timer for worst case scenario
    _timeoutProfileMessage.setInterval(_sensorTimeout);
    _baudrateConfigurationTimer.setInterval(100);

    connect(&_timeoutProfileMessage, &QTimer::timeout, this, [this] {
        qCWarning(PING_PROTOCOL_PING360) << "Profile message timeout, new request will be done.";
        requestNextProfile();
    });

    connect(&_baudrateConfigurationTimer, &QTimer::timeout, this, [this] {
        qCWarning(PING_PROTOCOL_PING360) << "Device Info timeout";
        checkBaudrateProcess();
    });

    // Start timer to calculate frequency for each message type
    _messageElapsedTimer.start();
}

void Ping360::startPreConfigurationProcess()
{
    // Fetch sensor configuration to update class variables
    // TODO: Ping base class should abstract the request message to allow version compatibility between protocol versions
    common_general_request msg;
    msg.set_requested_id(CommonId::DEVICE_INFORMATION);
    msg.updateChecksum();
    writeMessage(msg);
}

void Ping360::checkBaudrateProcess()
{
    // We use the pre configuration message to check for valid baud rates
    startPreConfigurationProcess();

    static int count = _ABRTotalNumberOfMessages;
    if(count--) {
        _baudrateConfigurationTimer.start();
    } else {
        detectBaudrates();
        count = 20;
    }
}

void Ping360::loadLastSensorConfigurationSettings()
{
    //TODO
}

void Ping360::updateSensorConfigurationSettings()
{
    //TODO
}

void Ping360::connectLink(LinkType connType, const QStringList& connString)
{
    Sensor::connectLink(LinkConfiguration{connType, connString});
}

void Ping360::requestNextProfile()
{
    // Calculate the next delta step
    int steps = _angular_speed;
    if(_reverse_direction) {
        steps *= -1;
    }

    // Check if steps is in sector
    auto isInside = [this](int iSteps) -> bool {
        int relativeAngle = (iSteps + angle() + _angularResolutionGrad)%_angularResolutionGrad;
        if(relativeAngle >= _angularResolutionGrad/2)
        {
            relativeAngle -= _angularResolutionGrad;
        }
        return std::clamp(relativeAngle, -_sectorSize/2, _sectorSize/2) == relativeAngle;
    };

    // Move the other direction to be in sector
    if(!isInside(steps)) {
        _reverse_direction = !_reverse_direction;
        steps *= -1;
    }

    // If we are not inside yet, we are not in section, go to zero
    if(!isInside(steps)) {
        _reverse_direction = !_reverse_direction;
        steps = -angle();
    }

    deltaStep(steps);
}

void Ping360::handleMessage(const ping_message& msg)
{
    qCDebug(PING_PROTOCOL_PING360) << "Handling Message:" << msg.message_id();

    // Update frequency for each
    messageFrequencies[msg.message_id()].setElapsed(_messageElapsedTimer.elapsed());
    // Since we don't have a huge number of messages and this variable is pretty simple,
    // we can use a single signal to update someone about the frequency update
    emit messageFrequencyChanged();

    switch (msg.message_id()) {

    case CommonId::DEVICE_INFORMATION: {
        if(_configuring) {
            _baudrateConfigurationTimer.start();
            checkBaudrateProcess();
            return;
        } else {
            _baudrateConfigurationTimer.stop();
            _timeoutProfileMessage.start();
            requestNextProfile();
            return;
        }
    }

    case Ping360Id::DEVICE_DATA: {
        // Parse message
        const ping360_device_data deviceData(msg);

        _angle = deviceData.angle();

        _data.resize(deviceData.data_length());
        for (int i = 0; i < deviceData.data_length(); i++) {
            _data.replace(i, deviceData.data()[i] / 255.0);
        }

        // TODO: doublecheck what we are getting and what we want
        // some parameter combinations are not valid and the sensor will automatically adjust
        // in order to detect this, we will have to track our last commanded values separately
        // from our presently commanded values
        emit angleChanged();

        // Only emit data changed when inside sector range
        if(_data.size()) {
            // Update total number of pings
            _ping_number++;

            if (_sectorSize == 400
                    || (angle() >= _angularResolutionGrad - _sectorSize/2) || (angle() <= _sectorSize/2)) {
                emit dataChanged();
            }
        }

        // request another transmission
        requestNextProfile();

        // Restart timer
        _timeoutProfileMessage.start();

        break;
    }

    case CommonId::NACK: {
        const common_nack nack(msg);
        if (nack.nacked_id() == Ping360Id::TRANSDUCER) {
            qCWarning(PING_PROTOCOL_PING360) << "transducer control was NACKED, reverting to default settings";

            _gain_setting = _firmwareDefaultGainSetting;
            _transmit_duration = _firmwareDefaultTransmitDuration;
            _sample_period = _firmwareDefaultSamplePeriod;
            _transmit_frequency = _viewerDefaultTransmitFrequency;
            _num_points = _viewerDefaultNumberOfSamples;

            // request another transmission
            requestNextProfile();

            // restart timer
            _timeoutProfileMessage.start();

            emit gainSettingChanged();
            emit transmitDurationChanged();
            emit samplePeriodChanged();
            emit transmitFrequencyChanged();
            emit numberOfPointsChanged();
            emit rangeChanged();
        }
        break;
    }

    default:
        qWarning(PING_PROTOCOL_PING360) << "UNHANDLED MESSAGE ID:" << msg.message_id();
        break;
    }
    emit parsedMsgsUpdate();
}

void Ping360::firmwareUpdate(QString fileUrl, bool sendPingGotoBootloader, int baud, bool verify)
{
    Q_UNUSED(fileUrl)
    Q_UNUSED(sendPingGotoBootloader)
    Q_UNUSED(baud)
    Q_UNUSED(verify)
    //TODO
}

void Ping360::flash(const QString& fileUrl, bool sendPingGotoBootloader, int baud, bool verify)
{
    Q_UNUSED(fileUrl)
    Q_UNUSED(sendPingGotoBootloader)
    Q_UNUSED(baud)
    Q_UNUSED(verify)
    //TODO
}

void Ping360::setLastSensorConfiguration()
{
    //TODO
}

void Ping360::printSensorInformation() const
{
    qCDebug(PING_PROTOCOL_PING360) << "Ping360 Status:";
    //TODO
}

void Ping360::checkNewFirmwareInGitHubPayload(const QJsonDocument& jsonDocument)
{
    Q_UNUSED(jsonDocument)
    //TODO
}

void Ping360::resetSensorLocalVariables()
{
    //TODO
}

const QList<int>& Ping360::validBaudRates()
{
    static QList<int> validBaudRates;
    if(validBaudRates.isEmpty()) {
        for(const auto& variant : _validBaudRates) {
            validBaudRates.append(variant.toInt());
        }
    }
    return validBaudRates;
}

const QVariantList& Ping360::validBaudRatesAsVariantList() const
{
    return _validBaudRates;
}

void Ping360::setBaudRate(int baudRate)
{
    // It's only possible to change baudrates in serial connections
    if(link()->type() != LinkType::Serial) {
        return;
    }

    // Since ping360 uses automatic baudrate detection
    // it's necessary to start the connection to force baud rate changes
    SerialLink* serialLink = dynamic_cast<SerialLink*>(link());
    if(!serialLink) {
        qCWarning(PING_PROTOCOL_PING360) << "Link is serial type, but cast was not possible!";
        return;
    }

    qCDebug(PING_PROTOCOL_PING360) << "Moving to baud rate:" << baudRate;
    serialLink->setBaudRate(baudRate);
    link()->startConnection();
    emit linkUpdate();
}

void Ping360::setBaudRateAndRequestProfile(int baudRate)
{
    setBaudRate(baudRate);
    QThread::usleep(100);
    requestNextProfile();
}

void Ping360::detectBaudrates()
{
    // Check all valid baudrates
    static int index = 0;

    // baudrate to error
    static QMap<int, int> baudRateToError;

    // last parser error count
    static int lastParserErrorCount = 0;
    static int lastParserMsgsCount = 0;

    if(!_configuring) {
        return;
    }

    if(index < validBaudRates().size()) {
        setBaudRate(validBaudRates()[index]);
    }

    int lastCounter = -1;

    // We have already something to calculate
    if(index != 0) {
        // Get previous baud rate and error margin
        lastCounter =
            // Number of messages that failed to parse
            parserErrors() - lastParserErrorCount +
            // Number of messages that was lost
            _ABRTotalNumberOfMessages - (parsedMsgs() - lastParserMsgsCount);

        baudRateToError[validBaudRates()[index - 1]] = lastCounter;
    }
    lastParserErrorCount = parserErrors();
    lastParserMsgsCount = parsedMsgs();
    index++;

    // We are in the end of the list or someone is the winner!
    if(index == validBaudRates().size() || lastCounter == 0) {
        _configuring = false;

        // We are not in the end of the list, so there is a valid baud rate that is faster
        // than the lowest speed
        if(index != validBaudRates().size()) {
            index = 0;
            while(baudRateToError[validBaudRates()[index]] != 0) {
                index++;
            }
        } else {
            // We are in the end of the list, we are going to use the lowest baud rate
            index = validBaudRates().size() - 1;
        }

        setBaudRate(validBaudRates()[index]);
    }
}

Ping360::~Ping360()
{
    updateSensorConfigurationSettings();
}
