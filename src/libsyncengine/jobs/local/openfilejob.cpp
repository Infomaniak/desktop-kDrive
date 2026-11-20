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

#include "openfilejob.h"

#include "jobs/network/kDrive_API/apitranslator.h"
#include "jobs/network/kDrive_API/itemsexistjob.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"
#include "libparms/db/parmsdb.h"

#include <log4cplus/loggingmacros.h>

#include <Poco/Exception.h>
#include <Poco/URI.h>

#include <algorithm>
#include <charconv>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace KDC {

namespace {
// The requested item may have been created on the remote side recently: keep resolving it during `_syncWaitTimeout`
// before giving up.
constexpr std::chrono::milliseconds syncWaitTickInterval(2000);
// Downloading a large item may take a long time: keep waiting for the hydration during `hydrationTimeout` before
// giving up.
constexpr std::chrono::hours hydrationTimeout(1);
constexpr std::chrono::milliseconds hydrationWaitTick(1000);

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
bool parsePositiveId(const std::string_view idStr, int64_t &id) {
    // std::from_chars parses the whole string strictly (no whitespace, no leading '+', no trailing garbage) and never
    // throws.
    const auto result = std::from_chars(idStr.data(), idStr.data() + idStr.size(), id);
    return result.ec == std::errc{} && result.ptr == idStr.data() + idStr.size() && id > 0;
}

//! Delay before the first notification of an ongoing synchronization sent to the user interface, and interval between
//! the following notifications.
constexpr std::chrono::seconds kFirstWaitNotifyDelay(5);
constexpr std::chrono::minutes kWaitNotifyInterval(2);
//! Minimum delay between two download progress notifications sent to the user interface.
constexpr std::chrono::milliseconds kProgressNotifyMinInterval(500);
} // namespace

//! Completion state of a hydration, shared between the OpenFileJob waiting for it and the observers invoked by the
//! download job. It is held through shared pointers because the observers may outlive the OpenFileJob (e.g. when the
//! wait times out, the job is aborted, or the request is canceled while the download is still running).
struct OpenFileJob::HydrationCompletion {
        std::mutex mutex;
        std::condition_variable cv;
        bool finished{false}; // The download has completed, successfully or not.
        bool success{false}; // Meaningful once `finished` is true: the download succeeded.
        std::atomic<int8_t> percent{-1}; // Latest download progress in percent reported by the engine.
};

OpenFileJob::OpenFileJob(std::string url, GetSyncPalCallback getSyncPal, GetVfsCallback getVfs) :
    _url(std::move(url)),
    _getSyncPal(std::move(getSyncPal)),
    _getVfs(std::move(getVfs)) {}

bool OpenFileJob::isFileOpeningUri(const std::string &urlStr) {
    Poco::URI uri;
    return parseUri(urlStr, uri) && isOpenFileUri(uri);
}

bool OpenFileJob::parseDeeplinkUrl(const std::string &urlStr, Request &request) {
    Poco::URI uri;
    if (!parseUri(urlStr, uri) || !isOpenFileUri(uri)) return false;

    // Poco::URI decodes the path and excludes the query and the fragment while parsing, and getPathSegments drops the
    // empty segments produced by the leading, doubled, and trailing slashes.
    std::vector<std::string> segments;
    uri.getPathSegments(segments);
    if (segments.size() != 6 || segments[0] != "user" || segments[2] != "drive" || segments[4] != "file") return false;

    UserId userId{0};
    if (!parsePositiveId(segments[1], userId)) return false;
    request.user.setUserId(userId);

    if (!parsePositiveId(segments[3], request.driveId)) return false;
    request.remoteNodeId = segments[5];

    return !request.remoteNodeId.empty();
}

ExitInfo OpenFileJob::canRun() {
    if (_getSyncPal && _getVfs) return ExitCode::Ok;

    LOGW_WARN(_logger, L"Error in OpenFileJob::canRun - Invalid callbacks");
    return {ExitCode::LogicError, ExitCause::InvalidArgument};
}

ExitInfo OpenFileJob::runJob() {
    if (const auto exitInfo = canRun(); !exitInfo) return exitInfo;

    if (!parseDeeplinkUrl(_url, _request)) {
        LOGW_WARN(_logger, L"Invalid file opening URL");
        _outcome = Outcome::Failed;
        return {ExitCode::LogicError, ExitCause::InvalidArgument};
    }

    // Step 1: resolve the item indicated by the URL, waiting for it to be synchronized if needed. A timeout is a
    // terminal outcome reported to the user interface: stop here, there is nothing to open.
    if (const auto exitInfo = resolveItem(); !exitInfo || _outcome == Outcome::ResolveTimedOut) return exitInfo;

    // Step 2: open the item, hydrating it first if it is a dehydrated placeholder.
    return openResolvedLocalItem();
}

ExitInfo OpenFileJob::resolveItem() {
    // The drive lookup is performed once: the drive configuration cannot appear while the request is being processed.
    if (!ParmsDb::instance()->driveDbIds(_request.user.userId(), _request.driveId, _driveDbIds)) {
        LOGW_WARN(_logger,
                  L"Error in ParmsDb::driveDbIds: userId=" << _request.user.userId() << L", driveId=" << _request.driveId);
        _outcome = Outcome::Failed;

        return ExitCode::DbError;
    }

    if (_driveDbIds.empty()) {
        LOGW_WARN(_logger, L"No sync is configured for this drive - userId=" << _request.user.userId() << L", driveId="
                                                                             << _request.driveId);
        _outcome = Outcome::Failed;

        return {ExitCode::DataError, ExitCause::NotFound};
    }

    const auto waitStart = std::chrono::steady_clock::now();
    const auto deadline = waitStart + _syncWaitTimeout;
    std::chrono::steady_clock::time_point lastWaitNotify;

    forever {
        if (isAborted()) {
            LOGW_WARN(_logger, L"Job aborted while resolving the item - " << Utility::formatSyncPath(_localPath));
            _outcome = Outcome::Canceled;
            return {ExitCode::OperationCanceled, ExitCause::OperationCanceled};
        }

        bool itemFound = false;
        if (const auto exitInfo = resolveDeeplink(itemFound); !exitInfo) {
            LOGW_WARN(_logger, L"Error in OpenFileJob::resolveDeeplink: userId="
                                       << _request.user.userId() << L", driveId=" << _request.driveId << L", remoteNodeId="
                                       << CommonUtility::s2ws(_request.remoteNodeId) << L": " << exitInfo);
            _outcome = Outcome::Failed;

            return exitInfo;
        }

        if (itemFound) {
            setAffectedFilePath(_localPath);

            return ExitCode::Ok;
        }

        // The item is not synchronized (yet): check whether it still exists on the remote side.
        bool remoteItemExists = false;
        if (const auto exitInfo = checkRemoteItemExistence(remoteItemExists); !exitInfo) {
            LOGW_WARN(_logger, L"Error in OpenFileJob::checkRemoteItemExistence - remoteNodeId="
                                       << CommonUtility::s2ws(_request.remoteNodeId) << L": " << exitInfo);
            _outcome = Outcome::Failed;

            return exitInfo;
        }

        if (!remoteItemExists) {
            LOGW_WARN(_logger, L"The item does not exist on the remote side anymore - remoteNodeId="
                                       << CommonUtility::s2ws(_request.remoteNodeId));
            _outcome = Outcome::NotFound;

            return {ExitCode::DataError, ExitCause::NotFound};
        }

        // The item exists on the remote side only: inform the user that synchronizing it may take some time, and give
        // the user the opportunity to cancel the request.
        if (const auto now = std::chrono::steady_clock::now(); isWaitNotificationDue(now, waitStart, lastWaitNotify)) {
            notifyFeedback(FileOpeningPhase::WaitingSync, -1);
            lastWaitNotify = now;
        }

        // The item exists on the remote side only: wait for it to be synchronized.
        std::this_thread::sleep_for(syncWaitTickInterval);
        if (std::chrono::steady_clock::now() >= deadline) {
            LOGW_WARN(_logger,
                      L"The item was not synchronized in time - remoteNodeId=" << CommonUtility::s2ws(_request.remoteNodeId));
            _outcome = Outcome::ResolveTimedOut;

            return ExitCode::Ok;
        }
    }
}

void OpenFileJob::sortSyncsByRunningState(std::vector<Sync> &syncs) const {
    (void) std::ranges::stable_sort(syncs, [this](const Sync &sync1, const Sync &sync2) {
        const bool running1 = _getSyncPal(sync1.dbId()) != nullptr;
        if (const bool running2 = _getSyncPal(sync2.dbId()) != nullptr; running1 != running2) return running1;

        return sync1.dbId() < sync2.dbId();
    });
}

std::shared_ptr<SyncDb> OpenFileJob::getSyncDb(const SyncDbId syncDbId) const {
    const std::shared_ptr<SyncPal> syncPal = _getSyncPal(syncDbId);

    return syncPal ? syncPal->syncDb() : nullptr;
}

std::shared_ptr<SyncDb> OpenFileJob::openSyncDb(const Sync &sync) {
    try {
        auto tempSyncDb = std::make_shared<SyncDb>(sync.dbPath().string(), sync.targetNodeId());
        if (!tempSyncDb->prepare()) {
            LOGW_WARN(_logger, L"Failed to prepare the sync DB - syncDbId=" << sync.dbId());

            return nullptr;
        }

        return tempSyncDb;
    } catch (const std::runtime_error &e) {
        LOGW_WARN(_logger, L"Failed to open the sync DB - syncDbId=" << sync.dbId() << L": " << CommonUtility::s2ws(e.what()));

        return nullptr;
    }
}

ExitInfo OpenFileJob::getBestAdmissibleSync(const std::vector<Sync> &syncs, Sync &bestSync, SyncPath &relativePath, bool &found) {
    bestSync = Sync{}; // Default value as long as an admissible sync is not found.
    relativePath.clear();
    found = false;

    for (const auto &sync: syncs) {
        std::shared_ptr<SyncDb> syncDb = getSyncDb(sync.dbId());
        if (!syncDb) {
            // The engine of this sync is not running: read its DB directly.
            syncDb = openSyncDb(sync);
        }

        if (!syncDb) continue;

        bool nodeFound = false;
        if (!syncDb->path(ReplicaSide::Remote, _request.remoteNodeId, relativePath, nodeFound)) {
            LOGW_WARN(_logger, L"Error in SyncDb::dbId - syncDbId=" << sync.dbId());
        }

        if (!nodeFound) {
            if (!_getSyncPal(sync.dbId())) {
                // The engine of this sync is still not running.
                syncDb->close();
            }

            continue;
        }

        bestSync = sync;
        found = true;

        break;
    }

    if (!found) {
        LOGW_DEBUG(_logger, L"No admissible sync found for userId=" << _request.user.userId() << L", driveId=" << _request.driveId
                                                                    << L", remoteNodeId="
                                                                    << CommonUtility::s2ws(_request.remoteNodeId));
    }

    return ExitCode::Ok;
}

ExitInfo OpenFileJob::resolveDeeplink(bool &found) {
    found = false;

    if (bool userFound = false; !ParmsDb::instance()->selectUserByUserId(_request.user.userId(), _request.user, userFound)) {
        LOGW_WARN(_logger, L"Error in ParmsDb::selectUserByUserId.");

        return ExitCode::DbError;
    }

    RemoteNodeId sharedFolderRemoteId;
    if (const auto exitInfo = ApiTranslator::getSpecialFolderRemoteId(_request.user.dbId(), _request.driveId,
                                                                      SpecialRemoteFolder::Shared, sharedFolderRemoteId);
        !exitInfo) {
        LOGW_WARN(_logger, L"Error in ApiTranslator::getSpecialFolderRemoteId: userDbId="
                                   << _request.user.dbId() << L", driveId=" << _request.driveId << L": " << exitInfo);
        return exitInfo;
    }

    // The `Shared` folder is not synchronized locally if no items have been shared on the specified drive with the
    // specified user.
    if (_request.remoteNodeId == sharedFolderRemoteId) {
        bool emptyShared = false;
        if (const auto exitInfo = ApiJobUtility::checkIfSharedFolderIsEmpty(_request.user.dbId(), _request.driveId, emptyShared);
            !exitInfo)

            return exitInfo;

        if (emptyShared) {
            LOGW_WARN(_logger,
                      L"The Shared folder is empty - userDbId=" << _request.user.dbId() << L", driveId=" << _request.driveId);

            return {ExitCode::DataError, ExitCause::Unknown};
        }
    }

    // The `Private` folder is not synchronized locally: it is represented by the root folder in the V2 API.
    if (const auto exitInfo = ApiTranslator::translateV3ToV2(_request.user.dbId(), _request.driveId, _request.remoteNodeId);
        !exitInfo)
        return exitInfo;

    std::vector<Sync> syncs;
    if (!ParmsDb::instance()->selectAllSyncs(syncs)) {
        LOGW_WARN(_logger, L"Error in ParmsDb::selectAllSyncs.");
        return ExitCode::DbError;
    }

    (void) std::erase_if(syncs, [this](const Sync &sync) { return !_driveDbIds.contains(sync.driveDbId()); });

    // Examine the syncs whose engine is running first, and make the examination order deterministic.
    sortSyncsByRunningState(syncs);
    if (!syncs.empty())
        _driveDbId = syncs.front().driveDbId(); // Default value as long as an admissible sync is not
                                                // found.

    Sync bestSync;
    SyncPath relativePath;
    if (const auto exitInfo = getBestAdmissibleSync(syncs, bestSync, relativePath, found); !exitInfo) return exitInfo;
    if (!found) return ExitCode::Ok; // The item is not synchronized (yet).

    _driveDbId = bestSync.driveDbId();
    _syncDbId = bestSync.dbId();
    _relativePath = relativePath;
    _localPath = relativePath.empty() ? bestSync.localPath() : bestSync.localPath() / relativePath;


    bool pathTraversesLink = false;
    SyncPath linkPath;
    if (const auto ioError = IoHelper::checkIfPathTraversesLink(_localPath, pathTraversesLink, linkPath);
        ioError != IoError::Success) {
        _outcome = Outcome::Failed;

        return IoHelper::toExitInfo(ioError);
    } else if (pathTraversesLink) {
        LOGW_WARN(_logger, L"The local path traverses a symlink - " << Utility::formatSyncPath(_localPath));
        _outcome = Outcome::Failed;

        return {ExitCode::SystemError, ExitCause::MoveThroughSymlink};
    }

    return ExitCode::Ok;
}

ExitInfo OpenFileJob::checkRemoteItemExistence(bool &exists) const {
    const RemoteNodeId remoteNodeId = _request.remoteNodeId;
    const auto job = std::make_shared<ItemsExistJob>(_driveDbId, RemoteNodeIdSet{remoteNodeId});

    const auto exitInfo = job->runSynchronously();

    auto ioError = IoError::Success;
    exists = job->exists(remoteNodeId, ioError);

    return exitInfo;
}

ExitInfo OpenFileJob::openResolvedLocalItem() {
    ItemType itemType;
    if (!IoHelper::getItemType(_localPath, itemType)) {
        LOGW_WARN(_logger, L"Error in IoHelper::getItemType: " << Utility::formatIoError(_localPath, itemType.ioError));
        _outcome = Outcome::Failed;

        return IoHelper::toExitInfo(itemType.ioError);
    }

    if (itemType.linkType != LinkType::None || itemType.nodeType == NodeType::Directory) {
        // Symlinks and directories do not need any hydration.
        return readyToOpen();
    }

    // No VFS plugin for this sync (e.g. LiteSync disabled): the item is a plain file, open it directly. The same
    // fallback is used when the engine of the sync is not running, in which case the system hydrates the placeholder
    // on access.
    const auto vfs = _getVfs(_syncDbId);
    if (!vfs) return readyToOpen();

    VfsStatus vfsStatus;
    if (const auto exitInfo = vfs->status(_localPath, vfsStatus); !exitInfo) {
        LOGW_WARN(_logger, L"Error in Vfs::status - " << Utility::formatExitInfo(_localPath, exitInfo));
        _outcome = Outcome::Failed;

        return exitInfo;
    }

    if (!vfsStatus.isPlaceholder || vfsStatus.isHydrated) {
        // Nothing to download, or a download is already ongoing: the system hydrates the file on access.
        return readyToOpen();
    }

    if (vfsStatus.isSyncing) {
        // A download is already ongoing: we cannot launch another hydration process.
        _outcome = Outcome::Failed;
        LOGW_WARN(_logger, L"The file is being hydrated, retry later: " << Utility::formatSyncPath(_localPath));

        // Happens under normal circumstances.
        return ExitCode::Ok;
    }

    // The item is a dehydrated placeholder: download it before opening it.
    return hydrateResolvedPlaceholder();
}

ExitInfo OpenFileJob::hydrateResolvedPlaceholder() {
    std::shared_ptr<SyncPal> syncPal = _getSyncPal(_syncDbId);
    if (!syncPal) {
        LOGW_WARN(_logger, L"The engine of the sync is not running: the item cannot be hydrated - "
                                   << Utility::formatSyncPath(_localPath));
        _outcome = Outcome::Failed;

        return {ExitCode::DataError, ExitCause::Unknown};
    }

    const auto vfs = syncPal->vfs();
    if (!vfs) {
        LOGW_WARN(_logger, L"Vfs not found - " << Utility::formatSyncPath(_localPath));

        return readyToOpen();
    }

    // The completion state is published under the mutex once the download has been enqueued, and loaded by cancel()
    // on the thread processing the cancellation. This orders the publication against the cancellation: if cancel()
    // runs before the publication, the abortion checks performed after each step of the hydration below undo it; if
    // it runs after, it observes the state and cancels the download.
    const auto completion = std::make_shared<HydrationCompletion>();

    // The observer is invoked by the engine when the download job completes, possibly long after this job has timed
    // out or been aborted (the completion state is shared and kept alive by the shared_ptr). It reports the outcome of
    // the actual download.
    const auto observer = [completion](const UniqueId, const bool success) {
        const std::scoped_lock lock(completion->mutex);
        completion->finished = true;
        completion->success = success;
        completion->cv.notify_all();
    };

    auto progressObserver = [completion](const int16_t progress) { completion->percent.store(static_cast<int8_t>(progress)); };

    // Check the item state and set the pin state of the placeholder. Most failures in this phase leave the opening to
    // the system, which hydrates the item on access; a failure to set the pin state fails the request.
    bool hydrate = false;
    const auto exitInfo = checkHydrationPrerequisites(syncPal, hydrate);
    if (!exitInfo) {
        // The outcome is already set by checkHydrationPrerequisites for each of its error exits (Canceled or NotFound).
        LOGW_WARN(_logger,
                  L"Error in OpenFileJob::checkHydrationPrerequisites - " << Utility::formatExitInfo(_localPath, exitInfo));
    }
    if (!hydrate) return exitInfo;

    PinState pinState{PinState::Unknown};
    if (const auto pinStateExitInfo = setHydrationPinState(vfs, pinState); !pinStateExitInfo) {
        LOGW_WARN(_logger,
                  L"Error in OpenFileJob::setHydrationPinState - " << Utility::formatExitInfo(_localPath, pinStateExitInfo));
        _outcome = Outcome::Failed;

        return pinStateExitInfo;
    }

    // Enqueue the download job
    if (const auto enqueueExitInfo = syncPal->addDlDirectJob(_relativePath, _localPath, SyncPath{}, observer, progressObserver);
        !enqueueExitInfo) {
        LOGW_INFO(_logger,
                  L"Error in SyncPal::addDlDirectJob - " << Utility::formatSyncPath(_localPath) << L": " << enqueueExitInfo);
        (void) cancelHydrate(vfs, pinState, enqueueExitInfo);

        return readyToOpen();
    }

    {
        const std::scoped_lock lock(_hydrationMutex);
        _hydration = completion;
    }

    // The download is starting: inform the user, and keep informing as long as the progress evolves.
    notifyFeedback(FileOpeningPhase::Hydrating, 0);
    _lastNotifiedProgress = 0;
    _lastProgressNotify = std::chrono::steady_clock::now();

    if (isAborted()) {
        // The request has been canceled while the download was being enqueued: stop that download and revert the pin
        // state, so that nothing is transferred for a canceled request.
        LOGW_INFO(_logger, L"File opening canceled - " << Utility::formatSyncPath(_localPath));
        (void) syncPal->cancelDlDirectJobs({_localPath});
        (void) cancelHydrate(vfs, pinState, {ExitCode::OperationCanceled, ExitCause::OperationCanceled});
        _outcome = Outcome::Canceled;

        return {ExitCode::OperationCanceled, ExitCause::OperationCanceled};
    }

    // Wait for the download to complete, keeping the user interface informed of the progress.
    const auto deadline = std::chrono::steady_clock::now() + hydrationTimeout;
    std::unique_lock lock(completion->mutex);

    while (!completion->finished) {
        if (isAborted()) {
            LOGW_WARN(_logger, L"Job aborted while hydrating the item - " << Utility::formatSyncPath(_localPath));
            _outcome = Outcome::Canceled;

            return {ExitCode::OperationCanceled, ExitCause::OperationCanceled};
        }

        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
        if (remaining <= std::chrono::milliseconds::zero()) {
            LOGW_WARN(_logger, L"The download of the item did not complete in time - " << Utility::formatSyncPath(_localPath));
            _outcome = Outcome::HydrationTimedOut;

            // This is a regular scenario.
            return ExitCode::Ok;
        }

        completion->cv.wait_for(lock, std::min(remaining, hydrationWaitTick));

        notifyProgressIfChanged(completion->percent.load());
    }

    if (isAborted()) {
        // The request was canceled while the last download updates were being processed.
        LOGW_INFO(_logger, L"File opening canceled while hydrating the item - " << Utility::formatSyncPath(_localPath));
        _outcome = Outcome::Canceled;

        return {ExitCode::OperationCanceled, ExitCause::OperationCanceled};
    }

    if (!completion->success) {
        LOGW_WARN(_logger, L"The download of the item failed - " << Utility::formatSyncPath(_localPath));
        _outcome = Outcome::Failed;

        return {ExitCode::SystemError, ExitCause::Unknown};
    }

    LOGW_INFO(_logger, L"Item hydrated - " << Utility::formatSyncPath(_localPath));

    return readyToOpen();
}

ExitInfo OpenFileJob::checkHydrationPrerequisites(const std::shared_ptr<SyncPal> &syncPal, bool &hydrate) {
    assert(syncPal);
    if (isAborted()) {
        LOGW_INFO(_logger, L"File opening canceled - " << Utility::formatSyncPath(_localPath));
        hydrate = false;
        _outcome = Outcome::Canceled;

        return ExitInfo{ExitCode::OperationCanceled, ExitCause::OperationCanceled};
    }

    const auto vfs = syncPal->vfs();

    // Check that the file still exists locally
    bool exists = false;
    if (auto ioError = IoError::Success;
        !IoHelper::checkIfPathExists(_localPath, exists, ioError, IoHelper::PathCheckOption::Insensitive) ||
        ioError != IoError::Success) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_localPath, ioError));

        return readyToOpen();
    }

    if (!exists) {
        LOGW_DEBUG(_logger, L"Item does not exist - " << Utility::formatSyncPath(_localPath));

        return readyToOpen();
    }

    // Check the item type
    ItemType itemType;
    if (!IoHelper::getItemTypeAndProcessIoError(_localPath, itemType) || itemType.ioError != IoError::Success) {
        LOGW_WARN(_logger, L"No file data - " << Utility::formatSyncPath(_localPath));

        return readyToOpen();
    }

    if (itemType.linkType != LinkType::None) {
        LOGW_DEBUG(_logger, L"Do not hydrate symlinks - " << Utility::formatSyncPath(_localPath));
        (void) cancelHydrate(vfs, PinState::OnlineOnly, ExitCode::Unknown);

        return readyToOpen();
    }

    // Check file status
    auto status = SyncFileStatus::Unknown;
    VfsStatus vfsStatus;
    {
        // The remote existence is checked through the API: unlike SyncPal::checkIfExistsOnServer, the check does not
        // rely on the remote live snapshot of the sync and therefore works while the sync is paused.
        bool existsOnServer = false;
        if (const auto exitInfo = checkRemoteItemExistence(existsOnServer); !exitInfo) {
            LOGW_WARN(_logger, L"Error in OpenFileJob::checkRemoteItemExistence - " << Utility::formatSyncPath(_localPath)
                                                                                    << L": " << exitInfo);
            (void) cancelHydrate(vfs, PinState::OnlineOnly, exitInfo);

            return readyToOpen();
        }

        if (!existsOnServer) {
            // The item does not exist on the remote side anymore: do not hydrate it, and inform the user interface.
            LOGW_INFO(_logger, L"Item does not exist on the remote side anymore - " << Utility::formatSyncPath(_localPath));
            _outcome = Outcome::NotFound;

            (void) cancelHydrate(vfs, PinState::OnlineOnly, {ExitCode::DataError, ExitCause::NotFound});

            return {ExitCode::DataError, ExitCause::NotFound};
        }

        status = SyncFileStatus::Success;

        if (vfs->mode() == VirtualFileMode::Mac || vfs->mode() == VirtualFileMode::Win) {
            if (const auto exitInfo = vfs->status(_localPath, vfsStatus); !exitInfo) {
                LOGW_WARN(_logger, L"Error in Vfs::status - " << Utility::formatSyncPath(_localPath));
                (void) cancelHydrate(vfs, PinState::OnlineOnly, exitInfo);

                return readyToOpen();
            }

            if (vfsStatus.isSyncing) status = SyncFileStatus::Syncing;
        }
    }

    if (!vfsStatus.isPlaceholder) {
        // File is not a placeholder, this should never happen
        LOGW_WARN(_logger, L"File is not a placeholder - " << Utility::formatSyncPath(_localPath));

        return readyToOpen();
    }

    if (vfsStatus.isHydrated || status == SyncFileStatus::Syncing) {
        LOGW_INFO(_logger, L"File is already hydrated/ing - " << Utility::formatSyncPath(_localPath));

        return readyToOpen();
    }

    if (isAborted()) {
        // Canceled before the hydration could change anything: no pin state has been set and no download has been
        // enqueued.
        LOGW_INFO(_logger, L"File opening canceled - " << Utility::formatSyncPath(_localPath));
        _outcome = Outcome::Canceled;

        return ExitInfo{ExitCode::OperationCanceled, ExitCause::OperationCanceled};
    }

    hydrate = true;

    return ExitCode::Ok;
}

ExitInfo OpenFileJob::setHydrationPinState(const std::shared_ptr<Vfs> &vfs, PinState &pinState) {
#if defined(KD_MACOS)
    // Not done in Windows case: triggers a hydration
    // Get pin state
    pinState = vfs->pinState(_relativePath);

    if (pinState != PinState::AlwaysLocal) {
        // Set pin state
        if (const auto exitInfo = vfs->setPinState(_relativePath, PinState::AlwaysLocal); !exitInfo) {
            LOGW_INFO(_logger, L"Error in Vfs::setPinState - " << Utility::formatExitInfo(_localPath, exitInfo));

            return exitInfo;
        }
    }
#else
    pinState = PinState::OnlineOnly;
#endif

    return ExitCode::Ok;
}

ExitInfo OpenFileJob::cancelHydrate(const std::shared_ptr<Vfs> &vfs, const PinState pinState, const ExitInfo &exitInfo) {
    vfs->cancelHydrate(_localPath, exitInfo);

    if (pinState == PinState::Unknown) return ExitCode::Ok;

    if (const auto revertExitInfo = vfs->setPinState(_relativePath, pinState); !revertExitInfo) {
        LOGW_INFO(_logger, L"Error in Vfs::setPinState - " << Utility::formatSyncPath(_localPath) << L": " << revertExitInfo);

        return revertExitInfo;
    }

    return ExitCode::Ok;
}

void OpenFileJob::cancel() {
    abort();

    // Cancel the hydration of the item, if it is in progress: stop the direct download and revert the pin state set
    // before starting it. No-op when no hydration has been started or when it has already completed.
    //
    // The state is loaded under the mutex it is published with. When cancel() runs before the publication, the
    // abortion checks performed after each step of the hydration (see hydrateResolvedPlaceholder) undo it: no
    // download has been enqueued yet, or the just-enqueued download is canceled and the pin state is reverted.
    std::shared_ptr<HydrationCompletion> hydration;
    {
        const std::scoped_lock lock(_hydrationMutex);
        hydration = _hydration;
    }
    if (!hydration) return;

    {
        const std::scoped_lock lock(hydration->mutex);
        if (hydration->finished) return;
    }

    const std::shared_ptr<SyncPal> syncPal = _getSyncPal(_syncDbId);
    if (!syncPal) return;

    LOGW_INFO(_logger, L"Canceling the hydration of the item - " << Utility::formatSyncPath(_localPath));
    (void) syncPal->cancelDlDirectJobs({_localPath});

    if (const auto vfs = _getVfs(_syncDbId)) {
        vfs->cancelHydrate(_localPath, {ExitCode::OperationCanceled, ExitCause::OperationCanceled});
        (void) vfs->setPinState(_relativePath, PinState::OnlineOnly);
    }
}

void OpenFileJob::notifyFeedback(const FileOpeningPhase phase, const int8_t percent) const {
    if (_feedbackCallback) {
        _feedbackCallback(phase, percent, CommonUtility::syncPath2CommString(_localPath.filename()));
    }
}

void OpenFileJob::notifyProgressIfChanged(const int8_t percent) {
    if (percent < 0 || percent == _lastNotifiedProgress) return;

    const auto now = std::chrono::steady_clock::now();
    if (percent != 100 && now - _lastProgressNotify < kProgressNotifyMinInterval) return;

    _lastNotifiedProgress = percent;
    _lastProgressNotify = now;

    notifyFeedback(FileOpeningPhase::Hydrating, percent);
}

bool OpenFileJob::isWaitNotificationDue(const std::chrono::steady_clock::time_point now,
                                        const std::chrono::steady_clock::time_point waitStart,
                                        const std::chrono::steady_clock::time_point lastNotify) {
    // A default-constructed time point means "never notified yet": the first notification is due after
    // kFirstWaitNotifyDelay, the following ones every kWaitNotifyInterval.
    if (lastNotify == std::chrono::steady_clock::time_point{}) return now - waitStart >= kFirstWaitNotifyDelay;

    return now - lastNotify >= kWaitNotifyInterval;
}

ExitInfo OpenFileJob::readyToOpen() {
    _outcome = Outcome::ReadyToOpen;

    return ExitCode::Ok;
}

} // namespace KDC
