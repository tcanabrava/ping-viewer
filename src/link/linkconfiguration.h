#pragma once

#include <QDebug>
#include <QMap>
#include <QString>
#include <QStringList>

#include "abstractlinknamespace.h"
#include "ping-message-all.h"

/**
 * @brief Link configuration class
 *
 */
class LinkConfiguration : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Link configuration structure
     *
     */
    struct LinkConf {
        QStringList args;
        QString name;
        LinkType type = LinkType::None;
        //TODO: We should somehow make this class or this structure more abstract
        // and remove any protocol or device specific information
        // right now this is necessary, since link configuration is our default
        // link information structure
        PingDeviceType deviceType = PingDeviceType::UNKNOWN;
    };

    /**
     * @brief Link configuration errors
     *
     */
    enum Error {
        MissingConfiguration, // This can be used in future for warnings and not real errors
        NoErrors = 0,
        NoType,
        InvalidType,
        NoArgs,
        InvalidArgsNumber,
        ArgsAreEmpty,
        InvalidUrl,
    };

    /**
     * @brief Construct a new Link Configuration object
     *
     * @param linkType
     * @param args
     * @param name
     * @param deviceType
     */
    LinkConfiguration(
        LinkType linkType = LinkType::None,
        QStringList args = QStringList(),
        QString name = QString(),
        PingDeviceType deviceType = PingDeviceType::UNKNOWN
    ) : _linkConf{args, name, linkType, deviceType} {};

    /**
     * @brief Construct a new Link Configuration object
     *
     * @param confLinkStructure
     */
    LinkConfiguration(LinkConf& confLinkStructure)
        : _linkConf{confLinkStructure} {};

    /**
     * @brief Construct a new Link Configuration object
     *
     * @param other
     */
    LinkConfiguration(const LinkConfiguration& other)
        : QObject(nullptr)
        , _linkConf{other.configurationStruct()} {};

    /**
     * @brief Destroy the Link Configuration object
     *
     */
    ~LinkConfiguration() = default;

    /**
     * @brief Return a list with the arguments
     *
     * @return const QStringList*
     */
    const QStringList* args() const { return &_linkConf.args; };

    /**
     * @brief Return list as copy
     *
     * @return Q_INVOKABLE argsAsConst
     */
    Q_INVOKABLE QStringList argsAsConst() const { return _linkConf.args; };

    /**
     * @brief Return PingDeviceType enumartion for device specific identification
     *
     * @return PingDeviceType
     */
    PingDeviceType deviceType() const { return _linkConf.deviceType; };

    /**
     * @brief Set the Device Type object
     *
     * @param deviceType
     */
    void setDeviceType(const PingDeviceType deviceType) { _linkConf.deviceType = deviceType; };

    /**
     * @brief Check if type is the one in the configuration
     *
     * @param type
     * @return true
     * @return false
     */
    bool checkType(LinkType type) const { return _linkConf.type == type; };

    /**
     * @brief Return configuration as structure
     *
     * @return LinkConf
     */
    LinkConf configurationStruct() const { return _linkConf; };

    /**
     * @brief Return a pointer for the configuration structure
     *
     * @return const LinkConf*
     */
    const LinkConf* configurationStructPtr() const { return &_linkConf; };

    /**
     * @brief Create and return a configuration string
     *
     * @return const QString
     */
    Q_INVOKABLE const QString createConfString() const { return _linkConf.args.join(":"); };

    /**
     * @brief Create and return a configuration in string list format
     *
     * @return const QStringList
     */
    const QStringList createConfStringList() const { return _linkConf.args; };

    /**
     * @brief Create and return old style configuration link
     *
     * @return const QString
     */
    const QString createFullConfString() const;

    /**
     * @brief Create a full configuration in string list format
     *
     * @return const QStringList
     */
    const QStringList createFullConfStringList() const;

    /**
     * @brief Return error numbers
     *
     * @return Error
     */
    Error error() const;

    /**
     * @brief Convert error number in a friendly human message
     *
     * @param error
     * @return QString
     */
    static QString errorToString(Error error) { return _errorMap[error]; }

    /**
     * @brief Return error in a friendly human message
     *
     * @return QString
     */
    QString errorToString() const { return errorToString(error()); }

    /**
     * @brief Check if configuration is correct
     *
     * @return bool
     */
    Q_INVOKABLE bool isValid() const { return error() <= NoErrors; }

    /**
     * @brief Return configuration name
     *
     * @return Q_INVOKABLE name
     */
    Q_INVOKABLE QString name() const { return _linkConf.name; };

    /**
     * @brief Set configuration name
     *
     * @param name
     */
    void setName(QString name) { _linkConf.name = name; };

    /**
     * @brief Return serialport system path
     *
     * @return QString
     */
    QString serialPort();

    /**
     * @brief Return serial baudrate
     *
     * @return int
     */
    int serialBaudrate();

    /**
     * @brief Set the Type object
     *
     * @param type
     */
    void setType(LinkType type) { _linkConf.type = type; };

    /**
     * @brief Return link configuration type
     *
     * @return AbstractLinkNamespace::LinkType
     */
    Q_INVOKABLE AbstractLinkNamespace::LinkType type() const { return _linkConf.type; };

    /**
     * @brief Return the type in a human readable format
     *  TODO: move this function to somewhere else
     * @return QString
     */
    Q_INVOKABLE QString typeToString() const
    {
        switch(_linkConf.type) {
        case LinkType::None:
            return QStringLiteral("None");
        case LinkType::File:
            return QStringLiteral("File");
        case LinkType::Serial:
            return QStringLiteral("Serial");
        case LinkType::Udp:
            return QStringLiteral("UDP");
        case LinkType::Ping1DSimulation:
            return QStringLiteral("Ping1D Simulation");
        case LinkType::Ping360Simulation:
            return QStringLiteral("Ping360 Simulation");
        default :
            return QStringLiteral("Unknown");
        }
    };

    /**
     * @brief Will return argument with UDP host name
     *
     * @return QString
     */
    QString udpHost();

    /**
     * @brief Will return port used in UDP connection
     *
     * @return int
     */
    int udpPort();

    /**
     * @brief Copy operator
     *
     * @param other
     * @return LinkConfiguration&
     */
    LinkConfiguration& operator = (const LinkConfiguration& other)
    {
        this->_linkConf = other.configurationStruct();
        return *this;
    }

private:
    static const QMap<Error, QString> _errorMap;
    LinkConf _linkConf;
};

bool operator==(const LinkConfiguration& first, const LinkConfiguration& second);
QDebug operator<<(QDebug d, const LinkConfiguration& other);
QDataStream& operator<<(QDataStream &out, const LinkConfiguration linkConfiguration);
QDataStream& operator>>(QDataStream &in, LinkConfiguration &linkConfiguration);

Q_DECLARE_METATYPE(LinkConfiguration)
