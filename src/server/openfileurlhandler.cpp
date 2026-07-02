/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "openfileurlhandler.h"
#include "appserver.h"
#include "libcommon/theme/theme.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"
#include "libparms/db/parmsdb.h"

#include <QDesktopServices>
#include <QSet>
#include <QUrlQuery>

#include <log4cplus/loggingmacros.h>

namespace KDC {

namespace {
constexpr std::chrono::milliseconds resolveTickInterval(1000);
constexpr std::chrono::milliseconds waitForSyncTickInterval(2000);
constexpr std::chrono::milliseconds waitForHydrationTickInterval(500);

// The syncs may not be started yet (e.g. when the URL is received at application startup): keep looking for the file
// during `resolveTimeout` before giving up.
constexpr std::chrono::milliseconds resolveTimeout(std::chrono::seconds(30));
constexpr std::chrono::milliseconds waitForSyncTimeout(std::chrono::minutes(10));
constexpr std::chrono::milliseconds waitForHydrationTimeout(std::chrono::hours(1));

// A download job can transiently be neither ongoing nor completed while its completion callback runs: require
// several consecutive negative checks before considering that the hydration has failed.
constexpr int downloadNotOngoingCountMax = 2;
} // namespace

OpenFileUrlHandler::OpenFileUrlHandler(AppServer *appServer, const QUrl &url, QObject *parent) :
    QObject(parent),
    _appServer(appServer),
    _url(url) {
    (void) connect(&_tickTimer, &QTimer::timeout, this, &OpenFileUrlHandler::onTick);
}

void OpenFileUrlHandler::start() {
    LOG_INFO(Log::instance()->getLogger(), "Processing file opening URL - url=" << _url.toString().toStdString());

    if (!parseUrl(_url, _request)) {
        LOG_WARN(Log::instance()->getLogger(), "Invalid file opening URL - url=" << _url.toString().toStdString());
        fail(tr("This kDrive link is invalid."));
        return;
    }

    setPhase(Phase::Resolve, resolveTickInterval);
    processResolvePhase();
}

bool OpenFileUrlHandler::isOpenFileUrl(const QUrl &url) {
    return url.isValid() && url.scheme() == "kdrive" && url.host() == "open";
}

bool OpenFileUrlHandler::parseUrl(const QUrl &url, Request &request) {
    if (!isOpenFileUrl(url)) return false;

    QString pathStr = url.path(QUrl::FullyDecoded);
    while (pathStr.startsWith('/')) {
        pathStr.remove(0, 1);
    }
    if (pathStr.isEmpty()) return false;

    request.relativePath = QStr2Path(pathStr);
    if (!isRelativePathSafe(request.relativePath)) return false;

    const QUrlQuery query(url);
    const QString driveIdStr = query.queryItemValue("driveId");
    request.hasDriveId = !driveIdStr.isEmpty();
    if (request.hasDriveId) {
        bool ok = false;
        request.driveId = driveIdStr.toLongLong(&ok);
        if (!ok || request.driveId <= 0) return false;
    }

    return true;
}

bool OpenFileUrlHandler::isRelativePathSafe(const SyncPath &relativePath) {
    if (relativePath.empty() || relativePath.has_root_name() || relativePath.is_absolute()) return false;

    for (const auto &component: relativePath) {
        if (component == ".." || component == ".") return false;
    }

    return true;
}

bool OpenFileUrlHandler::shouldOpenParentFolder(const SyncPath &path) {
    static const QSet<QString> executableExtensions = {".exe", ".bat", ".cmd", ".com", ".scr", ".msi", ".ps1",
                                                       ".vbs", ".js",  ".jar", ".sh",  ".app", ".apk"};
    return executableExtensions.contains(Path2QStr(path.extension()).toLower());
}

void OpenFileUrlHandler::onTick() {
    switch (_phase) {
        case Phase::Resolve:
            processResolvePhase();
            break;
        case Phase::WaitForSync:
            processWaitForSyncPhase();
            break;
        case Phase::WaitForHydration:
            processWaitForHydrationPhase();
            break;
    }
}

bool OpenFileUrlHandler::candidateSyncs(std::vector<Sync> &syncs) {
    std::vector<Sync> syncList;
    if (!ParmsDb::instance()->selectAllSyncs(syncList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllSyncs");
        return false;
    }

    for (const auto &sync: syncList) {
        if (_request.hasDriveId) {
            Drive drive;
            bool found = false;
            if (!ParmsDb::instance()->selectDrive(sync.driveDbId(), drive, found) || !found) {
                LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectDrive - driveDbId=" << sync.driveDbId());
                continue;
            }
            if (drive.driveId() != _request.driveId) continue;
        }
        syncs.push_back(sync);
    }

    return true;
}

void OpenFileUrlHandler::processResolvePhase() {
    std::vector<Sync> syncs;
    if (!candidateSyncs(syncs)) {
        fail(tr("The file requested from a kDrive link could not be opened."));
        return;
    }

    // The file is already present locally (possibly as a dehydrated placeholder).
    for (const auto &sync: syncs) {
        const SyncPath localPath = sync.localPath() / _request.relativePath;
        bool exists = false;
        auto ioError = IoError::Success;
        if (!IoHelper::checkIfPathExists(localPath, exists, ioError, IoHelper::PathCheckOption::Insensitive)) {
            LOGW_WARN(Log::instance()->getLogger(),
                      L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(localPath, ioError));
            continue;
        }
        if (exists) {
            _syncDbId = sync.dbId();
            _localPath = localPath;
            hydrateOrOpen();
            return;
        }
    }

    // The file is not present locally: check whether it is already known on the remote side.
    {
        const std::scoped_lock lock(_appServer->syncPalMapMutex);
        for (const auto &sync: syncs) {
            const auto syncPalMapIt = _appServer->syncPalMap.find(sync.dbId());
            if (syncPalMapIt == _appServer->syncPalMap.end() || !syncPalMapIt->second) continue;

            bool exists = false;
            if (!syncPalMapIt->second->checkIfExistsOnServer(_request.relativePath, exists)) continue;
            if (exists) {
                _syncDbId = sync.dbId();
                _localPath = sync.localPath() / _request.relativePath;
                if (!_waitingNotificationSent) {
                    _waitingNotificationSent = true;
                    notifyUser(tr("The requested file is being synchronized. It will open once available."));
                }
                setPhase(Phase::WaitForSync, waitForSyncTickInterval);
                return;
            }
        }
    }

    if (_phaseTimer.hasExpired(resolveTimeout.count())) {
        fail(tr("The file requested from a kDrive link could not be found in your kDrive."));
    }
}

void OpenFileUrlHandler::processWaitForSyncPhase() {
    bool exists = false;
    auto ioError = IoError::Success;
    if (IoHelper::checkIfPathExists(_localPath, exists, ioError, IoHelper::PathCheckOption::Insensitive) && exists) {
        hydrateOrOpen();
        return;
    }

    if (_phaseTimer.hasExpired(waitForSyncTimeout.count())) {
        fail(tr("The file requested from a kDrive link could not be synchronized. Please try again later."));
    }
}

void OpenFileUrlHandler::hydrateOrOpen() {
    _tickTimer.stop();

    ItemType itemType;
    if (!IoHelper::getItemType(_localPath, itemType)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in IoHelper::getItemType: " << Utility::formatIoError(_localPath, itemType.ioError));
        fail(tr("The file requested from a kDrive link could not be opened."));
        return;
    }

    std::error_code ec;
    const bool isDirectory = itemType.linkType == LinkType::None && std::filesystem::is_directory(_localPath, ec);
    if (itemType.linkType != LinkType::None || isDirectory) {
        // Symlinks and directories do not need any hydration.
        openResolvedItem();
        return;
    }

    std::shared_ptr<Vfs> vfs;
    if (const auto exitInfo = _appServer->getVfs(_syncDbId, vfs); !exitInfo || !vfs) {
        // No VFS plugin for this sync (e.g. LiteSync disabled): the file is a plain file, open it directly.
        openResolvedItem();
        return;
    }

    VfsStatus vfsStatus;
    if (const auto exitInfo = vfs->status(_localPath, vfsStatus); !exitInfo) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in Vfs::status - " << Utility::formatSyncPath(_localPath));
        fail(tr("The file requested from a kDrive link could not be opened."));
        return;
    }

    if (!vfsStatus.isPlaceholder || vfsStatus.isHydrated) {
        openResolvedItem();
        return;
    }

    if (!vfsStatus.isSyncing && !triggerHydration()) {
        fail(tr("The file requested from a kDrive link could not be downloaded."));
        return;
    }

    if (!_waitingNotificationSent) {
        _waitingNotificationSent = true;
        notifyUser(tr("The requested file is being downloaded. It will open once available."));
    }
    setPhase(Phase::WaitForHydration, waitForHydrationTickInterval);
}

bool OpenFileUrlHandler::triggerHydration() {
    std::shared_ptr<Vfs> vfs;
    if (const auto exitInfo = _appServer->getVfs(_syncDbId, vfs); !exitInfo || !vfs) return false;

#if defined(KD_MACOS)
    // On macOS, setting the pin state to AlwaysLocal is required to trigger the hydration
    // (see ExtensionJob::commandMakeAvailableLocallyDirect).
    _initialPinState = vfs->pinState(_request.relativePath);
    if (_initialPinState != PinState::AlwaysLocal) {
        if (const auto exitInfo = vfs->setPinState(_request.relativePath, PinState::AlwaysLocal); !exitInfo) {
            LOGW_WARN(Log::instance()->getLogger(), L"Error in Vfs::setPinState - " << Utility::formatSyncPath(_localPath));
            return false;
        }
    }
#endif

    bool jobAdded = false;
    {
        const std::scoped_lock lock(_appServer->syncPalMapMutex);
        const auto syncPalMapIt = _appServer->syncPalMap.find(_syncDbId);
        jobAdded = syncPalMapIt != _appServer->syncPalMap.end() && syncPalMapIt->second &&
                   syncPalMapIt->second->addDlDirectJob(_request.relativePath, _localPath, SyncPath()) == ExitCode::Ok;
    }

    if (!jobAdded) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in SyncPal::addDlDirectJob - " << Utility::formatSyncPath(_localPath));
#if defined(KD_MACOS)
        // Cancel the hydration and reset the pin state to its initial value.
        vfs->cancelHydrate(_localPath);
        if (_initialPinState != PinState::Unknown) (void) vfs->setPinState(_request.relativePath, _initialPinState);
#endif
        return false;
    }

    return true;
}

void OpenFileUrlHandler::processWaitForHydrationPhase() {
    std::shared_ptr<Vfs> vfs;
    if (const auto exitInfo = _appServer->getVfs(_syncDbId, vfs); !exitInfo || !vfs) {
        fail(tr("The file requested from a kDrive link could not be opened."));
        return;
    }

    VfsStatus vfsStatus;
    if (const auto exitInfo = vfs->status(_localPath, vfsStatus); !exitInfo) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in Vfs::status - " << Utility::formatSyncPath(_localPath));
        fail(tr("The file requested from a kDrive link could not be opened."));
        return;
    }

    if (vfsStatus.isHydrated) {
        openResolvedItem();
        return;
    }

    bool downloadOngoing = vfsStatus.isSyncing;
    if (!downloadOngoing) {
        const std::scoped_lock lock(_appServer->syncPalMapMutex);
        const auto syncPalMapIt = _appServer->syncPalMap.find(_syncDbId);
        downloadOngoing = syncPalMapIt != _appServer->syncPalMap.end() && syncPalMapIt->second &&
                          syncPalMapIt->second->isDownloadOngoing(_localPath);
    }

    if (!downloadOngoing) {
        if (++_downloadNotOngoingCount >= downloadNotOngoingCountMax) {
            fail(tr("The download of the file requested from a kDrive link failed."));
            return;
        }
    } else {
        _downloadNotOngoingCount = 0;
    }

    if (_phaseTimer.hasExpired(waitForHydrationTimeout.count())) {
        fail(tr("The download of the file requested from a kDrive link took too long."));
    }
}

void OpenFileUrlHandler::openResolvedItem() {
    _tickTimer.stop();

    SyncPath pathToOpen = _localPath;
    if (shouldOpenParentFolder(_localPath)) {
        LOGW_INFO(Log::instance()->getLogger(),
                  L"Executable item, revealing its parent folder instead - " << Utility::formatSyncPath(_localPath));
        pathToOpen = _localPath.parent_path();
    }

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(Path2QStr(pathToOpen)))) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in QDesktopServices::openUrl - " << Utility::formatSyncPath(pathToOpen));
        fail(tr("The file requested from a kDrive link could not be opened."));
        return;
    }

    LOGW_INFO(Log::instance()->getLogger(), L"File opened from URL - " << Utility::formatSyncPath(pathToOpen));
    succeed();
}

void OpenFileUrlHandler::setPhase(Phase phase, std::chrono::milliseconds tickInterval) {
    _phase = phase;
    _phaseTimer.restart();
    _tickTimer.start(tickInterval);
}

void OpenFileUrlHandler::notifyUser(const QString &message) const {
    _appServer->sendShowNotification(QString::fromStdString(Theme::instance()->appName()), message);
}

void OpenFileUrlHandler::succeed() {
    _tickTimer.stop();
    emit finished(true);
}

void OpenFileUrlHandler::fail(const QString &userMessage) {
    _tickTimer.stop();
    LOG_WARN(Log::instance()->getLogger(), "Failed to open file from URL - url=" << _url.toString().toStdString());
    notifyUser(userMessage);
    emit finished(false);
}

} // namespace KDC
