#include <QDateTime>
#include <QStandardPaths>
#include <QQmlEngine>

#include "filemanager.h"
#include "logger.h"

PING_LOGGING_CATEGORY(FILEMANAGER, "ping.filemanager");

FileManager::FileManager()
    : _docDir(QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation)[0])
    , _fmDir(_docDir.dir.filePath(QStringLiteral("PingViewer")))
    , _gradientsDir(_fmDir.dir.filePath(QStringLiteral("Waterfall_Gradients")), fileTypeExtension[TXT])
    , _guiLogDir(_fmDir.dir.filePath(QStringLiteral("Gui_Log")), fileTypeExtension[TXT])
    , _picturesDir(_fmDir.dir.filePath(QStringLiteral("Pictures")), fileTypeExtension[PICTURE])
    , _sensorLogDir(_fmDir.dir.filePath(QStringLiteral("Sensor_Log")), fileTypeExtension[BINARY])
{
    // Check for folders and create if necessary
    auto rootDir = QDir();
    for(auto f : {&_fmDir, &_guiLogDir, &_picturesDir, &_sensorLogDir, &_gradientsDir}) {
        qCDebug(FILEMANAGER) << "Folder: " << f->dir;
        if(!f->dir.exists()) {
            qCDebug(FILEMANAGER) << "Create folder" << f->dir.path();
            f->ok = rootDir.mkpath(f->dir.path());
            qCDebug(FILEMANAGER) << (f->ok ? "Done." : ("Error while creating folder" + f->dir.path()));
            // Something is wrong, continue with the others
            if(!f->ok) {
                continue;
            }
        }

        // Everything is fine, but we need to make sure !
        f->ok = QFileInfo(f->dir.path()).isWritable();
    }
}

/*
usually macros are not nice, but sometimes they add a 
layer of maintenability for the software, if well used.
creates a variable named 'folder' in the current scope
or returns if no variable could be created.
 */
#define getFolderOrReturn(folderType) \
    auto folderIt = folderMap.find(folderType); \
    if(folderIt == folderMap.end()) { \
        qCWarning(FILEMANAGER) << "Folder pointer does not exist!"; \
        return {}; \
    } \
    auto *folder = (*folderIt);

QFileInfoList FileManager::getFilesFrom(Folder folderType)
{
    getFolderOrReturn(folderType);
    if(!folder->ok) {
        return {};
    }
    folder->dir.setSorting(QDir::Name);
    folder->dir.setFilter(QDir::Files);
    return folder->dir.entryInfoList();
}

QUrl FileManager::getPathFrom(FileManager::Folder folderType)
{
    getFolderOrReturn(folderType);
    return QUrl::fromLocalFile(folder->dir.path());
}

QObject* FileManager::qmlSingletonRegister(QQmlEngine* engine, QJSEngine* scriptEngine)
{
    Q_UNUSED(scriptEngine)

    engine->setObjectOwnership(self(), QQmlEngine::CppOwnership);
    return self();
}

QString FileManager::createFileName(FileManager::Folder folderType)
{
    getFolderOrReturn(folderType);
    QString path = folder->dir.path();
    QString result = path + '/'
                     + QDateTime::currentDateTime().toString(_fileName)
                     + folder->extension;
    qCDebug(FILEMANAGER) << "Creating file name:" << result;
    return result;
}

FileManager* FileManager::self()
{
    static FileManager self;
    return &self;
}

FileManager::~FileManager() = default;
