/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "socketapi.h"
#include "config.h"
#include "version.h"
#include "libcommon/utility/logiffail.h"
#include "common/utility.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommon/theme/theme.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"

#include <array>

#include <QBitArray>
#include <QUrl>
#include <QMetaMethod>
#include <QMetaObject>
#include <QStringList>
#include <QScopedPointer>
#include <QFile>
#include <QDir>
#include <QApplication>
#include <QLocalSocket>
#include <QStringBuilder>
#include <QMessageBox>
#include <QFileDialog>
#include <QClipboard>
#include <QBuffer>
#include <QDesktopServices>
#include <QStandardPaths>

#if defined(KD_MACOS)
#include <CoreFoundation/CoreFoundation.h>
#endif

#include "utility/utility_base.h"


#include <log4cplus/loggingmacros.h>

// This is the version that is returned when the client asks for the VERSION.
// The first number should be changed if there is an incompatible change that breaks old clients.
// The second number should be changed when there are new features.
#define KDRIVE_SOCKET_API_VERSION "1.1"

#define MSG_CDE_SEPARATOR QChar(L':')
#define MSG_ARG_SEPARATOR QChar(L'\x1e')

#define QUERY_END_SEPARATOR QString("\\/\n")

static QString buildMessage(const QString &verb, const QString &path, const QString &status = QString()) {
    QString msg(verb);

    if (!status.isEmpty()) {
        msg.append(MSG_CDE_SEPARATOR);
        msg.append(status);
    }
    if (!path.isEmpty()) {
        msg.append(MSG_CDE_SEPARATOR);
        const QFileInfo fi(path);
        msg.append(QDir::toNativeSeparators(fi.absoluteFilePath()));
    }
    return msg;
}

namespace KDC {

struct ListenerHasSocketPred {
        QIODevice *socket;

        ListenerHasSocketPred(QIODevice *socket) :
            socket(socket) {}

        bool operator()(const SocketListener &listener) const { return listener.socket == socket; }
};

SocketApi::SocketApi(const std::unordered_map<int, std::shared_ptr<KDC::SyncPal>> &syncPalMap,
                     const std::unordered_map<int, std::shared_ptr<KDC::Vfs>> &vfsMap, QObject *parent) :
    QObject(parent),
    _syncPalMap(syncPalMap),
    _vfsMap(vfsMap) {
    QString socketPath;

    if (OldUtility::isWindows()) {
        socketPath = QString(R"(\\.\pipe\%1-%2)").arg(APPLICATION_SHORTNAME, Utility::userName().c_str());
    } else if (OldUtility::isMac()) {
        socketPath = SOCKETAPI_TEAM_IDENTIFIER_PREFIX APPLICATION_REV_DOMAIN ".socketApi";
#ifdef Q_OS_MAC
        // Tell Finder to use the Extension (checking it from System Preferences -> Extensions)
        QString cmd = QString("pluginkit -v -e use -i " APPLICATION_REV_DOMAIN ".Extension");
        system(cmd.toLocal8Bit());

        // Add it again. This was needed for Mojave to trigger a load.
        // TODO: Still needed?
        CFURLRef url = (CFURLRef) CFAutorelease((CFURLRef) CFBundleCopyBundleURL(CFBundleGetMainBundle()));
        QString appexPath = QUrl::fromCFURL(url).path() + "Contents/PlugIns/Extension.appex/";
        if (std::filesystem::exists(QStr2Path(appexPath))) {
            // Bundled app (i.e. not test executable)
            cmd = QString("pluginkit -v -a ") + appexPath;
            system(cmd.toLocal8Bit());
        }
#endif
    } else if (OldUtility::isLinux() || OldUtility::isBSD()) {
        const QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
        socketPath = runtimeDir + "/" + KDC::Theme::instance()->appName() + "/socket";
    } else {
        LOG_WARN(KDC::Log::instance()->getLogger(), "An unexpected system detected, this probably won't work.");
    }

    SocketApiServer::removeServer(socketPath);
    const QFileInfo info(socketPath);
    if (!info.dir().exists()) {
        bool result = info.dir().mkpath(".");
        LOGW_DEBUG(KDC::Log::instance()->getLogger(), L"Creating " << info.dir().path().toStdWString() << result);
        if (result) {
            QFile::setPermissions(socketPath, QFile::Permissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        }
    }

    if (!_localServer.listen(socketPath)) {
        LOGW_WARN(KDC::Log::instance()->getLogger(), L"Can't start server - " << Utility::formatPath(socketPath));
        _addError(KDC::Error(errId(), KDC::ExitCode::SystemError, KDC::ExitCause::Unknown));
    } else {
        LOGW_INFO(KDC::Log::instance()->getLogger(), L"Server started - " << Utility::formatPath(socketPath));
    }

    connect(&_localServer, &SocketApiServer::newConnection, this, &SocketApi::slotNewConnection);
}

SocketApi::~SocketApi() {
    _localServer.close();
    _listeners.clear();
}

void SocketApi::executeCommandDirect(const QString &commandLineStr) {
    LOGW_DEBUG(KDC::Log::instance()->getLogger(), L"Execute command - cmd=" << commandLineStr.toStdWString());

    QByteArray command = commandLineStr.split(MSG_CDE_SEPARATOR).value(0).toLatin1();
    if (command.compare(QByteArray("MAKE_AVAILABLE_LOCALLY_DIRECT")) == 0 || command.compare(QByteArray("SET_THUMBNAIL")) == 0) {
        // !!! Execute only commands without listeners !!!
        executeCommand(commandLineStr, nullptr);
    } else if (command.compare(QByteArray("STATUS")) == 0) {
        broadcastMessage(commandLineStr);
    }
}

void SocketApi::executeCommand(const QString &commandLine, const SocketListener *listener) {
    bool functionWith1Argument = false;
    bool functionWith2Arguments = false;
    QByteArray command = commandLine.split(MSG_CDE_SEPARATOR).value(0).toLatin1();
    QByteArray functionWithArguments = "command_" + command + "(QString,SocketListener*)";
    int indexOfMethod = staticMetaObject.indexOfMethod(functionWithArguments);
    if (indexOfMethod != -1) {
        functionWith2Arguments = true;
    } else {
        functionWithArguments = "command_" + command + "(QString)";
        indexOfMethod = staticMetaObject.indexOfMethod(functionWithArguments);
        if (indexOfMethod != -1) {
            functionWith1Argument = true;
        }
    }

    QString argument(commandLine);
    argument.remove(0, command.length() + 1);
    if (functionWith2Arguments) {
        staticMetaObject.method(indexOfMethod)
                .invoke(this, Q_ARG(QString, argument), Q_ARG(SocketListener *, const_cast<SocketListener *>(listener)));
    } else if (functionWith1Argument) {
        staticMetaObject.method(indexOfMethod).invoke(this, Q_ARG(QString, argument));
    } else {
        LOGW_WARN(KDC::Log::instance()->getLogger(), L"The command is not supported by this version of the client - cmd="
                                                             << KDC::Utility::s2ws(command.toStdString()) << L" arg="
                                                             << argument.toStdWString());
    }
}

void SocketApi::slotNewConnection() {
    QIODevice *socket = _localServer.nextPendingConnection();
    if (!socket) return;

    LOG_INFO(KDC::Log::instance()->getLogger(), "New connection - socket=" << socket);
    connect(socket, &QIODevice::readyRead, this, &SocketApi::slotReadSocket);
    connect(socket, SIGNAL(disconnected()), this, SLOT(onLostConnection()));
    connect(socket, &QObject::destroyed, this, &SocketApi::slotSocketDestroyed);

    _listeners.append(SocketListener(socket));
    SocketListener &listener = _listeners.last();

    // Load sync list
    std::vector<KDC::Sync> syncList;
    if (!KDC::ParmsDb::instance()->selectAllSyncs(syncList)) {
        LOG_WARN(KDC::Log::instance()->getLogger(), "Error in ParmsDb::selectAllSyncs");
        _addError(KDC::Error(errId(), KDC::ExitCode::DbError, KDC::ExitCause::Unknown));
        return;
    }

    for (const KDC::Sync &sync: syncList) {
        if (!sync.paused()) {
            QString message = buildRegisterPathMessage(SyncName2QStr(sync.localPath().native()));
            listener.sendMessage(message);
        }
    }
}

void SocketApi::onLostConnection() {
    LOG_INFO(KDC::Log::instance()->getLogger(), "Lost connection - sender=" << sender());
    sender()->deleteLater();

    auto socket = qobject_cast<QIODevice *>(sender());
    LOG_IF_FAIL(Log::instance()->getLogger(), socket)
    _listeners.erase(std::remove_if(_listeners.begin(), _listeners.end(), ListenerHasSocketPred(socket)), _listeners.end());
}

void SocketApi::slotSocketDestroyed(QObject *obj) {
    auto *socket = static_cast<QIODevice *>(obj);
    _listeners.erase(std::remove_if(_listeners.begin(), _listeners.end(), ListenerHasSocketPred(socket)), _listeners.end());
}

void SocketApi::slotReadSocket() {
    auto *socket = qobject_cast<QIODevice *>(sender());
    LOG_IF_FAIL(Log::instance()->getLogger(), socket)

    // Find the SocketListener
    //
    // It's possible for the disconnected() signal to be triggered before
    // the readyRead() signals are received - in that case there won't be a
    // valid listener. We execute the handler anyway, but it will work with
    // a SocketListener that doesn't send any messages.
    static auto noListener = SocketListener(nullptr);
    SocketListener *listener = &noListener;
    auto listenerIt = std::find_if(_listeners.begin(), _listeners.end(), ListenerHasSocketPred(socket));
    if (listenerIt != _listeners.end()) {
        listener = &*listenerIt;
    }

    while (socket->canReadLine()) {
        // Make sure to normalize the input from the socket to
        // make sure that the path will match, especially on OS X.
        QString line;
        while (!line.endsWith(QUERY_END_SEPARATOR)) {
            if (!socket->canReadLine()) {
                LOGW_WARN(KDC::Log::instance()->getLogger(),
                          L"Failed to parse SocketAPI message - msg=" << line.toStdWString() << L" socket=" << socket);
                return;
            }
            line.append(socket->readLine());
        }
        line.chop(QUERY_END_SEPARATOR.length()); // remove the separator
        LOGW_INFO(KDC::Log::instance()->getLogger(),
                  L"Received SocketAPI message - msg=" << line.toStdWString() << L" socket=" << socket);
        executeCommand(line, listener);
    }
}

bool SocketApi::tryToRetrieveSync(const int syncDbId, KDC::Sync &sync) const {
    if (!_registeredSyncs.contains(syncDbId)) return false; // Make sure not to register twice to each connected client

    bool found = false;
    if (!KDC::ParmsDb::instance()->selectSync(syncDbId, sync, found)) {
        LOG_WARN(KDC::Log::instance()->getLogger(), "Error in ParmsDb::selectSync - syncDbId=" << syncDbId);
        _addError(KDC::Error(errId(), KDC::ExitCode::DbError, KDC::ExitCause::Unknown));
        return false;
    }
    if (!found) {
        LOG_WARN(KDC::Log::instance()->getLogger(), "Sync not found in sync table - syncDbId=" << syncDbId);
        _addError(KDC::Error(errId(), KDC::ExitCode::DataError, KDC::ExitCause::Unknown));
        return false;
    }

    return true;
}


void SocketApi::registerSync(int syncDbId) {
    KDC::Sync sync;
    if (!tryToRetrieveSync(syncDbId, sync)) return;

    QString message = buildRegisterPathMessage(SyncName2QStr(sync.localPath().native()));
    foreach (auto &listener, _listeners) {
        listener.sendMessage(message);
    }

    _registeredSyncs.insert(syncDbId);
}

void SocketApi::unregisterSync(int syncDbId) {
    Sync sync;
    if (!tryToRetrieveSync(syncDbId, sync)) return;

    broadcastMessage(buildMessage(QString("UNREGISTER_PATH"), SyncName2QStr(sync.localPath().native()), QString()), true);

    _registeredSyncs.remove(syncDbId);
}

void SocketApi::broadcastMessage(const QString &msg, bool doWait) {
    foreach (auto &listener, _listeners) {
        listener.sendMessage(msg, doWait);
    }
}

void SocketApi::command_RETRIEVE_FOLDER_STATUS(const QString &argument, SocketListener *listener) {
    // This command is the same as RETRIEVE_FILE_STATUS
    command_RETRIEVE_FILE_STATUS(argument, listener);
}

void SocketApi::command_RETRIEVE_FILE_STATUS(const QString &argument, SocketListener *listener) {
    const auto fileData = FileData::get(argument);
    if (fileData.syncDbId) {
        // The user probably visited this directory in the file shell.
        // Let the listener know that it should now send status pushes for sibblings of this file.
        QString directory = fileData.localPath.left(fileData.localPath.lastIndexOf('/'));
        listener->registerMonitoredDirectory(static_cast<unsigned int>(qHash(directory)));
    }

    auto status = SyncFileStatus::Unknown;
    VfsStatus vfsStatus;
    if (!syncFileStatus(fileData, status, vfsStatus)) {
        LOGW_DEBUG(KDC::Log::instance()->getLogger(),
                   L"Error in SocketApi::syncFileStatus - " << Utility::formatPath(fileData.localPath));
        return;
    }

    const QString message = buildMessage(QString("STATUS"), fileData.localPath, socketAPIString(status, vfsStatus));
    listener->sendMessage(message);
}

void SocketApi::command_VERSION(const QString &, SocketListener *listener) {
    listener->sendMessage(
            QString("VERSION%1%2%1%3").arg(MSG_CDE_SEPARATOR).arg(KDRIVE_VERSION_STRING, KDRIVE_SOCKET_API_VERSION));
}

void SocketApi::command_COPY_PUBLIC_LINK(const QString &localFile, SocketListener *) {
    const auto fileData = FileData::get(localFile);
    if (!fileData.syncDbId) return;

    const auto syncPalMapIt = retrieveSyncPalMapIt(fileData.syncDbId);
    if (syncPalMapIt == _syncPalMap.end()) return;

    // Get NodeId
    KDC::NodeId nodeId;
    KDC::ExitCode exitCode = syncPalMapIt->second->fileRemoteIdFromLocalPath(QStr2Path(fileData.relativePath), nodeId);
    if (exitCode != KDC::ExitCode::Ok) {
        LOGW_WARN(KDC::Log::instance()->getLogger(),
                  L"Error in SyncPal::itemId - " << Utility::formatPath(fileData.relativePath));
        return;
    }

    // Get public link URL
    QString linkUrl;
    exitCode = _getPublicLinkUrl(fileData.driveDbId, QString::fromStdString(nodeId), linkUrl);
    if (exitCode != KDC::ExitCode::Ok) {
        LOGW_WARN(KDC::Log::instance()->getLogger(),
                  L"Error in getPublicLinkUrl - " << Utility::formatPath(fileData.relativePath));
        return;
    }

    copyUrlToClipboard(linkUrl);
}

// Fetches the private link url asynchronously and then calls the target slot
void SocketApi::fetchPrivateLinkUrlHelper(const QString &localFile, const std::function<void(const QString &url)> &targetFun) {
    // Find the common sync
    KDC::Sync sync;
    if (!syncForPath(QStr2Path(localFile), sync)) {
        LOGW_WARN(KDC::Log::instance()->getLogger(), L"Sync not found - " << Utility::formatPath(localFile));
        return;
    }

    KDC::Drive drive;
    bool found = false;
    if (!KDC::ParmsDb::instance()->selectDrive(sync.driveDbId(), drive, found)) {
        LOG_WARN(KDC::Log::instance()->getLogger(), "Error in ParmsDb::selectDrive");
        return;
    }
    if (!found) {
        LOG_WARN(KDC::Log::instance()->getLogger(), "Drive not found - id=" << sync.driveDbId());
        return;
    }

    // Find the syncpal associated to sync
    std::unordered_map<int, std::shared_ptr<KDC::SyncPal>>::const_iterator syncPalMapIt = _syncPalMap.end();
    if (sync.dbId()) {
        syncPalMapIt = retrieveSyncPalMapIt(sync.dbId());
    }

    if (syncPalMapIt == _syncPalMap.end()) return;

    FileData fileData = FileData::get(localFile);
    KDC::NodeId itemId;
    if (syncPalMapIt->second->fileRemoteIdFromLocalPath(QStr2Path(fileData.relativePath), itemId) != KDC::ExitCode::Ok) {
        LOGW_WARN(KDC::Log::instance()->getLogger(), L"Error in SyncPal::itemId - " << Utility::formatPath(localFile));
        return;
    }

    const QString linkUrl = QString(APPLICATION_PREVIEW_URL).arg(drive.driveId()).arg(itemId.c_str());
    targetFun(linkUrl);
}

bool SocketApi::syncFileStatus(const FileData &fileData, SyncFileStatus &status, VfsStatus &vfsStatus) {
    status = SyncFileStatus::Unknown;
    vfsStatus.isPlaceholder = false;
    vfsStatus.isHydrated = false;

    if (!fileData.syncDbId) return false;

    const auto syncPalMapIt = retrieveSyncPalMapIt(fileData.syncDbId);
    if (syncPalMapIt == _syncPalMap.end()) return false;

    bool exists = false;
    if (!syncPalMapIt->second->checkIfExistsOnServer(QStr2Path(fileData.relativePath), exists)) {
        LOGW_DEBUG(KDC::Log::instance()->getLogger(),
                   L"Error in SyncPal::checkIfExistsOnServer: " << Utility::formatPath(fileData.relativePath));
        // Occurs when the sync is stopped
        return false;
    }

    if (exists) {
        status = SyncFileStatus::Success;
    }

    const auto vfsMapIt = retrieveVfsMapIt(fileData.syncDbId);
    if (vfsMapIt == _vfsMap.end()) return false;


    if (vfsMapIt->second->mode() == KDC::VirtualFileMode::Mac || vfsMapIt->second->mode() == KDC::VirtualFileMode::Win) {
        if (!vfsMapIt->second->status(QStr2Path(fileData.localPath), vfsStatus)) {
            LOGW_WARN(KDC::Log::instance()->getLogger(), L"Error in Vfs::status - " << Utility::formatPath(fileData.localPath));
            return false;
        }

        if (vfsStatus.isSyncing) {
            status = KDC::SyncFileStatus::Syncing;
        }
    }

    return true;
}

std::unordered_map<int, std::shared_ptr<KDC::SyncPal>>::const_iterator SocketApi::retrieveSyncPalMapIt(const int syncDbId) const {
    const auto result = _syncPalMap.find(syncDbId);

    if (result == _syncPalMap.end()) {
        LOG_WARN(KDC::Log::instance()->getLogger(), "SyncPal not found in SyncPalMap - syncDbId=" << syncDbId);
        return _syncPalMap.end();
    }

    return result;
}

std::unordered_map<int, std::shared_ptr<KDC::Vfs>>::const_iterator SocketApi::retrieveVfsMapIt(const int syncDbId) const {
    const auto result = _vfsMap.find(syncDbId);
    if (result == _vfsMap.cend()) {
        LOG_WARN(KDC::Log::instance()->getLogger(), "Vfs not found in VfsMap - syncDbId=" << syncDbId);
        return _vfsMap.cend();
    }

    return result;
}

ExitInfo SocketApi::setPinState(const FileData &fileData, KDC::PinState pinState) {
    if (!fileData.syncDbId) return {ExitCode::LogicError, ExitCause::InvalidArgument};

    const auto vfsMapIt = retrieveVfsMapIt(fileData.syncDbId);
    if (vfsMapIt == _vfsMap.cend()) return {ExitCode::LogicError};

    return vfsMapIt->second->setPinState(QStr2Path(fileData.relativePath), pinState);
}

ExitInfo SocketApi::dehydratePlaceholder(const FileData &fileData) {
    if (!fileData.syncDbId) return {ExitCode::LogicError, ExitCause::InvalidArgument};

    const auto vfsMapIt = retrieveVfsMapIt(fileData.syncDbId);
    if (vfsMapIt == _vfsMap.cend()) return {ExitCode::LogicError};

    return vfsMapIt->second->dehydratePlaceholder(QStr2Path(fileData.relativePath));
}

bool SocketApi::addDownloadJob(const FileData &fileData) {
    if (!fileData.syncDbId) return false;

    const auto syncPalMapIt = retrieveSyncPalMapIt(fileData.syncDbId);
    if (syncPalMapIt == _syncPalMap.end()) return false;

    // Create download job
    const KDC::ExitCode exitCode =
            syncPalMapIt->second->addDlDirectJob(QStr2Path(fileData.relativePath), QStr2Path(fileData.localPath));
    if (exitCode != KDC::ExitCode::Ok) {
        LOGW_WARN(KDC::Log::instance()->getLogger(),
                  L"Error in SyncPal::addDownloadJob - " << Utility::formatPath(fileData.relativePath));
        return false;
    }

    return true;
}

bool SocketApi::cancelDownloadJobs(int syncDbId, const QStringList &fileList) {
    const auto syncPalMapIt = retrieveSyncPalMapIt(syncDbId);
    if (syncPalMapIt == _syncPalMap.end()) return false;

    const auto vfsMapIt = retrieveVfsMapIt(syncDbId);
    if (vfsMapIt == _vfsMap.end()) return false;

    std::list<KDC::SyncPath> syncPathList;
    processFileList(fileList, syncPathList);

    // Cancel download jobs
    const KDC::ExitCode exitCode = syncPalMapIt->second->cancelDlDirectJobs(syncPathList);
    if (exitCode != KDC::ExitCode::Ok) {
        LOG_WARN(KDC::Log::instance()->getLogger(), "Error in SyncPal::cancelDlDirectJobs");
        return false;
    }

    return true;
}

void SocketApi::command_COPY_PRIVATE_LINK(const QString &localFile, SocketListener *) {
    fetchPrivateLinkUrlHelper(localFile, &SocketApi::copyUrlToClipboard);
}

void SocketApi::command_OPEN_PRIVATE_LINK(const QString &localFile, SocketListener *) {
    fetchPrivateLinkUrlHelper(localFile, &SocketApi::openPrivateLink);
}

void SocketApi::copyUrlToClipboard(const QString &link) {
    QApplication::clipboard()->setText(link);
}

void SocketApi::command_MAKE_AVAILABLE_LOCALLY_DIRECT(const QString &filesArg) {
    const QStringList fileList = filesArg.split(MSG_ARG_SEPARATOR);

    QSet<QString> impactedFolders;

#ifdef Q_OS_MAC
    std::list<KDC::SyncPath> fileListExpanded;
    processFileList(fileList, fileListExpanded);

    for (const auto &filePath: qAsConst(fileListExpanded)) {
#else
    for (const auto &str: qAsConst(fileList)) {
        std::filesystem::path filePath = QStr2Path(str);
#endif
        auto fileData = FileData::get(filePath);
        if (!fileData.syncDbId) {
            LOGW_WARN(KDC::Log::instance()->getLogger(), L"No file data - " << Utility::formatSyncPath(filePath));
            continue;
        }

        if (fileData.isLink) {
            LOGW_DEBUG(KDC::Log::instance()->getLogger(), L"Don't hydrate symlinks - " << Utility::formatSyncPath(filePath));
            continue;
        }

        // Check file status
        auto status = SyncFileStatus::Unknown;
        VfsStatus vfsStatus;
        if (!syncFileStatus(fileData, status, vfsStatus)) {
            LOGW_WARN(KDC::Log::instance()->getLogger(),
                      L"Error in SocketApi::syncFileStatus - " << Utility::formatSyncPath(filePath));
            continue;
        }

        if (!vfsStatus.isPlaceholder) {
            // File is not a placeholder, this should never happen
            LOGW_WARN(KDC::Log::instance()->getLogger(), L"File is not a placeholder - " << Utility::formatSyncPath(filePath));
            continue;
        }

        if (vfsStatus.isHydrated || status == KDC::SyncFileStatus::Syncing) {
            LOGW_INFO(KDC::Log::instance()->getLogger(), L"File is already hydrated/ing - " << Utility::formatSyncPath(filePath));
            continue;
        }

#if defined(KD_MACOS)
        // Not done in Windows case: triggers a hydration
        // Set pin state
        if (!setPinState(fileData, KDC::PinState::AlwaysLocal)) {
            LOGW_INFO(KDC::Log::instance()->getLogger(),
                      L"Error in SocketApi::setPinState - " << Utility::formatSyncPath(filePath));
            continue;
        }
#endif

        if (!addDownloadJob(fileData)) {
            LOGW_INFO(KDC::Log::instance()->getLogger(),
                      L"Error in SocketApi::addDownloadJob - " << Utility::formatSyncPath(filePath));
            continue;
        }

        QCoreApplication::processEvents();
    }
}

bool SocketApi::syncForPath(const std::filesystem::path &path, KDC::Sync &sync) {
    std::vector<KDC::Sync> syncList;
    if (!KDC::ParmsDb::instance()->selectAllSyncs(syncList)) {
        LOG_WARN(KDC::Log::instance()->getLogger(), "Error in ParmsDb::selectAllSyncs");
        return false;
    }

    for (const KDC::Sync &tmpSync: syncList) {
        if (KDC::CommonUtility::isSubDir(tmpSync.localPath(), path)) {
            sync = tmpSync;
            return true;
        }
    }

    return false;
}

QString SocketApi::socketAPIString(SyncFileStatus status, const VfsStatus &vfsStatus) const {
    QString statusString;

    switch (status) {
        case SyncFileStatus::Unknown:
            statusString = QLatin1String("NOP");
            break;
        case SyncFileStatus::Syncing:
            statusString = QLatin1String("SYNC_%1").arg(QString::number(vfsStatus.progress));
            break;
        case SyncFileStatus::Conflict:
        case SyncFileStatus::Ignored:
            statusString = QLatin1String("IGNORE");
            break;
        case SyncFileStatus::Success:
        case SyncFileStatus::Inconsistency:
            if ((vfsStatus.isPlaceholder && vfsStatus.isHydrated) || !vfsStatus.isPlaceholder) {
                statusString = QLatin1String("OK");
            } else {
                statusString = QLatin1String("ONLINE");
            }
            break;
        case SyncFileStatus::Error:
            statusString = QLatin1String("ERROR");
            break;
        case SyncFileStatus::EnumEnd:
            assert(false && "Invalid enum value in switch statement.");
    }

    return statusString;
}

void SocketApi::command_MAKE_ONLINE_ONLY_DIRECT(const QString &filesArg, SocketListener *) {
    const QStringList fileList = filesArg.split(MSG_ARG_SEPARATOR);

    _dehydrationMutex.lock();
    _nbOngoingDehydration++;
    _dehydrationMutex.unlock();

#ifdef Q_OS_MAC
    std::list<KDC::SyncPath> fileListExpanded;
    processFileList(fileList, fileListExpanded);

    for (const auto &filePath: qAsConst(fileListExpanded)) {
#else
    for (const auto &str: qAsConst(fileList)) {
        std::filesystem::path filePath = QStr2Path(str);
#endif
        if (_dehydrationCanceled) {
            break;
        }

        const auto fileData = FileData::get(filePath);
        if (!fileData.syncDbId) {
            LOGW_WARN(KDC::Log::instance()->getLogger(), L"No file data - " << Utility::formatSyncPath(filePath));
            continue;
        }

        if (fileData.isLink) {
            LOGW_DEBUG(KDC::Log::instance()->getLogger(), L"Don't dehydrate symlinks - " << Utility::formatSyncPath(filePath));
            continue;
        }

        // Set pin state
        if (ExitInfo exitInfo = setPinState(fileData, KDC::PinState::OnlineOnly); !exitInfo) {
            LOGW_INFO(KDC::Log::instance()->getLogger(),
                      L"Error in SocketApi::setPinState - " << Utility::formatSyncPath(filePath) << L": " << exitInfo);
            continue;
        }

        // Dehydrate placeholder
        if (ExitInfo exitInfo = dehydratePlaceholder(fileData); !exitInfo) {
            LOGW_INFO(KDC::Log::instance()->getLogger(),
                      L"Error in SocketApi::dehydratePlaceholder - " << Utility::formatSyncPath(filePath) << exitInfo);
            continue;
        }

        QCoreApplication::processEvents();
    }

    _dehydrationMutex.lock();
    _nbOngoingDehydration--;
    if (_nbOngoingDehydration == 0) {
        _dehydrationCanceled = false;
    }
    _dehydrationMutex.unlock();
}

void SocketApi::command_CANCEL_DEHYDRATION_DIRECT(const QString &) {
    LOG_INFO(KDC::Log::instance()->getLogger(), "Ongoing files dehydrations canceled");
    _dehydrationCanceled = true;
    return;
}

void SocketApi::command_CANCEL_HYDRATION_DIRECT(const QString &filesArg) {
    LOG_INFO(KDC::Log::instance()->getLogger(), "Ongoing files hydrations canceled");

    const QStringList fileList = filesArg.split(MSG_ARG_SEPARATOR);

    if (fileList.size() == 0) {
        return;
    }

    KDC::Sync sync;
    if (!syncForPath(QStr2Path(fileList[0]), sync)) {
        LOGW_WARN(KDC::Log::instance()->getLogger(), L"Sync not found - " << Utility::formatPath(fileList[0]));
        return;
    }

    if (!cancelDownloadJobs(sync.dbId(), fileList)) {
        LOGW_INFO(KDC::Log::instance()->getLogger(), L"Error in SocketApi::cancelDownloadJobs");
        return;
    }

#ifdef Q_OS_WIN32
    for (const auto &p: qAsConst(fileList)) {
        QString filePath = QDir::cleanPath(p);
        FileData fileData = FileData::get(filePath);

        if (!fileData.syncDbId) {
            continue;
        }

        auto vfsMapIt = retrieveVfsMapIt(fileData.syncDbId);
        if (vfsMapIt == _vfsMap.cend()) continue;

        vfsMapIt->second->cancelHydrate(QStr2Path(filePath));
    }
#endif
}

void SocketApi::openPrivateLink(const QString &link) {
    openBrowser(link);
}

void SocketApi::command_GET_STRINGS(const QString &argument, SocketListener *listener) {
    static std::array<std::pair<const char *, QString>, 2> strings{{
            {"CONTEXT_MENU_TITLE", KDC::Theme::instance()->appNameGUI()},
            {"COPY_PRIVATE_LINK_MENU_TITLE", tr("Copy private share link")},
    }};
    listener->sendMessage(QString("GET_STRINGS%1BEGIN").arg(MSG_CDE_SEPARATOR));
    for (auto &[key, value]: strings) {
        if (argument.isEmpty() || argument == key) {
            listener->sendMessage(QString("STRING%1%2%1%3").arg(MSG_CDE_SEPARATOR).arg(key, value));
        }
    }
    listener->sendMessage(QString("GET_STRINGS%1END").arg(MSG_CDE_SEPARATOR));
}

#ifdef Q_OS_WIN
void SocketApi::command_GET_THUMBNAIL(const QString &argument, SocketListener *listener) {
    QStringList argumentList = argument.split(MSG_ARG_SEPARATOR);

    if (argumentList.size() != 3) {
        LOGW_WARN(KDC::Log::instance()->getLogger(), L"Invalid argument - arg=" << argument.toStdWString());
        return;
    }

    // Msg Id
    uint64_t msgId(argumentList[0].toULongLong());

    // Picture width asked
    unsigned int width(argumentList[1].toInt());
    if (width == 0) {
        LOG_WARN(KDC::Log::instance()->getLogger(), "Bad width - value=" << width);
        return;
    }

    // File path
    QString filePath(argumentList[2]);
    if (!QFileInfo(filePath).isFile()) {
        LOGW_WARN(KDC::Log::instance()->getLogger(), L"Not a file - " << Utility::formatPath(filePath));
        return;
    }

    FileData fileData = FileData::get(filePath);
    if (!fileData.syncDbId) {
        LOGW_WARN(KDC::Log::instance()->getLogger(),
                  L"The file is not in a synchonized folder - " << Utility::formatPath(filePath));
        return;
    }

    auto syncPalMapIt = retrieveSyncPalMapIt(fileData.syncDbId);
    if (syncPalMapIt == _syncPalMap.end()) return;


    // Get NodeId
    KDC::NodeId nodeId;
    KDC::ExitCode exitCode = syncPalMapIt->second->fileRemoteIdFromLocalPath(QStr2WStr(fileData.relativePath), nodeId);
    if (exitCode != KDC::ExitCode::Ok) {
        LOGW_WARN(KDC::Log::instance()->getLogger(), L"Error in SyncPal::itemId - " << Utility::formatPath(filePath));
        return;
    }

    // Get thumbnail
    std::string thumbnail;
    exitCode = _getThumbnail(fileData.driveDbId, nodeId, 256, thumbnail);
    if (exitCode != KDC::ExitCode::Ok) {
        LOGW_WARN(KDC::Log::instance()->getLogger(), L"Error in getThumbnail - " << Utility::formatPath(filePath));
        return;
    }

    QByteArray thumbnailArr(thumbnail.c_str(), thumbnail.length());
    QPixmap pixmap;
    if (!pixmap.loadFromData(thumbnailArr)) {
        LOGW_WARN(KDC::Log::instance()->getLogger(), L"Error in QPixmap::loadFromData - " << Utility::formatPath(filePath));
        return;
    }

    // Resize pixmap
    LOG_DEBUG(KDC::Log::instance()->getLogger(), "Thumbnail fetched - size=" << pixmap.width() << "x" << pixmap.height());
    if (width) {
        if (pixmap.width() > pixmap.height()) {
            pixmap = pixmap.scaledToWidth(width, Qt::SmoothTransformation);
        } else {
            pixmap = pixmap.scaledToHeight(width, Qt::SmoothTransformation);
        }
        LOG_DEBUG(KDC::Log::instance()->getLogger(), "Thumbnail scaled - size=" << pixmap.width() << "x" << pixmap.height());
    }

    // Set Thumbnail
    QBuffer pixmapBuffer;
    pixmapBuffer.open(QIODevice::WriteOnly);
    pixmap.save(&pixmapBuffer, "BMP");

    listener->sendMessage(
            QString("%1%2%3").arg(QString::number(msgId)).arg(MSG_CDE_SEPARATOR).arg(QString(pixmapBuffer.data().toBase64())));
}
#endif

#ifdef Q_OS_MAC

void SocketApi::command_SET_THUMBNAIL(const QString &filePath) {
    if (filePath.isEmpty()) {
        // No thumbnail for root
        return;
    }

    if (!QFileInfo(filePath).isFile()) {
        LOGW_WARN(KDC::Log::instance()->getLogger(), L"Not a file - " << Utility::formatPath(filePath));
        return;
    }

    FileData fileData = FileData::get(filePath);
    if (!fileData.syncDbId) {
        LOGW_WARN(KDC::Log::instance()->getLogger(),
                  L"The file is not in a synchronized folder - " << Utility::formatPath(filePath));
        return;
    }

    // Find SyncPal and Vfs associated to sync
    auto syncPalMapIt = retrieveSyncPalMapIt(fileData.syncDbId);
    if (syncPalMapIt == _syncPalMap.end()) return;

    auto vfsMapIt = retrieveVfsMapIt(fileData.syncDbId);
    if (vfsMapIt == _vfsMap.end()) return;

    // Get NodeId
    KDC::NodeId nodeId;
    KDC::ExitCode exitCode = syncPalMapIt->second->fileRemoteIdFromLocalPath(QStr2Path(fileData.relativePath), nodeId);
    if (exitCode != KDC::ExitCode::Ok) {
        LOGW_WARN(KDC::Log::instance()->getLogger(), L"Error in SyncPal::itemId - " << Utility::formatPath(filePath));
        return;
    }

    // Get thumbnail
    std::string thumbnail;
    exitCode = _getThumbnail(fileData.driveDbId, nodeId, 256, thumbnail);
    if (exitCode != KDC::ExitCode::Ok) {
        LOGW_WARN(KDC::Log::instance()->getLogger(), L"Error in getThumbnail - " << Utility::formatPath(filePath));
        return;
    }

    QByteArray thumbnailArr(thumbnail.c_str(), static_cast<qsizetype>(thumbnail.length()));
    QPixmap pixmap;
    if (!pixmap.loadFromData(thumbnailArr)) {
        LOGW_WARN(KDC::Log::instance()->getLogger(), L"Error in QPixmap::loadFromData - " << Utility::formatPath(filePath));
        return;
    }

    LOG_DEBUG(KDC::Log::instance()->getLogger(), "Thumbnail fetched - size=" << pixmap.width() << "x" << pixmap.height());

    // Set thumbnail
    if (!vfsMapIt->second->setThumbnail(QStr2Path(fileData.localPath), pixmap)) {
        LOGW_WARN(KDC::Log::instance()->getLogger(), L"Error in setThumbnail - " << Utility::formatPath(filePath));
        return;
    }
}

#endif

void SocketApi::sendSharingContextMenuOptions(const FileData &fileData, const SocketListener *listener) {
    auto theme = KDC::Theme::instance();
    if (!(theme->userGroupSharing() || theme->linkSharing())) return;

    // Find SyncPal associated to sync
    auto syncPalMapIt = _syncPalMap.end();
    if (fileData.syncDbId) {
        syncPalMapIt = retrieveSyncPalMapIt(fileData.syncDbId);
    }

    bool isOnTheServer = false;
    if (!syncPalMapIt->second->checkIfExistsOnServer(QStr2Path(fileData.relativePath), isOnTheServer)) {
        LOGW_DEBUG(KDC::Log::instance()->getLogger(),
                   L"Error in SyncPal::checkIfExistsOnServer: " << Utility::formatPath(fileData.relativePath));
        // Occurs when the sync is stopped
        return;
    }

    bool canShare = false;
    if (!syncPalMapIt->second->checkIfCanShareItem(QStr2Path(fileData.relativePath), canShare)) {
        LOGW_DEBUG(KDC::Log::instance()->getLogger(),
                   L"Error in SyncPal::checkIfCanShareItem: " << Utility::formatPath(fileData.relativePath));
        // Occurs when the sync is stopped
        return;
    }

    const auto flagString = QString("%1%2%1").arg(MSG_CDE_SEPARATOR).arg(isOnTheServer ? QString() : QString("d"));

    // If sharing is globally disabled, do not show any sharing entries.
    // If there is no permission to share for this file, add a disabled entry saying so
    if (isOnTheServer && !canShare) {
        listener->sendMessage(QString("MENU_ITEM%1DISABLED%1d%1%2")
                                      .arg(MSG_CDE_SEPARATOR)
                                      .arg(fileData.isDirectory ? tr("Resharing this file is not allowed")
                                                                : tr("Resharing this folder is not allowed")));
    } else {
        // Do we have public links?
        bool publicLinksEnabled = theme->linkSharing();

        // Is is possible to create a public link without user choices?
        bool canCreateDefaultPublicLink = publicLinksEnabled;

        if (canCreateDefaultPublicLink) {
            listener->sendMessage(QString("MENU_ITEM%1COPY_PUBLIC_LINK%2")
                                          .arg(MSG_CDE_SEPARATOR)
                                          .arg(flagString + tr("Copy public share link")));
        } else if (publicLinksEnabled) {
            listener->sendMessage(QString("MENU_ITEM%1MANAGE_PUBLIC_LINKS%2")
                                          .arg(MSG_CDE_SEPARATOR)
                                          .arg(flagString + tr("Copy public share link")));
        }
    }

    listener->sendMessage(
            QString("MENU_ITEM%1COPY_PRIVATE_LINK%2").arg(MSG_CDE_SEPARATOR).arg(flagString + tr("Copy private share link")));
}

void SocketApi::addSharingContextMenuOptions(const FileData &fileData, QTextStream &response) {
    auto theme = KDC::Theme::instance();
    if (!(theme->userGroupSharing() || theme->linkSharing())) return;

    // Find SyncPal associated to sync
    auto syncPalMapIt = _syncPalMap.end();
    if (fileData.syncDbId) {
        syncPalMapIt = retrieveSyncPalMapIt(fileData.syncDbId);
    }

    bool isOnTheServer = false;
    if (!syncPalMapIt->second->checkIfExistsOnServer(QStr2Path(fileData.relativePath), isOnTheServer)) {
        LOGW_DEBUG(KDC::Log::instance()->getLogger(),
                   L"Error in SyncPal::checkIfExistsOnServer: " << Utility::formatPath(fileData.relativePath));
        // Occurs when the sync is stopped
        return;
    }

    bool canShare = false;
    if (!syncPalMapIt->second->checkIfCanShareItem(QStr2Path(fileData.relativePath), canShare)) {
        LOGW_DEBUG(KDC::Log::instance()->getLogger(),
                   L"Error in SyncPal::checkIfCanShareItem: " << Utility::formatPath(fileData.relativePath));
        // Occurs when the sync is stopped
        return;
    }

    const auto flagString = QString("%1%2%1").arg(MSG_CDE_SEPARATOR).arg(isOnTheServer ? QString() : QString("d"));

    // If sharing is globally disabled, do not show any sharing entries.
    // If there is no permission to share for this file, add a disabled entry saying so
    if (isOnTheServer && !canShare) {
        response << QString("%1DISABLED%1d%1%2")
                            .arg(MSG_CDE_SEPARATOR)
                            .arg(fileData.isDirectory ? tr("Resharing this file is not allowed")
                                                      : tr("Resharing this folder is not allowed"));
    } else {
        // Do we have public links?
        bool publicLinksEnabled = theme->linkSharing();

        // Is is possible to create a public link without user choices?
        bool canCreateDefaultPublicLink = publicLinksEnabled;

        if (canCreateDefaultPublicLink) {
            response << QString("%1COPY_PUBLIC_LINK%2").arg(MSG_CDE_SEPARATOR).arg(flagString + tr("Copy public share link"));
        } else if (publicLinksEnabled) {
            response << QString("%1MANAGE_PUBLIC_LINKS%2").arg(MSG_CDE_SEPARATOR).arg(flagString + tr("Copy public share link"));
        }
    }

    response << QString("%1COPY_PRIVATE_LINK%2").arg(MSG_CDE_SEPARATOR).arg(flagString + tr("Copy private share link"));
}

void SocketApi::command_GET_MENU_ITEMS(const QString &argument, SocketListener *listener) {
    listener->sendMessage(QString("GET_MENU_ITEMS%1BEGIN").arg(MSG_CDE_SEPARATOR));
    const QStringList files = argument.split(MSG_ARG_SEPARATOR);

    // Find the common sync
    Sync sync;
    for (const auto &file: qAsConst(files)) {
        Sync tmpSync;
        if (!syncForPath(QStr2Path(file), tmpSync)) {
            return;
        }
        if (tmpSync.dbId() != sync.dbId()) {
            if (!sync.dbId()) {
                sync = tmpSync;
            } else {
                sync.setDbId(0);
                break;
            }
        }
    }

    // Find SyncPal and Vfs associated to sync
    std::unordered_map<int, std::shared_ptr<KDC::SyncPal>>::const_iterator syncPalMapIt = _syncPalMap.end();
    std::unordered_map<int, std::shared_ptr<KDC::Vfs>>::const_iterator vfsMapIt = _vfsMap.end();
    if (sync.dbId()) {
        syncPalMapIt = retrieveSyncPalMapIt(sync.dbId());
        if (syncPalMapIt == _syncPalMap.end()) return;

        vfsMapIt = retrieveVfsMapIt(sync.dbId());
        if (vfsMapIt == _vfsMap.end()) return;
    }

    // Some options only show for single files
    bool isSingleFile = false;
    if (files.size() == 1) {
        manageActionsOnSingleFile(listener, files, syncPalMapIt, vfsMapIt, sync);

        isSingleFile = QFileInfo(files.first()).isFile();
    }

    // Manage dehydration cancellation
    bool canCancelDehydration = false;
    _dehydrationMutex.lock();
    if (_nbOngoingDehydration > 0) {
        canCancelDehydration = true;
    }
    _dehydrationMutex.unlock();

    // File availability actions
    if (sync.dbId() && sync.virtualFileMode() != VirtualFileMode::Off && vfsMapIt->second->socketApiPinStateActionsShown()) {
        LOG_IF_FAIL(Log::instance()->getLogger(), !files.isEmpty());

        bool canHydrate = true;
        bool canDehydrate = true;
        bool canCancelHydration = false;
        for (const auto &file: qAsConst(files)) {
            VfsStatus vfsStatus;
            if (!canCancelHydration && vfsMapIt->second->status(QStr2Path(file), vfsStatus) && vfsStatus.isSyncing) {
                canCancelHydration = syncPalMapIt->second->isDownloadOngoing(QStr2Path(file));
            }

            if (isSingleFile) {
                canHydrate = vfsStatus.isPlaceholder && !vfsStatus.isSyncing && !vfsStatus.isHydrated;
                canDehydrate = vfsStatus.isPlaceholder && !vfsStatus.isSyncing && vfsStatus.isHydrated;
            }
        }

        // TODO: Should be a submenu, should use icons
        auto makePinContextMenu = [&](bool makeAvailableLocally, bool freeSpace, bool cancelDehydration, bool cancelHydration) {
            listener->sendMessage(QString("MENU_ITEM%1MAKE_AVAILABLE_LOCALLY_DIRECT%1%2%1%3")
                                          .arg(MSG_CDE_SEPARATOR)
                                          .arg(makeAvailableLocally ? QString() : QString("d"))
                                          .arg(vfsPinActionText()));
            if (cancelHydration) {
                listener->sendMessage(QString("MENU_ITEM%1CANCEL_HYDRATION_DIRECT%1%2%1%3")
                                              .arg(MSG_CDE_SEPARATOR)
                                              .arg(QString(""))
                                              .arg(cancelHydrationText()));
            }

            listener->sendMessage(QString("MENU_ITEM%1MAKE_ONLINE_ONLY_DIRECT%1%2%1%3")
                                          .arg(MSG_CDE_SEPARATOR)
                                          .arg(freeSpace ? QString() : QString("d"))
                                          .arg(vfsFreeSpaceActionText()));
            if (cancelDehydration) {
                listener->sendMessage(QString("MENU_ITEM%1CANCEL_DEHYDRATION_DIRECT%1%2%1%3")
                                              .arg(MSG_CDE_SEPARATOR)
                                              .arg(QString(""))
                                              .arg(cancelDehydrationText()));
            }
        };

        makePinContextMenu(canHydrate, canDehydrate, canCancelDehydration, canCancelHydration);
    }

    listener->sendMessage(QString("GET_MENU_ITEMS%1END").arg(MSG_CDE_SEPARATOR));
}

void SocketApi::manageActionsOnSingleFile(SocketListener *listener, const QStringList &files,
                                          std::unordered_map<int, std::shared_ptr<KDC::SyncPal>>::const_iterator syncPalMapIt,
                                          std::unordered_map<int, std::shared_ptr<KDC::Vfs>>::const_iterator vfsMapIt,
                                          const KDC::Sync &sync) {
    bool exists = false;
    IoError ioError = IoError::Success;
    if (!KDC::IoHelper::checkIfPathExists(QStr2Path(files.first()), exists, ioError) || !exists) {
        return;
    }

    FileData fileData = FileData::get(files.first());
    if (fileData.localPath.isEmpty()) {
        return;
    }
    bool isExcluded = vfsMapIt->second->isExcluded(QStr2Path(fileData.localPath));
    if (isExcluded) {
        return;
    }

    KDC::NodeId nodeId;
    KDC::ExitCode exitCode = syncPalMapIt->second->fileRemoteIdFromLocalPath(QStr2Path(fileData.relativePath), nodeId);
    if (exitCode != KDC::ExitCode::Ok) {
        LOGW_WARN(KDC::Log::instance()->getLogger(),
                  L"Error in SyncPal::itemId - " << Utility::formatPath(fileData.relativePath));
        return;
    }
    bool isOnTheServer = !nodeId.empty();
    auto flagString = QString("%1%2%1").arg(MSG_CDE_SEPARATOR).arg(isOnTheServer ? QString() : QString("d"));

    if (sync.dbId()) {
        listener->sendMessage(QString("VFS_MODE%1%2").arg(MSG_CDE_SEPARATOR).arg(KDC::Vfs::modeToString(sync.virtualFileMode())));
    }

    if (sync.dbId()) {
        sendSharingContextMenuOptions(fileData, listener);
        listener->sendMessage(
                QString("MENU_ITEM%1OPEN_PRIVATE_LINK%2").arg(MSG_CDE_SEPARATOR).arg(flagString + tr("Open in browser")));
    }
}

#ifdef Q_OS_WIN
void SocketApi::command_GET_ALL_MENU_ITEMS(const QString &argument, SocketListener *listener) {
    QStringList argumentList = argument.split(MSG_ARG_SEPARATOR);

    QString msgId = argumentList[0];
    argumentList.removeFirst();

    QString responseStr;
    QTextStream response(&responseStr);
    response << msgId << MSG_CDE_SEPARATOR << KDC::Theme::instance()->appNameGUI();

    // Find the common sync
    KDC::Sync sync;
    for (const auto &file: qAsConst(argumentList)) {
        KDC::Sync tmpSync;
        if (!syncForPath(QStr2Path(file), tmpSync)) {
            listener->sendMessage(responseStr);
            return;
        }

        if (tmpSync.dbId() != sync.dbId()) {
            if (!sync.dbId()) {
                sync = tmpSync;
            } else {
                sync.setDbId(0);
                break;
            }
        }
    }

    std::unordered_map<int, std::shared_ptr<KDC::SyncPal>>::const_iterator syncPalMapIt;
    std::unordered_map<int, std::shared_ptr<KDC::Vfs>>::const_iterator vfsMapIt;
    if (sync.dbId()) {
        syncPalMapIt = retrieveSyncPalMapIt(sync.dbId());
        if (syncPalMapIt == _syncPalMap.end()) {
            listener->sendMessage(responseStr);
            return;
        }

        vfsMapIt = retrieveVfsMapIt(sync.dbId());
        if (vfsMapIt == _vfsMap.end()) {
            listener->sendMessage(responseStr);
            return;
        }
    }

    response << MSG_CDE_SEPARATOR << (sync.dbId() ? KDC::Vfs::modeToString(vfsMapIt->second->mode()) : QString());

    // Some options only show for single files
    if (argumentList.size() == 1) {
        FileData fileData = FileData::get(argumentList.first());
        KDC::NodeId nodeId;
        KDC::ExitCode exitCode = syncPalMapIt->second->fileRemoteIdFromLocalPath(QStr2Path(fileData.relativePath), nodeId);
        if (exitCode != KDC::ExitCode::Ok) {
            LOGW_WARN(KDC::Log::instance()->getLogger(),
                      L"Error in SyncPal::itemId - " << Utility::formatPath(fileData.relativePath));
            listener->sendMessage(responseStr);
            return;
        }
        bool isOnTheServer = !nodeId.empty();
        auto flagString = QString("%1%2%1").arg(MSG_CDE_SEPARATOR).arg(isOnTheServer ? QString() : QString("d"));

        if (sync.dbId()) {
            addSharingContextMenuOptions(fileData, response);
            response << QString("%1OPEN_PRIVATE_LINK%2").arg(MSG_CDE_SEPARATOR).arg(flagString + tr("Open in browser"));
        }
    }

    bool canCancelHydration = false;
    bool canCancelDehydration = false;

    // File availability actions
    if (sync.dbId() && sync.virtualFileMode() != KDC::VirtualFileMode::Off) {
        LOG_IF_FAIL(Log::instance()->getLogger(), !argumentList.isEmpty());

        for (const auto &file: qAsConst(argumentList)) {
            auto fileData = FileData::get(file);
            if (syncPalMapIt->second->isDownloadOngoing(QStr2Path(fileData.relativePath))) {
                canCancelHydration = true;
                break;
            }
        }
    }

    if (canCancelDehydration) {
        response << QString("%1CANCEL_DEHYDRATION_DIRECT%1%1%2").arg(MSG_CDE_SEPARATOR, cancelDehydrationText());
    }

    if (canCancelHydration) {
        response << QString("%1CANCEL_HYDRATION_DIRECT%1%1%2").arg(MSG_CDE_SEPARATOR, cancelHydrationText());
    }

    listener->sendMessage(responseStr);
}
#endif

QString SocketApi::buildRegisterPathMessage(const QString &path) {
    QFileInfo fi(path);
    QString message = QString("REGISTER_PATH%1").arg(MSG_CDE_SEPARATOR);
    message.append(QDir::toNativeSeparators(fi.absoluteFilePath()));
    return message;
}

void SocketApi::processFileList(const QStringList &inFileList, std::list<SyncPath> &outFileList) {
    // Process all files
    for (const QString &path: qAsConst(inFileList)) {
        FileData fileData = FileData::get(path);
        if (fileData.virtualFileMode == VirtualFileMode::Mac) {
            QFileInfo info(path);
            if (info.isDir()) {
                const QFileInfoList infoList = QDir(path).entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
                QStringList fileList;
                for (const auto &tmpInfo: qAsConst(infoList)) {
                    QString tmpPath(tmpInfo.filePath());
                    FileData tmpFileData = FileData::get(tmpPath);

                    auto status = SyncFileStatus::Unknown;
                    if (VfsStatus vfsStatus; !syncFileStatus(tmpFileData, status, vfsStatus)) {
                        LOGW_WARN(KDC::Log::instance()->getLogger(),
                                  L"Error in SocketApi::syncFileStatus - " << Utility::formatPath(tmpPath));
                        continue;
                    }

                    if (status == SyncFileStatus::Unknown || status == SyncFileStatus::Ignored) {
                        continue;
                    }

                    fileList.append(tmpPath);
                }

                if (fileList.size() > 0) {
                    processFileList(fileList, outFileList);
                }
            } else {
                outFileList.push_back(QStr2Path(path));
            }
        } else {
            outFileList.push_back(QStr2Path(path));
        }
    }
}

QString SocketApi::vfsPinActionText() {
    return QCoreApplication::translate("utility", "Make available locally");
}

QString SocketApi::vfsFreeSpaceActionText() {
    return QCoreApplication::translate("utility", "Free up local space");
}

QString SocketApi::cancelDehydrationText() {
    return QCoreApplication::translate("utility", "Cancel free up local space");
}

QString SocketApi::cancelHydrationText() {
#ifdef Q_OS_MAC
    return QCoreApplication::translate("utility", "Cancel make available locally");
#else
    return QCoreApplication::translate("utility", "Cancel make available locally");
#endif
}

bool SocketApi::openBrowser(const QUrl &url) {
    if (!QDesktopServices::openUrl(url)) {
        LOGW_WARN(KDC::Log::instance()->getLogger(), L"QDesktopServices::openUrl failed - url=" << url.toString().toStdWString());
        return false;
    }
    return true;
}


FileData FileData::get(const QString &path) {
    std::filesystem::path localPath = QStr2Path(path);
    return get(localPath);
}

FileData FileData::get(const KDC::SyncPath &path) {
#if defined(KD_WINDOWS)
    KDC::SyncPath tmpPath;
    bool notFound = false;
    if (!KDC::Utility::longPath(path, tmpPath, notFound)) {
        if (notFound) {
            LOGW_WARN(KDC::Log::instance()->getLogger(), L"File not found - " << Utility::formatSyncPath(path));
        } else {
            LOGW_WARN(KDC::Log::instance()->getLogger(), L"Error in Utility::longpath - " << Utility::formatSyncPath(path));
        }
        return FileData();
    }
#else
    KDC::SyncPath tmpPath(path);
#endif

    KDC::Sync sync;
    if (!SocketApi::syncForPath(tmpPath, sync)) {
        LOGW_WARN(KDC::Log::instance()->getLogger(), L"Sync not found - " << Utility::formatSyncPath(tmpPath));
        return FileData();
    }

    FileData data;
    data.syncDbId = sync.dbId();
    data.driveDbId = sync.driveDbId();
    data.virtualFileMode = sync.virtualFileMode();
    data.localPath = SyncName2QStr(tmpPath.native());
    data.relativePath = SyncName2QStr(KDC::CommonUtility::relativePath(sync.localPath(), tmpPath).native());

    ItemType itemType;
    if (!KDC::IoHelper::getItemType(tmpPath, itemType)) {
        LOGW_WARN(KDC::Log::instance()->getLogger(),
                  L"Error in Utility::getItemType: " << Utility::formatIoError(tmpPath, itemType.ioError));
        return FileData();
    }

    if (itemType.ioError == IoError::NoSuchFileOrDirectory) {
        LOGW_DEBUG(KDC::Log::instance()->getLogger(), L"Item does not exist anymore - " << Utility::formatSyncPath(tmpPath));
        return FileData();
    }

    data.isLink = itemType.linkType != KDC::LinkType::None;
    if (data.isLink) {
        data.isDirectory = false;
    } else {
        std::error_code ec;
        data.isDirectory = std::filesystem::is_directory(tmpPath, ec);
        if (!data.isDirectory && ec.value() != 0) {
            const bool exists = !utility_base::isLikeFileNotFoundError(ec);
            if (!exists) {
                // Item doesn't exist anymore
                LOGW_DEBUG(KDC::Log::instance()->getLogger(), L"Item doesn't exist - " << Utility::formatPath(data.localPath));
            } else {
                LOGW_WARN(KDC::Log::instance()->getLogger(), L"Failed to check if the path is a directory - "
                                                                     << Utility::formatPath(data.localPath) << L" err="
                                                                     << KDC::Utility::s2ws(ec.message()) << L" (" << ec.value()
                                                                     << L")");
            }
            return FileData();
        }
    }

    return data;
}

FileData FileData::parentFolder() const {
    return FileData::get(QFileInfo(localPath).dir().path().toUtf8());
}

} // namespace KDC

#include "socketapi.moc"
