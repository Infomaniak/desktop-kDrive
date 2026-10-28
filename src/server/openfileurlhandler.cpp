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
#include "libsyncengine/db/syncdb.h"
#include "libsyncengine/jobs/network/kDrive_API/itemsexistjob.h"
#include "libsyncengine/jobs/syncjobmanager.h"
#include "libsyncengine/syncpal/syncpal.h"

#include <QDesktopServices>
#include <QSet>

#include <Poco/Exception.h>
#include <Poco/URI.h>

#include <log4cplus/loggingmacros.h>

#include <algorithm>
#include <charconv>
#include <vector>

namespace KDC {

namespace {
// The syncs may not be started yet (e.g. when the URL is received at application startup) and the requested item may
// have been created on the remote side recently: keep looking for it during `syncWaitTimeout` before giving up.
constexpr std::chrono::milliseconds syncWaitTickInterval(2000);
constexpr std::chrono::milliseconds syncWaitTimeout(std::chrono::minutes(10));
constexpr std::chrono::milliseconds hydrationTimeout(std::chrono::hours(1));

//! Parses `urlStr` into `uri`. Returns false if `urlStr` is not a syntactically valid URI.
bool parseUri(const std::string &urlStr, Poco::URI &uri) {
    try {
        uri = Poco::URI(urlStr);
        return true;
    } catch (const Poco::URISyntaxException &) {
        return false;
    }
}

//! Returns true if `uri` is a file opening URI, i.e. "kdrive://open/...". Poco::URI stores the scheme and the host
//! in lowercase, making the comparisons case-insensitive.
bool isOpenFileUri(const Poco::URI &uri) {
    return uri.getScheme() == "kdrive" && uri.getHost() == "open";
}
} // namespace

OpenFileUrlHandler::OpenFileUrlHandler(AppServer *appServer, const std::string &url, QObject *parent) :
    QObject(parent),
    _appServer(appServer),
    _url(url) {
    _deadlineTimer.setSingleShot(true);
    (void) connect(&_syncWaitTimer, &QTimer::timeout, this, &OpenFileUrlHandler::onSyncWaitTick);
    (void) connect(&_deadlineTimer, &QTimer::timeout, this, &OpenFileUrlHandler::onDeadlineReached);
}

void OpenFileUrlHandler::start() {
    LOG_INFO(Log::instance()->getLogger(), "Processing file opening URL - url=" << _url);

    if (!parseUrl(_url, _request)) {
        LOG_WARN(Log::instance()->getLogger(), "Invalid file opening URL - url=" << _url);
        fail(tr("This kDrive link is invalid."));
        return;
    }

    setPhase(Phase::Resolve, std::chrono::milliseconds(0), std::chrono::milliseconds(0));
    processResolvePhase();
}

bool OpenFileUrlHandler::isOpenFileUrl(const std::string &urlStr) {
    Poco::URI uri;
    return parseUri(urlStr, uri) && isOpenFileUri(uri);
}

bool OpenFileUrlHandler::parseUrl(const std::string &urlStr, Request &request) {
    Poco::URI uri;
    if (!parseUri(urlStr, uri) || !isOpenFileUri(uri)) return false;

    // Poco::URI decodes the path and excludes the query and the fragment while parsing, and getPathSegments drops the
    // empty segments produced by the leading, doubled, and trailing slashes.
    std::vector<std::string> segments;
    uri.getPathSegments(segments);
    if (segments.size() != 4 || segments[0] != "drive" || segments[2] != "file") return false;

    // std::from_chars parses the whole string strictly (no whitespace, no leading '+', no trailing garbage) and never
    // throws.
    const std::string &driveIdStr = segments[1];
    DriveId driveId = 0;

    const auto result = std::from_chars(driveIdStr.data(), driveIdStr.data() + driveIdStr.size(), driveId);
    if (result.ec || result.ptr != driveIdStr.data() + driveIdStr.size()) return false;
    if (driveId <= 0) return false;

    request.driveId = driveId;
    request.nodeId = segments[3];

    return !request.nodeId.empty();
}

bool OpenFileUrlHandler::shouldOpenParentFolder(const SyncPath &path) {
    static const QSet<QString> executableExtensions = {".exe", ".bat", ".cmd", ".com", ".scr", ".msi", ".ps1",
                                                       ".vbs", ".js",  ".jar", ".sh",  ".app", ".apk"};
    return executableExtensions.contains(Path2QStr(path.extension()).toLower());
}

void OpenFileUrlHandler::processResolvePhase() {
    // The drive lookup is performed once: the drive configuration cannot appear while the request is being processed.
    if (_driveDbId == 0) {
        Drive drive;
        bool found = false;
        if (!ParmsDb::instance()->selectDriveByDriveId(_request.driveId, drive, found)) {
            LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectDriveByDriveId - driveId=" << _request.driveId);
            fail(tr("The element requested from this kDrive link could not be opened."));
            return;
        }
        if (!found) {
            // No sync of this drive is configured on this machine.
            fail(tr("The drive of the element requested from this kDrive link is not synchronized on this machine."));
            return;
        }
        _driveDbId = drive.dbId();
    }

    bool found = false;
    if (!resolveNode(found)) {
        fail(tr("The element requested from this kDrive link could not be opened."));
        return;
    }
    if (found) {
        openOrHydrate();
        return;
    }

    // The node is not synchronized (yet): check whether it exists on the remote side.
    checkRemoteExistence();
}

bool OpenFileUrlHandler::resolveNode(bool &found) {
    found = false;

    std::vector<Sync> syncs;
    if (!ParmsDb::instance()->selectAllSyncs(_driveDbId, syncs)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllSyncs - driveDbId=" << _driveDbId);
        return false;
    }

    // Examine the syncs whose engine is running first, and make the examination order deterministic.
    {
        const std::scoped_lock lock(_appServer->syncPalMapMutex);
        std::stable_sort(syncs.begin(), syncs.end(), [this](const Sync &sync1, const Sync &sync2) {
            const bool running1 = _appServer->syncPalMap.contains(sync1.dbId());
            const bool running2 = _appServer->syncPalMap.contains(sync2.dbId());
            if (running1 != running2) return running1;
            return sync1.dbId() < sync2.dbId();
        });
    }

    for (const auto &sync: syncs) {
        std::shared_ptr<SyncDb> syncDb;
        {
            const std::scoped_lock lock(_appServer->syncPalMapMutex);
            const auto syncPalMapIt = _appServer->syncPalMap.find(sync.dbId());
            if (syncPalMapIt != _appServer->syncPalMap.end() && syncPalMapIt->second) {
                syncDb = syncPalMapIt->second->syncDb();
            }
        }
        if (!syncDb) {
            // The engine of this sync is not running: read its DB directly.
            auto tempSyncDbIt = _tempSyncDbMap.find(sync.dbId());
            if (tempSyncDbIt == _tempSyncDbMap.end()) {
                try {
                    tempSyncDbIt = _tempSyncDbMap
                                           .try_emplace(sync.dbId(),
                                                        std::make_shared<SyncDb>(sync.dbPath().string(), sync.targetNodeId()))
                                           .first;
                } catch (const std::exception &e) {
                    LOG_WARN(Log::instance()->getLogger(),
                             "Failed to open the sync DB - syncDbId=" << sync.dbId() << ": " << e.what());
                    continue;
                }
            }
            syncDb = tempSyncDbIt->second;
        }

        SyncPath relativePath;
        bool nodeFound = false;
        if (!syncDb->path(ReplicaSide::Remote, _request.nodeId, relativePath, nodeFound)) {
            LOG_WARN(Log::instance()->getLogger(), "Error in SyncDb::path - syncDbId=" << sync.dbId());
            continue;
        }
        if (!nodeFound) continue;

        found = true;
        _syncDbId = sync.dbId();
        _relativePath = relativePath;
        _localPath = relativePath.empty() ? sync.localPath() : sync.localPath() / relativePath;
        break;
    }

    return true;
}

void OpenFileUrlHandler::checkRemoteExistence() {
    setPhase(Phase::CheckRemoteExistence, std::chrono::milliseconds(0), std::chrono::milliseconds(0));

    const NodeId nodeId = _request.nodeId;
    const auto job = std::make_shared<ItemsExistJob>(_driveDbId, NodeSet{nodeId});
    job->setAdditionalCallback([this, job, nodeId](UniqueId) {
        // Job manager worker thread.
        const ExitInfo exitInfo = job->exitInfo();
        IoError ioError = IoError::Success;
        const bool exists = job->exists(nodeId, ioError);

        // Hand the result over to the AppServer thread. `this` is used as the context object: if the handler has
        // been destroyed in the meantime, the call is simply discarded.
        QMetaObject::invokeMethod(
                this, [this, exitInfo, exists] { onRemoteExistenceChecked(exitInfo, exists); }, Qt::QueuedConnection);
    });
    SyncJobManagerSingleton::instance()->queueAsyncJob(job, Poco::Thread::PRIO_NORMAL);
}

void OpenFileUrlHandler::onRemoteExistenceChecked(const ExitInfo &exitInfo, bool exists) {
    if (_phase != Phase::CheckRemoteExistence) return; // Stale callback.

    if (!exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ItemsExistJob - nodeId=" << _request.nodeId << ": " << exitInfo);
        fail(tr("The element requested from this kDrive link could not be opened."));
        return;
    }

    if (!exists) {
        fail(tr("The element requested from this kDrive link could not be found."));
        return;
    }

    if (!_waitingNotificationSent) {
        _waitingNotificationSent = true;
        notifyUser(tr("The requested element is being synchronized. It will open once available."));
    }

    setPhase(Phase::WaitForSync, syncWaitTickInterval, syncWaitTimeout);
}

void OpenFileUrlHandler::onSyncWaitTick() {
    if (_phase != Phase::WaitForSync) return;

    bool found = false;
    if (!resolveNode(found)) {
        fail(tr("The element requested from this kDrive link could not be opened."));
        return;
    }
    if (found) {
        openOrHydrate();
    }
}

void OpenFileUrlHandler::onDeadlineReached() {
    switch (_phase) {
        case Phase::WaitForSync:
            fail(tr("The element requested from this kDrive link could not be synchronized. Please try again later."));
            break;
        case Phase::WaitForHydration:
            fail(tr("The download of the element requested from this kDrive link took too long."));
            break;
        default:
            break;
    }
}

void OpenFileUrlHandler::openOrHydrate() {
    ItemType itemType;
    if (!IoHelper::getItemType(_localPath, itemType)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in IoHelper::getItemType: " << Utility::formatIoError(_localPath, itemType.ioError));
        fail(tr("The element requested from this kDrive link could not be opened."));
        return;
    }

    if (itemType.linkType != LinkType::None || itemType.nodeType == NodeType::Directory) {
        // Symlinks and directories do not need any hydration.
        openResolvedItem();
        return;
    }

    // No VFS plugin for this sync (e.g. LiteSync disabled): the item is a plain file, open it directly. The same
    // fallback is used when the engine of the sync is not running, in which case the system hydrates the placeholder
    // on access.
    std::shared_ptr<Vfs> vfs;
    if (const auto exitInfo = _appServer->getVfs(_syncDbId, vfs); !exitInfo || !vfs) {
        openResolvedItem();
        return;
    }

    VfsStatus vfsStatus;
    if (const auto exitInfo = vfs->status(_localPath, vfsStatus); !exitInfo) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in Vfs::status - " << Utility::formatSyncPath(_localPath));
        fail(tr("The element requested from this kDrive link could not be opened."));
        return;
    }

    if (!vfsStatus.isPlaceholder || vfsStatus.isHydrated || vfsStatus.isSyncing) {
        // Nothing to download, or a download is already ongoing: the system hydrates the file on access.
        openResolvedItem();
        return;
    }

    // The item is a dehydrated placeholder: download it before opening it.
    if (!triggerHydration()) {
        // The download could not be triggered: open the item anyway, the system hydrates it on access.
        openResolvedItem();
        return;
    }

    if (!_waitingNotificationSent) {
        _waitingNotificationSent = true;
        notifyUser(tr("The requested element is being downloaded. It will open once available."));
    }
    setPhase(Phase::WaitForHydration, std::chrono::milliseconds(0), hydrationTimeout);
}

bool OpenFileUrlHandler::triggerHydration() {
    std::shared_ptr<SyncPal> syncPal;
    {
        const std::scoped_lock lock(_appServer->syncPalMapMutex);
        const auto syncPalMapIt = _appServer->syncPalMap.find(_syncDbId);
        if (syncPalMapIt == _appServer->syncPalMap.end() || !syncPalMapIt->second) return false;
        syncPal = syncPalMapIt->second;
    }

#if defined(KD_MACOS)
    std::shared_ptr<Vfs> vfs;
    if (const auto exitInfo = _appServer->getVfs(_syncDbId, vfs); !exitInfo || !vfs) return false;

    // On macOS, setting the pin state to AlwaysLocal is required to trigger the hydration
    // (see ExtensionJob::commandMakeAvailableLocallyDirect).
    _initialPinState = vfs->pinState(_relativePath);
    if (_initialPinState != PinState::AlwaysLocal) {
        if (const auto exitInfo = vfs->setPinState(_relativePath, PinState::AlwaysLocal); !exitInfo) {
            LOGW_WARN(Log::instance()->getLogger(), L"Error in Vfs::setPinState - " << Utility::formatSyncPath(_localPath));
            return false;
        }
    }
#endif

    const auto observer = [this](UniqueId, bool success) {
        // Invoked on a job manager worker thread, while the SyncPal direct download jobs map is locked: it must not
        // call any SyncPal method. Hand the result over to the AppServer thread. `this` is used as the context
        // object: if the handler has been destroyed in the meantime, the call is simply discarded.
        QMetaObject::invokeMethod(
                this, [this, success] { onHydrationFinished(success); }, Qt::QueuedConnection);
    };

    if (const auto exitInfo = syncPal->addDlDirectJob(_relativePath, _localPath, SyncPath(), observer); !exitInfo) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in SyncPal::addDlDirectJob - " << Utility::formatSyncPath(_localPath) << L": " << exitInfo);
#if defined(KD_MACOS)
        // Cancel the hydration and restore the initial pin state.
        vfs->cancelHydrate(_localPath);
        if (_initialPinState != PinState::Unknown) (void) vfs->setPinState(_relativePath, _initialPinState);
#endif
        return false;
    }

    return true;
}

void OpenFileUrlHandler::onHydrationFinished(bool success) {
    if (_phase != Phase::WaitForHydration) return; // Stale callback.

    if (!success) {
        fail(tr("The download of the element requested from this kDrive link failed."));
        return;
    }

    openResolvedItem();
}

void OpenFileUrlHandler::openResolvedItem() {
    SyncPath pathToOpen = _localPath;
    if (shouldOpenParentFolder(_localPath)) {
        LOGW_INFO(Log::instance()->getLogger(),
                  L"Executable item, revealing its parent folder instead - " << Utility::formatSyncPath(_localPath));
        pathToOpen = _localPath.parent_path();
    }

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(Path2QStr(pathToOpen)))) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in QDesktopServices::openUrl - " << Utility::formatSyncPath(pathToOpen));
        fail(tr("The element requested from this kDrive link could not be opened."));
        return;
    }

    LOGW_INFO(Log::instance()->getLogger(), L"Item opened from URL - " << Utility::formatSyncPath(pathToOpen));
    succeed();
}

void OpenFileUrlHandler::setPhase(Phase phase, std::chrono::milliseconds tickInterval, std::chrono::milliseconds timeout) {
    _phase = phase;
    if (tickInterval.count() > 0) {
        _syncWaitTimer.start(tickInterval);
    } else {
        _syncWaitTimer.stop();
    }
    if (timeout.count() > 0) {
        _deadlineTimer.start(timeout);
    } else {
        _deadlineTimer.stop();
    }
}

void OpenFileUrlHandler::notifyUser(const QString &message) const {
    _appServer->sendShowNotification(QString::fromStdString(Theme::instance()->appName()), message);
}

void OpenFileUrlHandler::succeed() {
    setPhase(Phase::Done, std::chrono::milliseconds(0), std::chrono::milliseconds(0));
    emit finished(true);
}

void OpenFileUrlHandler::fail(const QString &userMessage) {
    setPhase(Phase::Done, std::chrono::milliseconds(0), std::chrono::milliseconds(0));
    LOG_WARN(Log::instance()->getLogger(), "Failed to open item from URL - url=" << _url);
    notifyUser(userMessage);
    emit finished(false);
}

} // namespace KDC
