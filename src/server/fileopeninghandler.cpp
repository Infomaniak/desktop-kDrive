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

#include "fileopeninghandler.h"

#include "appserver.h"
#include "libcommon/theme/theme.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"
#include "libparms/db/parmsdb.h"
#include "libsyncengine/db/syncdb.h"
#include "libsyncengine/jobs/network/kDrive_API/itemsexistjob.h"
#include "libsyncengine/jobs/syncjobmanager.h"
#include "libsyncengine/syncpal/syncpal.h"

#include "libsyncengine/jobs/network/kDrive_API/apitranslator.h"

#include <QDesktopServices>

#include <Poco/Exception.h>
#include <Poco/URI.h>

#include <log4cplus/loggingmacros.h>

#include <algorithm>
#include <charconv>
#include <unordered_set>
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

//! Parses a strictly positive integer identifier (e.g. a user id or a drive id) into `id`. Returns false if `idStr`
//! does not exactly represent one.
bool parsePositiveId(std::string_view idStr, int64_t &id) {
    // std::from_chars parses the whole string strictly (no whitespace, no leading '+', no trailing garbage) and never
    // throws.
    const auto result = std::from_chars(idStr.data(), idStr.data() + idStr.size(), id);
    return result.ec == std::errc{} && result.ptr == idStr.data() + idStr.size() && id > 0;
}
} // namespace

FileOpeningHandler::FileOpeningHandler(AppServer *appServer, std::string url, QObject *parent) :
    QObject(parent),
    _appServer(appServer),
    _url(std::move(url)) {
    _deadlineTimer.setSingleShot(true);
    (void) connect(&_syncWaitTimer, &QTimer::timeout, this, &FileOpeningHandler::onSyncWaitTick);
    (void) connect(&_deadlineTimer, &QTimer::timeout, this, &FileOpeningHandler::onDeadlineReached);
}

void FileOpeningHandler::start() {
    LOG_INFO(Log::instance()->getLogger(), "Processing file opening URL - url=" << _url);

    if (!parseDeeplinkUrl(_url, _request)) {
        LOG_WARN(Log::instance()->getLogger(), "Invalid file opening URL - url=" << _url);
        handleFileOpeningFailure("This kDrive link is invalid.");
        return;
    }

    setPhase(Phase::Resolve, std::chrono::milliseconds(0), std::chrono::milliseconds(0));
    openFileIndicatedByDeeplink();
}

bool FileOpeningHandler::isFileOpeningDeeplink(const std::string &urlStr) {
    Poco::URI uri;
    return parseUri(urlStr, uri) && isOpenFileUri(uri);
}

bool FileOpeningHandler::parseDeeplinkUrl(const std::string &urlStr, Request &request) {
    Poco::URI uri;
    if (!parseUri(urlStr, uri) || !isOpenFileUri(uri)) return false;

    // Poco::URI decodes the path and excludes the query and the fragment while parsing, and getPathSegments drops the
    // empty segments produced by the leading, doubled, and trailing slashes.
    std::vector<std::string> segments;
    uri.getPathSegments(segments);
    if (segments.size() != 6 || segments[0] != "user" || segments[2] != "drive" || segments[4] != "file") return false;

    UserId userId;
    if (!parsePositiveId(segments[1], userId)) return false;
    request.user.setUserId(userId);

    if (!parsePositiveId(segments[3], request.driveId)) return false;
    request.remoteNodeId = segments[5];

    return !request.remoteNodeId.empty();
}

bool FileOpeningHandler::shouldOpenParentFolder(const SyncPath &path) {
    static const std::unordered_set<std::string> executableExtensions = {".exe", ".bat", ".cmd", ".com", ".scr", ".msi", ".ps1",
                                                                         ".vbs", ".js",  ".jar", ".sh",  ".app", ".apk"};
    return executableExtensions.contains(CommonUtility::toLower(path.extension().string()));
}

void FileOpeningHandler::openFileIndicatedByDeeplink() {
    constexpr auto failureMessage = "The element requested from this kDrive deeplink could not be opened.";
    // The drive lookup is performed once: the drive configuration cannot appear while the request is being processed.
    if (_driveDbIds.empty() && !ParmsDb::instance()->driveDbIds(_request.user.userId(), _request.driveId, _driveDbIds)) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in ParmsDb::driveDbIds: userId=" << _request.user.userId() << ", driveId=" << _request.driveId);
        handleFileOpeningFailure(failureMessage);

        return;
    }

    bool itemFound = false;
    if (!resolveDeeplink(itemFound)) {
        handleFileOpeningFailure(failureMessage);

        return;
    }

    if (itemFound) {
        (void) openResolvedItem();

        return;
    }

    // The node is not synchronized (yet): check whether it exists on the remote side.
    checkRemoteExistence();
}

void FileOpeningHandler::sortSyncsByDbId(std::vector<Sync> &syncs) const {
    const std::scoped_lock lock(_appServer->syncPalMapMutex);

    std::ranges::stable_sort(syncs, [this](const Sync &sync1, const Sync &sync2) {
        const bool running1 = _appServer->syncPalMap.contains(sync1.dbId());
        if (const bool running2 = _appServer->syncPalMap.contains(sync2.dbId()); running1 != running2) return running1;

        return sync1.dbId() < sync2.dbId();
    });
}

std::shared_ptr<SyncDb> FileOpeningHandler::getSyncDb(SyncDbId syncDbId) const {
    std::shared_ptr<SyncDb> syncDb;

    const std::scoped_lock lock(_appServer->syncPalMapMutex);

    if (const auto syncPalMapIt = _appServer->syncPalMap.find(syncDbId);
        syncPalMapIt != _appServer->syncPalMap.end() && syncPalMapIt->second)
        syncDb = syncPalMapIt->second->syncDb();

    return syncDb;
}

ExitInfo FileOpeningHandler::getBestAdmissibleSync(const std::vector<Sync> &syncs, Sync &bestSync, SyncPath &relativePath) {
    bestSync = Sync{}; // Default value as long as an admissible sync is not found.
    relativePath.clear();

    for (const auto &sync: syncs) {
        std::shared_ptr<SyncDb> syncDb = getSyncDb(sync.dbId());
        if (!syncDb) {
            // The engine of this sync is not running: read its DB directly.
            auto tempSyncDbIt = _tempSyncDbMap.find(sync.dbId());
            if (tempSyncDbIt == _tempSyncDbMap.end()) {
                try {
                    tempSyncDbIt = _tempSyncDbMap
                                           .try_emplace(sync.dbId(),
                                                        std::make_shared<SyncDb>(sync.dbPath().string(), sync.targetNodeId()))
                                           .first;
                } catch (const std::runtime_error &e) {
                    LOG_WARN(Log::instance()->getLogger(),
                             "Failed to open the sync DB - syncDbId=" << sync.dbId() << ": " << e.what());
                    continue;
                }
            }
            syncDb = tempSyncDbIt->second;
        }

        bool nodeFound = false;
        if (!syncDb->path(ReplicaSide::Remote, _request.remoteNodeId, relativePath, nodeFound)) {
            LOG_WARN(Log::instance()->getLogger(), "Error in SyncDb::path - syncDbId=" << sync.dbId());

            continue;
        }

        if (!nodeFound) continue;
        bestSync = sync;

        break;
    }

    if (bestSync.dbId() == 0) {
        LOG_WARN(Log::instance()->getLogger(),
                 "No admissible sync found for userId=" << _request.user.userId() << ", driveId=" << _request.driveId
                                                        << ", remoteNodeId=" << _request.remoteNodeId);

        return ExitCode::DataError;
    }

    return ExitCode::Ok;
}

ExitInfo FileOpeningHandler::resolveDeeplink(bool &itemFound) {
    itemFound = false;

    if (bool userFound = false; !ParmsDb::instance()->selectUserByUserId(_request.user.userId(), _request.user, userFound)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectUserByUserId.");

        return ExitCode::DbError;
    }

    RemoteNodeId sharedFolderRemoteId;
    if (const auto exitInfo = ApiTranslator::getSpecialFolderRemoteId(_request.user.dbId(), _request.driveId,
                                                                      SpecialRemoteFolder::Shared, sharedFolderRemoteId);
        !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Error in FileOpeningHandler::getSpecialFolderRemoteIds: userDbId="
                                                       << _request.user.dbId() << ", driveId=" << _request.driveId << ": "
                                                       << exitInfo);

        return exitInfo;
    }

    // The `Shared` folder is not synchronized locally if no items have been shared on the specified drive with the specified
    // user.
    if (_request.remoteNodeId == sharedFolderRemoteId) {
        bool emptyShared = false;
        if (const auto exitInfo = ApiJobUtility::checkIfSharedFolderIsEmpty(_request.user.dbId(), _request.driveId, emptyShared);
            !exitInfo)
            return exitInfo;

        if (emptyShared) {
            LOG_WARN(Log::instance()->getLogger(),
                     "The Shared folder is empty - userDbId=" << _request.user.dbId() << ", driveId=" << _request.driveId);

            return ExitCode::DataError;
        }
    }

    // The `Private` folder is not synchronized locally: it is represented by the root folder in the V2 API.
    if (const auto exitInfo = ApiTranslator::translateV3ToV2(_request.user.dbId(), _request.driveId, _request.remoteNodeId);
        !exitInfo)
        return exitInfo;

    std::vector<Sync> syncs;
    if (!ParmsDb::instance()->selectAllSyncs(syncs)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllSyncs.");

        return ExitCode::DbError;
    }

    std::erase_if(syncs, [this](const Sync &sync) { return !_driveDbIds.contains(sync.driveDbId()); });

    // Examine the syncs whose engine is running first, and make the examination order deterministic.
    sortSyncsByDbId(syncs);
    _driveDbId = syncs.front().driveDbId(); // Default value as long as an admissible sync is not found.

    Sync bestSync;
    SyncPath relativePath;
    if (const auto exitInfo = getBestAdmissibleSync(syncs, bestSync, relativePath); !exitInfo) return exitInfo;

    itemFound = true;
    _driveDbId = bestSync.driveDbId();
    _syncDbId = bestSync.dbId();
    _relativePath = relativePath;
    _localPath = relativePath.empty() ? bestSync.localPath() : bestSync.localPath() / relativePath;

    return ExitCode::Ok;
}

void FileOpeningHandler::checkRemoteExistence() {
    setPhase(Phase::CheckRemoteExistence, std::chrono::milliseconds(0), std::chrono::milliseconds(0));

    const NodeId nodeId = _request.remoteNodeId;
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

void FileOpeningHandler::onRemoteExistenceChecked(const ExitInfo &exitInfo, bool exists) {
    if (_phase != Phase::CheckRemoteExistence) return; // Stale callback.

    if (!exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ItemsExistJob - nodeId=" << _request.remoteNodeId << ": " << exitInfo);
        handleFileOpeningFailure("The element requested from this kDrive link could not be opened.");
        return;
    }

    if (!exists) {
        handleFileOpeningFailure("The element requested from this kDrive link could not be found.");
        return;
    }

    if (!_waitingNotificationSent) {
        _waitingNotificationSent = true;
        notifyUser("The requested element is being synchronized. It will open once available.");
    }

    setPhase(Phase::WaitForItemToSynchronize, syncWaitTickInterval, syncWaitTimeout);
}

void FileOpeningHandler::onSyncWaitTick() {
    if (_phase != Phase::WaitForItemToSynchronize) return;

    bool found = false;
    if (!resolveDeeplink(found)) {
        handleFileOpeningFailure("The element requested from this kDrive link could not be opened.");
        return;
    }
    if (found) {
        (void) openResolvedItem();
    }
}

void FileOpeningHandler::onDeadlineReached() {
    switch (_phase) {
        case Phase::WaitForItemToSynchronize:
            handleFileOpeningFailure(
                    "The element requested from this kDrive link could not be synchronized. Please try again later.");
            break;
        case Phase::WaitForItemHydrationToComplete:
            handleFileOpeningFailure("The download of the element requested from this kDrive link took too long.");
            break;
        default:
            break;
    }
}

ExitInfo FileOpeningHandler::openResolvedItem() {
    ItemType itemType;
    if (!IoHelper::getItemType(_localPath, itemType)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in IoHelper::getItemType: " << Utility::formatIoError(_localPath, itemType.ioError));
        handleFileOpeningFailure("The element requested from this kDrive link could not be opened.");

        return ExitCode::SystemError;
    }

    if (itemType.linkType != LinkType::None || itemType.nodeType == NodeType::Directory) {
        // Symlinks and directories do not need any hydration.
        return openLocalItem();
    }

    // No VFS plugin for this sync (e.g. LiteSync disabled): the item is a plain file, open it directly. The same
    // fallback is used when the engine of the sync is not running, in which case the system hydrates the placeholder
    // on access.
    std::shared_ptr<Vfs> vfs;
    if (const auto exitInfo = _appServer->getVfs(_syncDbId, vfs); !exitInfo || !vfs) {
        return openLocalItem();
    }

    VfsStatus vfsStatus;
    if (const auto exitInfo = vfs->status(_localPath, vfsStatus); !exitInfo) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in Vfs::status - " << Utility::formatSyncPath(_localPath));
        handleFileOpeningFailure("The element requested from this kDrive link could not be opened.");

        return ExitCode::SystemError;
    }

    if (!vfsStatus.isPlaceholder || vfsStatus.isHydrated || vfsStatus.isSyncing) {
        // Nothing to download, or a download is already ongoing: the system hydrates the file on access.
        return openLocalItem();
    }

    // The item is a dehydrated placeholder: download it before opening it.
    if (const auto exitInfo = hydrateResolvedPlaceholder(); !exitInfo) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in hydrateResolvedPlaceholder " << Utility::formatExitInfo(_localPath, exitInfo));
        // The download could not be triggered: open the item anyway, the system hydrates it on access.
        return openLocalItem();
    }

    if (!_waitingNotificationSent) {
        _waitingNotificationSent = true;
        notifyUser("The requested element is being downloaded. It will open once available.");
    }
    setPhase(Phase::WaitForItemHydrationToComplete, std::chrono::milliseconds(0), hydrationTimeout);

    return ExitCode::Ok;
}

ExitInfo FileOpeningHandler::hydrateResolvedPlaceholder() {
    std::shared_ptr<SyncPal> syncPal;
    {
        const std::scoped_lock lock(_appServer->syncPalMapMutex);
        const auto syncPalMapIt = _appServer->syncPalMap.find(_syncDbId);
        if (syncPalMapIt == _appServer->syncPalMap.end() || !syncPalMapIt->second) return ExitCode::DataError;
        syncPal = syncPalMapIt->second;
    }

#if defined(KD_MACOS)
    std::shared_ptr<Vfs> vfs;
    if (const auto exitInfo = _appServer->getVfs(_syncDbId, vfs); !exitInfo || !vfs) return exitInfo;

    // On macOS, setting the pin state to `AlwaysLocal` is required to trigger the hydration
    // (see ExtensionJob::commandMakeAvailableLocallyDirect).
    _initialPinState = vfs->pinState(_relativePath);
    if (_initialPinState != PinState::AlwaysLocal) {
        if (const auto exitInfo = vfs->setPinState(_relativePath, PinState::AlwaysLocal); !exitInfo) {
            LOGW_WARN(Log::instance()->getLogger(), L"Error in Vfs::setPinState - " << Utility::formatSyncPath(_localPath));

            return exitInfo;
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
        return exitInfo;
    }

    return ExitCode::Ok;
}

void FileOpeningHandler::onHydrationFinished(bool success) {
    if (_phase != Phase::WaitForItemHydrationToComplete) return; // Stale callback.

    if (!success) {
        handleFileOpeningFailure("The download of the element requested from this kDrive link failed.");
        return;
    }

    (void) openLocalItem();
}

ExitInfo FileOpeningHandler::openLocalItem() {
    SyncPath pathToOpen = _localPath;
    if (shouldOpenParentFolder(_localPath)) {
        LOGW_INFO(Log::instance()->getLogger(),
                  L"Executable item, revealing its parent folder instead - " << Utility::formatSyncPath(_localPath));
        pathToOpen = _localPath.parent_path();
    }

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(Path2QStr(pathToOpen)))) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in QDesktopServices::openUrl - " << Utility::formatSyncPath(pathToOpen));
        handleFileOpeningFailure("The element requested from this kDrive link could not be opened.");

        return ExitCode::SystemError;
    }

    LOGW_INFO(Log::instance()->getLogger(), L"Item opened from URL - " << Utility::formatSyncPath(pathToOpen));
    handleFileOpeningSuccess();

    return ExitCode::Ok;
}

void FileOpeningHandler::setPhase(Phase phase, std::chrono::milliseconds tickInterval, std::chrono::milliseconds timeout) {
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

void FileOpeningHandler::notifyUser(const std::string &message) const {
    _appServer->sendShowNotification(QString::fromStdString(Theme::instance()->appName()), QString::fromStdString(message));
}

void FileOpeningHandler::handleFileOpeningSuccess() {
    setPhase(Phase::Done, std::chrono::milliseconds(0), std::chrono::milliseconds(0));
    emit finished(true);
}

void FileOpeningHandler::handleFileOpeningFailure(const std::string &userMessage) {
    setPhase(Phase::Done, std::chrono::milliseconds(0), std::chrono::milliseconds(0));
    LOG_WARN(Log::instance()->getLogger(), "Failed to open item from deeplink URL - url=" << _url);
    notifyUser(userMessage);
    emit finished(false);
}

} // namespace KDC
