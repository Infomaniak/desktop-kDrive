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

#include "jobs/local/hydrateplaceholderjob.h"
#include "jobs/network/kDrive_API/apitranslator.h"
#include "jobs/network/kDrive_API/itemsexistjob.h"
#include "jobs/syncjobmanager.h"
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
// The requested item may have been created on the remote side recently: keep resolving it during `kSyncWaitTimeout`
// before giving up.
constexpr std::chrono::milliseconds kSyncWaitTickInterval(2000);
constexpr std::chrono::minutes kSyncWaitTimeout(10);
// Downloading a large item may take a long time: keep waiting for the hydration during `kHydrationTimeout` before
// giving up.
constexpr std::chrono::hours kHydrationTimeout(1);
constexpr std::chrono::milliseconds kHydrationWaitTick(1000);

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

//! Completion state of a hydration, shared between the OpenFileJob waiting for it and the observer invoked by the
//! download job. It is held through shared pointers because the observer may outlive the OpenFileJob (e.g. when the
//! wait times out or the job is aborted while the download is still running).
struct HydrationCompletion {
        std::mutex mutex;
        std::condition_variable cv;
        bool triggered{false}; // The hydration job has run: the item state has been checked and the download has been
                               // enqueued, unless an error occurred (see `triggerExitInfo`) or no download was needed
                               // (see `finished`).
        ExitInfo triggerExitInfo;
        bool finished{false}; // The hydration is over: the download has completed, or no download was needed.
        bool success{false}; // Meaningful once `finished` is true: the download succeeded, or no download was needed.
};
} // namespace

OpenFileJob::OpenFileJob(std::string url, GetSyncPalCallback getSyncPal, GetVfsCallback getVfs) :
    _url(std::move(url)),
    _getSyncPal(std::move(getSyncPal)),
    _getVfs(std::move(getVfs)) {}

bool OpenFileJob::parseDeeplinkUrl(const std::string &urlStr, Request &request) {
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

    // Step 1: resolve the item indicated by the URL, waiting for it to be synchronized if needed.
    if (const auto exitInfo = resolveItem(); !exitInfo) return exitInfo;

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

        return {ExitCode::DataError, ExitCause::Unknown};
    }

    const auto deadline = std::chrono::steady_clock::now() + kSyncWaitTimeout;

    forever {
        if (isAborted()) {
            LOGW_WARN(_logger, L"Job aborted while resolving the item - " << Utility::formatSyncPath(_localPath));
            _outcome = Outcome::Failed;
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

        // The item exists on the remote side only: wait for it to be synchronized.
        std::this_thread::sleep_for(kSyncWaitTickInterval);
        if (std::chrono::steady_clock::now() >= deadline) {
            LOGW_WARN(_logger,
                      L"The item was not synchronized in time - remoteNodeId=" << CommonUtility::s2ws(_request.remoteNodeId));
            _outcome = Outcome::ResolveTimedOut;

            return {ExitCode::SystemError, ExitCause::Unknown};
        }
    }
}

void OpenFileJob::sortSyncsByRunningState(std::vector<Sync> &syncs) const {
    std::ranges::stable_sort(syncs, [this](const Sync &sync1, const Sync &sync2) {
        const bool running1 = _getSyncPal(sync1.dbId()) != nullptr;
        if (const bool running2 = _getSyncPal(sync2.dbId()) != nullptr; running1 != running2) return running1;

        return sync1.dbId() < sync2.dbId();
    });
}

std::shared_ptr<SyncDb> OpenFileJob::getSyncDb(const SyncDbId syncDbId) const {
    std::shared_ptr<SyncPal> syncPal = _getSyncPal(syncDbId);

    return syncPal ? syncPal->syncDb() : nullptr;
}

ExitInfo OpenFileJob::getBestAdmissibleSync(const std::vector<Sync> &syncs, Sync &bestSync, SyncPath &relativePath, bool &found) {
    bestSync = Sync{}; // Default value as long as an admissible sync is not found.
    relativePath.clear();
    found = false;

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
                    LOGW_WARN(_logger,
                              L"Failed to open the sync DB - syncDbId=" << sync.dbId() << L": " << CommonUtility::s2ws(e.what()));
                    continue;
                }
            }
            syncDb = tempSyncDbIt->second;
        }

        bool nodeFound = false;
        if (!syncDb->path(ReplicaSide::Remote, _request.remoteNodeId, relativePath, nodeFound)) {
            LOGW_WARN(_logger, L"Error in SyncDb::path - syncDbId=" << sync.dbId());
            continue;
        }

        if (!nodeFound) continue;
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

    std::erase_if(syncs, [this](const Sync &sync) { return !_driveDbIds.contains(sync.driveDbId()); });

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

        return ExitCode::SystemError;
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
        LOGW_WARN(_logger, L"Error in Vfs::status - " << Utility::formatSyncPath(_localPath) << L": " << exitInfo);
        _outcome = Outcome::Failed;

        return ExitCode::SystemError;
    }

    if (!vfsStatus.isPlaceholder || vfsStatus.isHydrated || vfsStatus.isSyncing) {
        // Nothing to download, or a download is already ongoing: the system hydrates the file on access.
        return readyToOpen();
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

    const auto completion = std::make_shared<HydrationCompletion>();

    // Two callbacks are wired to the hydration job below, and they report two different events:
    //
    // - The additional callback (AbstractJob::setAdditionalCallback): one of the two job-completion callbacks offered
    //   by the job framework (alongside the main callback, invoked after it, unused here). It fires as soon as
    //   HydratePlaceHolderJob::runJob() has returned, on the thread that ran it, and reports the trigger outcome:
    //   whether the item checks passed and the download job was enqueued. Its signature carries no result, hence the
    //   weak_ptr used to read exitInfo() and downloadQueued() from the job. A weak_ptr is required because the
    //   callback is owned by the job: capturing a shared_ptr would create a reference cycle.
    //
    // - The observer (job constructor parameter, forwarded to SyncPal::addDlDirectJob): invoked by the engine when the
    //   download job completes, possibly long after the hydration job has run and even after this job has timed out or
    //   been aborted (the completion state is shared and kept alive by the shared_ptr). It reports the outcome of the
    //   actual download.
    const auto observer = [completion](UniqueId, bool success) {
        const std::scoped_lock lock(completion->mutex);
        completion->finished = true;
        completion->success = success;
        completion->cv.notify_all();
    };

    auto hydrationJob =
            std::make_shared<HydratePlaceHolderJob>(std::move(syncPal), _localPath, _relativePath, SyncPath{}, observer);
    std::weak_ptr<HydratePlaceHolderJob> weakHydrationJob = hydrationJob;
    hydrationJob->setAdditionalCallback([weakHydrationJob, completion](UniqueId) {
        if (const auto job = weakHydrationJob.lock()) {
            const std::scoped_lock lock(completion->mutex);
            completion->triggered = true;
            completion->triggerExitInfo = job->exitInfo();
            if (job->exitInfo() && !job->downloadQueued()) {
                // The job ran successfully but enqueued no download (e.g. the item got hydrated in the meantime):
                // there is nothing to wait for and the observer will never fire. Consider the hydration finished.
                completion->finished = true;
                completion->success = true;
            }
        }
        completion->cv.notify_all();
    });

    // Enqueue the hydration job: it checks the item state, sets the pin state to `AlwaysLocal` if needed (macOS), and
    // enqueues the download job (see ExtensionJob::commandMakeAvailableLocallyDirect).
    SyncJobManagerSingleton::instance()->queueAsyncJob(hydrationJob, Poco::Thread::PRIO_HIGH);

    const auto deadline = std::chrono::steady_clock::now() + kHydrationTimeout;
    std::unique_lock lock(completion->mutex);

    // Body of the two wait loops below. Checks for abort and deadline, then waits for a tick. Returns an Ok ExitInfo
    // to let the loop re-check its condition, or the ExitInfo to propagate when the job has been aborted or the
    // deadline has been reached.
    const auto handleWaitTick = [this, &completion, &lock, &deadline](const wchar_t *timeoutMessage) -> ExitInfo {
        if (isAborted()) {
            LOGW_WARN(_logger, L"Job aborted while hydrating the item - " << Utility::formatSyncPath(_localPath));
            _outcome = Outcome::Failed;

            return {ExitCode::OperationCanceled, ExitCause::OperationCanceled};
        }

        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
        if (remaining <= std::chrono::milliseconds::zero()) {
            LOGW_WARN(_logger, timeoutMessage << Utility::formatSyncPath(_localPath));
            _outcome = Outcome::HydrationTimedOut;

            return {ExitCode::SystemError, ExitCause::Unknown};
        }

        completion->cv.wait_for(lock, std::min(remaining, kHydrationWaitTick));

        return ExitCode::Ok;
    };

    // Wait for the hydration job to run.
    while (!completion->triggered && !completion->finished) {
        if (const auto exitInfo = handleWaitTick(L"The hydration did not complete in time - "); !exitInfo) return exitInfo;
    }

    if (completion->triggered && !completion->triggerExitInfo) {
        // The download could not be triggered: open the item anyway, the system hydrates it on access.
        LOGW_WARN(_logger, L"Error in HydratePlaceHolderJob - " << Utility::formatSyncPath(_localPath) << L": "
                                                                << completion->triggerExitInfo);
        return readyToOpen();
    }

    // Wait for the download to complete.
    while (!completion->finished) {
        if (const auto exitInfo = handleWaitTick(L"The download of the item did not complete in time - "); !exitInfo)
            return exitInfo;
    }

    if (!completion->success) {
        LOGW_WARN(_logger, L"The download of the item failed - " << Utility::formatSyncPath(_localPath));
        _outcome = Outcome::Failed;

        return {ExitCode::SystemError, ExitCause::Unknown};
    }

    LOGW_INFO(_logger, L"Item hydrated - " << Utility::formatSyncPath(_localPath));

    return readyToOpen();
}

ExitInfo OpenFileJob::readyToOpen() {
    _outcome = Outcome::ReadyToOpen;

    return ExitCode::Ok;
}

} // namespace KDC
