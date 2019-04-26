#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QDebug>
#include <QRegularExpression>

#if defined(QT_DEBUG) && defined(Q_OS_WIN)
    #include <KCrash>
#endif

#include "abstractlink.h"
#include "filemanager.h"
#include "flasher.h"
#include "linkconfiguration.h"
#include "logger.h"
#include "notificationmanager.h"
#include "ping.h"
#include "settingsmanager.h"
#include "stylemanager.h"
#include "util.h"
#include "waterfall.h"

// Register message enums to qml
#include "pingmessage/pingmessage.h"

Q_DECLARE_LOGGING_CATEGORY(mainCategory)

PING_LOGGING_CATEGORY(mainCategory, "ping.main")

int main(int argc, char *argv[])
{
    // Start logger ASAP
    Logger::installHandler();

    QCoreApplication::setOrganizationName("Blue Robotics Inc.");
    QCoreApplication::setOrganizationDomain("bluerobotics.com");
    QCoreApplication::setApplicationName("Ping Viewer");

    QQuickStyle::setStyle("Material");

    // Singleton register
    qRegisterMetaType<AbstractLinkNamespace::LinkType>();
    qmlRegisterSingletonType<FileManager>("FileManager", 1, 0, "FileManager", FileManager::qmlSingletonRegister);
    qmlRegisterSingletonType<Logger>("Logger", 1, 0, "Logger", Logger::qmlSingletonRegister);
    qmlRegisterSingletonType<NotificationManager>("NotificationManager", 1, 0, "NotificationManager",
            NotificationManager::qmlSingletonRegister);
    qmlRegisterSingletonType<SettingsManager>("SettingsManager", 1, 0, "SettingsManager",
            SettingsManager::qmlSingletonRegister);
    qmlRegisterSingletonType<StyleManager>("StyleManager", 1, 0, "StyleManager", StyleManager::qmlSingletonRegister);
    qmlRegisterSingletonType<Util>("Util", 1, 0, "Util", Util::qmlSingletonRegister);

    // Normal register
    qmlRegisterType<AbstractLink>("AbstractLink", 1, 0, "AbstractLink");
    qmlRegisterType<Flasher>("Flasher", 1, 0, "Flasher");
    qmlRegisterType<LinkConfiguration>("LinkConfiguration", 1, 0, "LinkConfiguration");
    qmlRegisterType<Ping>("Ping", 1, 0, "Ping");
    qmlRegisterType<Waterfall>("Waterfall", 1, 0, "Waterfall");

    // Uncreatable register
    qmlRegisterUncreatableMetaObject(
        Ping1DNamespace::staticMetaObject, "Ping1DNamespace", 1, 0, "Ping1DNamespace", "This is a enum."
    );

    qmlRegisterUncreatableMetaObject(
        AbstractLinkNamespace::staticMetaObject,
        "AbstractLinkNamespace", 1, 0, "AbstractLinkNamespace", "This is another enum."
    );

    QApplication app(argc, argv);

    // DPI support and HiDPI pixmaps
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QQmlApplicationEngine engine;

    // Load the QML and set the Context
    // Logo
#ifdef QT_NO_DEBUG
    engine.load(QUrl(QStringLiteral("qrc:/Logo.qml")));
    app.exec();
#endif

    // Function used in CI to test runtime errors
    // After 5 seconds, check if qml engine was loaded
#ifdef AUTO_KILL
    QTimer::singleShot(5000, [&app, &engine]() {
        if(engine.rootObjects().isEmpty()) {
            printf("Application failed to load GUI!");
            app.exit(-1);
        }
        app.exit(0);
    });
#endif

    engine.rootContext()->setContextProperty("GitVersion", QStringLiteral(GIT_VERSION));
    engine.rootContext()->setContextProperty("GitVersionDate", QStringLiteral(GIT_VERSION_DATE));
    engine.rootContext()->setContextProperty("GitTag", QStringLiteral(GIT_TAG));
    engine.rootContext()->setContextProperty("GitUrl", QStringLiteral(GIT_URL));
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));

    qCInfo(mainCategory) << "Git version:" << GIT_VERSION;
    qCInfo(mainCategory) << "Git version date:" << GIT_VERSION_DATE;
    qCInfo(mainCategory) << "Git tag:" << GIT_TAG;
    qCInfo(mainCategory) << "Git url:" << GIT_URL;

    StyleManager::self()->setApplication(&app);
    StyleManager::self()->setQmlEngine(&engine);

#if defined(QT_DEBUG) && defined(Q_OS_WIN)
    // Start KCrash
    KCrash::initialize();
#endif

    return app.exec();
}
