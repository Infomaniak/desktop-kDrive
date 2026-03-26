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

#include "remotefilesystemobserverworker.h"
#include "jobs/syncjobmanager.h"

#include "jobs/network/jobexceptions.h"

#include "jobs/network/kDrive_API/apitranslator.h"
#include "jobs/network/kDrive_API/listing/continuefilelistwithcursorjob.h"
#include "jobs/network/kDrive_API/listing/csvfullfilelistwithcursorjob.h"
#include "jobs/network/kDrive_API/listing/longpolljob.h"
#include "jobs/network/kDrive_API/getfileinfojob.h"

#if defined(KD_WINDOWS)
#include "reconciliation/platform_inconsistency_checker/platforminconsistencycheckerutility.h"
#endif
#include "libcommon/log/sentry/ptraces.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/utility/utility.h"
#include "requests/syncnodecache.h"
#include "requests/parameterscache.h"
#include "requests/exclusiontemplatecache.h"
#include "libcommonserver/utility/jsonparserutility.h"

#if defined(KD_MACOS)
#include "utility/utility.h"
#endif

#include "utility/timerutility.h"


#include <log4cplus/loggingmacros.h>

#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>
#include <Poco/Dynamic/Var.h>

namespace KDC {

RemoteFileSystemObserverWorker::RemoteFileSystemObserverWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name,
                                                               const std::string &shortName) :
    FileSystemObserverWorker(syncPal, name, shortName, ReplicaSide::Remote),
    _driveDbId(syncPal->driveDbId()) {}

RemoteFileSystemObserverWorker::~RemoteFileSystemObserverWorker() {
    LOG_SYNCPAL_DEBUG(_logger, "~RemoteFileSystemObserverWorker");
}

void RemoteFileSystemObserverWorker::execute() {
    ExitInfo exitInfo = ExitCode::Ok;
    LOG_SYNCPAL_DEBUG(_logger, "Worker started: name=" << name());

    // Sync loop
    for (;;) {
        if (stopAsked()) {
            exitInfo = ExitCode::Ok;
            tryToInvalidateSnapshot();
            break;
        }
        // We never pause this thread

        if (!_liveSnapshot.isValid()) {
            exitInfo = generateInitialSnapshot();
            if (!exitInfo) {
                LOG_SYNCPAL_DEBUG(_logger, "Error in generateInitialSnapshot: " << exitInfo);
                break;
            }
        }

        std::vector<std::string> mainDirectoriesRemoteIds;
        if (const auto mainDirectoriesExitInfo = getMainDirectoriesRemoteIds(mainDirectoriesRemoteIds);
            !mainDirectoriesExitInfo) {
            LOG_SYNCPAL_DEBUG(_logger, "Error in getMainDirectoriesRemoteIds: " << mainDirectoriesExitInfo);
            break;
        }
        for (const auto &remoteDirId: mainDirectoriesRemoteIds) {
            exitInfo = processEvents(remoteDirId);
            if (!exitInfo) {
                LOG_SYNCPAL_DEBUG(_logger, "Error in processEvents: remoteDirId=" << remoteDirId << ", ExitInfo: " << exitInfo);
                break;
            }
        }

        if (!exitInfo) break;

        _initializing = false;
        Utility::msleep(LOOP_EXEC_SLEEP_PERIOD);
    }
    LOG_SYNCPAL_DEBUG(_logger, "Worker stopped: name=" << name());
    setExitCause(exitInfo.cause());
    setDone(exitInfo.code());
}

ExitInfo RemoteFileSystemObserverWorker::generateInitialSnapshot() {
    ExitInfo exitInfo = ExitCode::Ok;

    LOG_SYNCPAL_INFO(_logger, "Starting remote snapshot generation");
    const auto start = std::chrono::steady_clock::now();
    sentry::pTraces::scoped::RFSOGenerateInitialSnapshot perfMonitor(syncDbId());

    // Retrieve the list of blacklisted folders.
    (void) SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), SyncNodeType::BlackList, _blackList);

    _liveSnapshot.init();
    _updating = true;
    countListingRequests();

    const auto end = std::chrono::steady_clock::now();
    const std::chrono::duration<double> elapsedSeconds = end - start;
    exitInfo = initWithCursor();
    if (exitInfo && !stopAsked()) {
        _liveSnapshot.setValid(true);
        LOG_SYNCPAL_INFO(_logger, "Remote snapshot generated in: " << elapsedSeconds.count() << "s for "
                                                                   << _liveSnapshot.nbItems() << " items");
        perfMonitor.stop();
    } else {
        tryToInvalidateSnapshot();
        LOG_SYNCPAL_WARN(_logger, "Remote snapshot generation stopped or failed after: " << elapsedSeconds.count() << "s");

        switch (exitInfo.code()) {
            case ExitCode::NetworkError:
                _syncPal->addError(Error(ERR_ID, exitInfo));
                break;
            case ExitCode::LogicError:
            case ExitCode::InvalidSync:
                _syncPal->addError(Error(_syncPal->syncDbId(), shortName(), exitInfo));
                break;
            default:
                break;
        }
    }
    _updating = false;

    return exitInfo;
}

ExitInfo RemoteFileSystemObserverWorker::processEvents(const NodeId &remoteDirId) {
    if (stopAsked()) return ExitCode::Ok;

    // Get last listing cursor used
    int64_t timestamp = 0;
    if (const auto exitInfo = listingCursor(remoteDirId, _listingCursorMap[remoteDirId], timestamp); !exitInfo) {
        LOG_SYNCPAL_WARN(_logger, "Error in RemoteFileSystemObserverWorker::listingCursor: " << exitInfo);
        return exitInfo;
    }

    if (!_updating && !_initializing) {
        // Send long poll request
        bool changes = false;
        if (const auto exitInfo = sendLongPoll(remoteDirId, changes); !exitInfo) {
            LOG_SYNCPAL_WARN(_logger, "Error in RemoteFileSystemObserverWorker::sendLongPoll: " << exitInfo);
            return exitInfo;
        }

        if (!changes) return ExitCode::Ok;
    }

    // Retrieve changes
    _updating = true;
    ExitInfo exitInfo = ExitCode::Ok;
    bool hasMore = true;
    while (hasMore) {
        hasMore = false;

        if (stopAsked()) {
            exitInfo = ExitCode::Ok;
            break;
        }

        std::shared_ptr<ContinueFileListWithCursorJob> job = nullptr;
        if (_listingCursorMap.at(remoteDirId).empty()) {
            LOG_SYNCPAL_WARN(_logger, "Cursor is empty for driveDbId=" << _driveDbId << ", invalidating remote snapshot.");
            tryToInvalidateSnapshot();
            exitInfo = ExitCode::DataError;
            break;
        }

        try {
            job = std::make_shared<ContinueFileListWithCursorJob>(_driveDbId, remoteDirId, _listingCursorMap[remoteDirId],
                                                                  _blackList);
        } catch (const std::exception &e) {
            LOG_SYNCPAL_WARN(_logger, "Error in ContinueFileListWithCursorJob::ContinueFileListWithCursorJob for driveDbId="
                                              << _driveDbId << " error=" << e.what());
            exitInfo = exception2ExitCode(e);
            break;
        }

        exitInfo = job->runSynchronously();
        if (!exitInfo) {
            LOG_SYNCPAL_WARN(_logger, "Error in ContinuousCursorListingJob: " << exitInfo);
            break;
        }

        Poco::JSON::Object::Ptr resObj = job->jsonRes();
        if (!resObj) {
            LOG_SYNCPAL_WARN(_logger, "Continue cursor listing request failed for drive: " << std::to_string(_driveDbId).c_str()
                                                                                           << " and cursor: "
                                                                                           << _listingCursorMap[remoteDirId]);
            exitInfo = ExitCode::BackError;
            break;
        }

        if (job->hasErrorApi()) {
            if (getNetworkErrorCode(job->backError().code()) == NetworkErrorCode::ForbiddenError) {
                LOG_SYNCPAL_WARN(_logger, "Access forbidden");
                exitInfo = ExitCode::Ok;
                break;
            } else {
                std::ostringstream os;
                resObj->stringify(os);
                LOGW_SYNCPAL_WARN(_logger, L"Continue cursor listing request failed: " << CommonUtility::s2ws(os.str()));
                exitInfo = ExitCode::BackError;
                break;
            }
        }

        Poco::JSON::Object::Ptr dataObj = resObj->getObject(dataKey);
        if (!dataObj) continue;

        std::string cursor;
        if (!JsonParserUtility::extractValue(resObj, cursorKey, cursor)) {
            exitInfo = ExitCode::BackError;
            break;
        }

        if (cursor != _listingCursorMap[remoteDirId]) {
            _listingCursorMap[remoteDirId] = cursor;
            LOG_SYNCPAL_DEBUG(_logger, "Sync cursor updated: " << _listingCursorMap[remoteDirId]);
            const auto cursorTimestamp = static_cast<int64_t>(time(0));
            exitInfo = saveListingCursor(remoteDirId, _listingCursorMap[remoteDirId], cursorTimestamp);
            if (!exitInfo) {
                LOG_SYNCPAL_WARN(_logger, "Error in RemoteFileSystemObserverWorker::saveListingCursor: " << exitInfo);
                break;
            }
        }

        if (!JsonParserUtility::extractValue(resObj, hasMoreKey, hasMore)) {
            exitInfo = ExitCode::BackError;
            break;
        }

        // Look for new actions
        exitInfo = processActions(dataObj->getArray(actionsKey), dataObj->getArray(actionsFilesKey));
        if (!exitInfo) {
            LOG_SYNCPAL_WARN(_logger, "Error in RemoteFileSystemObserverWorker::processActions: " << exitInfo);
            tryToInvalidateSnapshot();
            break;
        }
    }

    _updating = false;

    return exitInfo;
}

ExitInfo RemoteFileSystemObserverWorker::updateV3MainFolderItem(const RemoteNodeId &remoteNodeId) {
    SyncName folderName;
    if (const auto exitInfo = getV3RemoteFolderName(remoteNodeId, folderName); !exitInfo) {
        LOGW_SYNCPAL_WARN(_logger, L"Error in RemoteFileSystemObserverWorker::getV3RemoteFolderName: remoteNodeId="
                                           << CommonUtility::s2ws(remoteNodeId) << L", exitInfo=" << exitInfo);
        return exitInfo;
    }

    // As long as the local folder hierarchy is reflects the v2 API
    // we should not create a Private folder at the root of the local synchronization folder.
    if (folderName == Str("Private")) return ExitCode::Ok;

    DbNode dbNode;
    bool found = false;
    if (!_syncDb->node(ReplicaSide::Remote, remoteNodeId, dbNode, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::node");
        return ExitCode::DbError;
    }

    SnapshotItem remoteSnapshotItem(remoteNodeId);
    const SyncTime now = std::chrono::steady_clock::now().time_since_epoch().count();
    remoteSnapshotItem.setType(NodeType::Directory);
    remoteSnapshotItem.setName(folderName);
    remoteSnapshotItem.setCreatedAt(now);
    remoteSnapshotItem.setLastModified(now);
    if (const auto exitInfo = remoteSnapshotItem.setParentId(_driveDbId, ApiTranslator::v2RootFolderRemoteId()); !exitInfo)
        return exitInfo;
    if (found) {
        assert(dbNode.type() == NodeType::Directory && "Invalid node type.");
        remoteSnapshotItem.setCreatedAt(dbNode.created().value_or(now));
        remoteSnapshotItem.setLastModified(dbNode.lastModified(ReplicaSide::Remote));
    }

    if (!_liveSnapshot.updateItem(remoteSnapshotItem)) {
        LOGW_SYNCPAL_WARN(_logger, L"Failed to insert item: " << Utility::formatSyncName(remoteSnapshotItem.name()) << L" ("
                                                              << CommonUtility::s2ws(remoteSnapshotItem.id()) << L")");
        tryToInvalidateSnapshot();
        return ExitCode::DataError;
    }

    return ExitCode::Ok;
}

ExitInfo RemoteFileSystemObserverWorker::initWithCursor() {
    if (stopAsked()) return ExitCode::Ok;

    std::vector<std::string> mainDirectoriesRemoteIds;
    if (const auto mainDirectoriesExitInfo = getMainDirectoriesRemoteIds(mainDirectoriesRemoteIds); !mainDirectoriesExitInfo) {
        LOG_SYNCPAL_DEBUG(_logger, "Error in getMainDirectoriesRemoteIds: " << mainDirectoriesExitInfo);
        return mainDirectoriesExitInfo;
    }

    ExitInfo exitInfo;
    for (const auto &mainFolderRemoteId: mainDirectoriesRemoteIds) {
        if (_blackList.contains(mainFolderRemoteId)) continue;

        updateV3MainFolderItem(mainFolderRemoteId);

        constexpr bool saveCursor = true;
        exitInfo = getItemsInDir(mainFolderRemoteId, saveCursor);
        if (!exitInfo) return exitInfo;
    }

    deleteOrphans();

    return ExitCode::Ok;
}

ExitInfo RemoteFileSystemObserverWorker::exploreDirectory(const NodeId &nodeId) {
    if (stopAsked()) return ExitCode::Ok;

    constexpr bool saveCursor = false;

    return getItemsInDir(nodeId, saveCursor);
}

ExitInfo RemoteFileSystemObserverWorker::handleSnapshotItem(
        const SnapshotItem &item, SyncNameSet &existingFiles, ParsingIterationState &iterationState,
        sentry::pTraces::counterScoped::RFSOExploreItem &itemHandlingMonitor) {
    if (iterationState.ignore) return ExitCode::Ok; // continue
    if (iterationState.eof) return ExitCode::Ok; // break

    itemHandlingMonitor.start();

    ++iterationState.itemCount;
    if (iterationState.error) {
        LOG_SYNCPAL_WARN(_logger, "Logic error: failed to parse CSV reply.");

        return {ExitCode::LogicError, ExitCause::FullListParsingError};
    }

    if (stopAsked()) return ExitCode::Ok;

    if (bool isWarning = false; ExclusionTemplateCache::instance()->isExcluded(item.name(), isWarning)) {
        return ExitCode::Ok;
    }

    // Check unsupported characters
    if (const auto exitInfo = checkForUnsupportedCharacters(item.name(), item.id(), item.type()); !exitInfo) {
        if (exitInfo.cause() == ExitCause::TmpDirAccessError) return exitInfo;

        return ExitCode::Ok;
    }

    if (const auto &[_, inserted] = existingFiles.insert(Str2SyncName(item.parentId()) + item.name()); !inserted) {
        // An item with the exact same name already exists in the parent folder.
        LOGW_SYNCPAL_DEBUG(_logger, L"Item with " << Utility::formatSyncName(item.name()) << L" already exists in directory with "
                                                  << Utility::formatSyncName(_liveSnapshot.name(item.parentId())));

        SyncPath path;
        _liveSnapshot.path(item.parentId(), path, iterationState.ignore);
        path /= item.name();

        Error err(_syncPal->syncDbId(), "", item.id(), NodeType::Directory, path, ConflictType::None, InconsistencyType::None,
                  CancelType::TmpBlacklisted);
        _syncPal->addError(err);

        return ExitCode::Ok;
    }

    if (_liveSnapshot.updateItem(item) && ParametersCache::isExtendedLogEnabled()) {
        assert(!item.name().empty());
        LOGW_SYNCPAL_DEBUG(_logger, L"Item inserted in remote snapshot: name:"
                                            << Utility::quotedSyncName(item.name()) << L", inode:"
                                            << CommonUtility::s2ws(item.id()) << L", parent inode:"
                                            << CommonUtility::s2ws(item.parentId()) << L", createdAt:" << item.createdAt()
                                            << L", modtime:" << item.lastModified() << L", isDir:"
                                            << (item.type() == NodeType::Directory) << L", size:" << item.size() << L", isLink:"
                                            << item.isLink());
    }

    return ExitCode::Ok;
}

void RemoteFileSystemObserverWorker::deleteOrphans() {
    NodeSet nodeIds;
    _liveSnapshot.ids(nodeIds);

    for (auto nodeIdIt = nodeIds.begin(); nodeIdIt != nodeIds.end(); ++nodeIdIt) {
        if (!_liveSnapshot.isOrphan(*nodeIdIt)) continue;

        LOGW_SYNCPAL_DEBUG(_logger, L"Node with " << Utility::formatSyncName(_liveSnapshot.name(*nodeIdIt)) << L" ("
                                                  << CommonUtility::s2ws(*nodeIdIt) << L") is an orphan. Removing it from "
                                                  << _liveSnapshot.side() << L" snapshot.");
        _liveSnapshot.removeItem(*nodeIdIt);
    }
}

ExitInfo RemoteFileSystemObserverWorker::getItemsInDir(const NodeId &remoteDirId, const bool saveCursor) {
    // Send request
    sentry::pTraces::scoped::RFSOBackRequest perfMonitorBackRequest(!saveCursor, syncDbId());
    std::shared_ptr<CsvFullFileListWithCursorJob> job = nullptr;
    try {
        constexpr bool zip = true;
        job = std::make_shared<CsvFullFileListWithCursorJob>(_driveDbId, remoteDirId, _blackList, zip);
    } catch (const std::exception &e) {
        LOG_SYNCPAL_WARN(_logger, "Error in CsvFullFileListWithCursorJob::CsvFullFileListWithCursorJob for driveDbId="
                                          << _driveDbId << " error=" << e.what());
        return exception2ExitCode(e);
    }

    SyncJobManagerSingleton::instance()->queueAsyncJob(job, Poco::Thread::PRIO_LOW);
    while (!SyncJobManagerSingleton::instance()->isJobFinished(job->jobId())) {
        if (stopAsked()) return ExitCode::Ok;
        Utility::msleep(100);
    }

    if (!job->exitInfo()) {
        LOG_SYNCPAL_WARN(_logger, "Error in CsvFullFileListWithCursorJob: " << job->exitInfo());
        if (job->exitInfo().code() == ExitCode::RateLimited) {
            setPauseDuration(job->sleepDuration());
        }

        return job->exitInfo();
    }

    if (saveCursor) {
        if (const auto &cursor = job->getCursor();
            !_listingCursorMap.contains(remoteDirId) || cursor != _listingCursorMap[remoteDirId]) {
            _listingCursorMap[remoteDirId] = cursor;
            LOG_SYNCPAL_DEBUG(_logger, "Cursor updated: " << _listingCursorMap[remoteDirId]);
            const auto cursorTimestamp = static_cast<int64_t>(time(0));
            if (const ExitInfo exitInfo = saveListingCursor(remoteDirId, _listingCursorMap[remoteDirId], cursorTimestamp);
                !exitInfo) {
                LOG_SYNCPAL_WARN(_logger, "Error in RemoteFileSystemObserverWorker::saveListingCursor");

                return exitInfo;
            }
        }
    }

    // Parse reply
    LOG_SYNCPAL_DEBUG(_logger, "Start parsing of the CSV reply.");
    const TimerUtility timer;
    SnapshotItem item;
    SyncNameSet existingFiles;
    ParsingIterationState iterationState;

    perfMonitorBackRequest.stop();

    sentry::pTraces::counterScoped::RFSOExploreItem itemHandlingMonitor(!saveCursor, syncDbId());
    while (job->getItem(item, iterationState.error, iterationState.ignore, iterationState.eof)) {
        if (const auto exitInfo = handleSnapshotItem(item, existingFiles, iterationState, itemHandlingMonitor); !exitInfo) {
            return exitInfo;
        }

        if (iterationState.eof) break;
        if (stopAsked()) return ExitCode::Ok;
    }

    if (!iterationState.eof) {
        constexpr auto msg = "Failed to parse CSV reply: missing EOF delimiter";
        LOG_SYNCPAL_WARN(_logger, msg);
        sentry::Handler::captureMessage(sentry::Level::Warning, "RemoteFileSystemObserverWorker::getItemsInDir", msg);

        return {ExitCode::NetworkError, ExitCause::FullListParsingError};
    }

    LOG_SYNCPAL_DEBUG(_logger, "End reply parsing in " << timer.elapsed<DoubleSeconds>().count() << "s for "
                                                       << iterationState.itemCount << " items");

    return ExitCode::Ok;
}

ExitInfo RemoteFileSystemObserverWorker::sendLongPoll(const RemoteNodeId &remoteDirId, bool &changes) {
    // Temporary mocks the long poll request as `listing/longpoll` does not exist in the API v3 yet.
    changes = true;

    return ExitCode::Ok;

    // Actual implementation
    changes = false;
    if (!_liveSnapshot.isValid()) return ExitCode::Ok;

    std::shared_ptr<LongPollJob> notifyJob = nullptr;
    auto timestamp = 0;
    if (const auto exitInfo = listingCursor(remoteDirId, _listingCursorMap.at(remoteDirId), timestamp); !exitInfo) {
        LOG_SYNCPAL_WARN(_logger, "Error in RemoteFileSystemObserverWorker::listingCursor: " << exitInfo);
        return exitInfo;
    }
    try {
        notifyJob = std::make_shared<LongPollJob>(_driveDbId, _listingCursorMap.at(remoteDirId));
    } catch (const std::exception &e) {
        LOG_SYNCPAL_WARN(_logger, "Error in LongPollJob::LongPollJob for driveDbId=" << _driveDbId << " error=" << e.what());
        return exception2ExitCode(e);
    }

    SyncJobManagerSingleton::instance()->queueAsyncJob(notifyJob, Poco::Thread::PRIO_LOW);
    while (!SyncJobManagerSingleton::instance()->isJobFinished(notifyJob->jobId())) {
        if (stopAsked()) {
            LOG_DEBUG(_logger, "Request " << notifyJob->jobId() << ": aborting LongPoll job");
            notifyJob->abort();

            return ExitCode::Ok;
        }

        {
            const std::scoped_lock lock(_mutex);
            if (_updating) { // We want to update snapshot immediately, cancel LongPoll job and send a listing/continue
                             // request
                notifyJob->abort();
                changes = true;

                return ExitCode::Ok;
            }
        }

        // Wait until job finished
        Utility::msleep(100);
    }

    if (!notifyJob->exitInfo()) {
        LOG_SYNCPAL_WARN(_logger, "Error in LongPollJob: " << notifyJob->exitInfo()
                                                           << " for drive: " << std::to_string(_driveDbId)
                                                           << " and cursor: " << _listingCursorMap.at(remoteDirId));

        if (notifyJob->exitInfo() == ExitInfo(ExitCode::NetworkError, ExitCause::NetworkTimeout)) {
            _syncPal->addError(Error(_syncPal->syncDbId(), ERR_ID, notifyJob->exitInfo()));
        }

        return notifyJob->exitInfo();
    }

    Poco::JSON::Object::Ptr resObj = notifyJob->jsonRes();
    if (!resObj) {
        // If error, fall
        LOG_SYNCPAL_DEBUG(_logger,
                          "Notify changes request failed for drive: " << std::to_string(_driveDbId).c_str()
                                                                      << " and cursor: " << _listingCursorMap.at(remoteDirId));
        return {ExitCode::BackError, ExitCause::ApiErr};
    }

    if (!JsonParserUtility::extractValue(resObj, changesKey, changes)) {
        return {ExitCode::BackError, ExitCause::ApiErr};
    }

    return ExitCode::Ok;
}

ExitInfo RemoteFileSystemObserverWorker::createActionInfoList(const Poco::JSON::Array::Ptr actionArray,
                                                              ActionInfoList &actionInfoList) {
    actionInfoList.clear();
    for (auto it = actionArray->begin(); it != actionArray->end(); ++it) {
        if (stopAsked()) return ExitCode::Ok;

        const auto actionObj = it->extract<Poco::JSON::Object::Ptr>();
        ActionInfo actionInfo;
        if (const auto exitInfo = extractActionInfo(actionObj, actionInfo); !exitInfo) return exitInfo;

        // The relative path "Private" from API v3 is translated into the empty relative path "" via ActionInfo::setPath.
        // We do not record the ActionInfo in this case.
        if (actionInfo.path().empty()) continue;

        bool isWarning = false;
        if (ExclusionTemplateCache::instance()->isExcluded(actionInfo.snapshotItem.name(), isWarning)) {
            if (isWarning) {
                Error error(_syncPal->syncDbId(), "", actionInfo.snapshotItem.id(), actionInfo.snapshotItem.type(),
                            actionInfo.path(), ConflictType::None, InconsistencyType::None, CancelType::ExcludedByTemplate);
                _syncPal->addError(error);
            }
            // Remove it from liveSnapshot
            _liveSnapshot.removeItem(actionInfo.snapshotItem.id());
            continue;
        }

#if defined(KD_WINDOWS)
        SyncName newName;
        if (PlatformInconsistencyCheckerUtility::instance()->fixNameWithBackslash(actionInfo.snapshotItem.name(), newName)) {
            actionInfo.snapshotItem.setName(newName);
        }
#endif
        actionInfoList.push_back(actionInfo);
    }

    return ExitCode::Ok;
}

ExitInfo RemoteFileSystemObserverWorker::fillActionsFilesInfo(const Poco::JSON::Array::Ptr actionFilesArray,
                                                              ActionInfoList &actionInfoList) {
    assert(actionFilesArray && "Expected 'actions_files' array. Got nullptr.");

    for (auto it = actionFilesArray->begin(); it != actionFilesArray->end(); ++it) {
        if (stopAsked()) return ExitCode::Ok;

        const auto actionFileObj = it->extract<Poco::JSON::Object::Ptr>();
        if (const auto exitInfo = extractActionFileInfo(actionFileObj, actionInfoList); !exitInfo) return exitInfo;
    }

    return ExitCode::Ok;
}

ExitInfo RemoteFileSystemObserverWorker::processActions(const Poco::JSON::Array::Ptr actionArray,
                                                        const Poco::JSON::Array::Ptr actionsFilesArray) {
    if (!actionArray) return ExitCode::Ok;

    ActionInfoList actionInfoList;
    if (const auto exitInfo = createActionInfoList(actionArray, actionInfoList); !exitInfo) return exitInfo;
    if (const auto exitInfo = fillActionsFilesInfo(actionsFilesArray, actionInfoList); !exitInfo) return exitInfo;

    // Filter out items whose names contain unsupported characters.
    for (auto it = actionInfoList.begin(); it != actionInfoList.end();) {
        const auto &actionInfo = *it;
        if (const auto exitInfo = checkForUnsupportedCharacters(actionInfo.snapshotItem.name(), actionInfo.snapshotItem.id(),
                                                                actionInfo.snapshotItem.type());
            !exitInfo) {
            if (exitInfo.cause() == ExitCause::TmpDirAccessError) return exitInfo;
            it = actionInfoList.erase(it);
        } else
            ++it;
    }

    MoveItemMap movedItems;
    for (auto &actionInfo: actionInfoList) {
        sentry::pTraces::scoped::RFSOChangeDetected perfMonitor(syncDbId());
        if (const auto exitInfo = processAction(actionInfo, movedItems); !exitInfo) {
            LOG_SYNCPAL_WARN(_logger, "Error in RemoteFileSystemObserverWorker::processAction: " << exitInfo);
            return exitInfo;
        }
    }

    for (const auto &[nodeId, actionCode]: movedItems) {
        // Items remaining in "movedItems" have been either blacklisted or whitelisted.
        switch (actionCode) {
            case ActionCode::ActionCodeMoveIn: {
                if (_liveSnapshot.type(nodeId) != NodeType::Directory) break; // Do not explore if moved item is a file
                const sentry::pTraces::scoped::RFSOChangeDetected perfMonitor(syncDbId());
                if (const auto exitInfo = exploreDirectory(nodeId); !exitInfo) return exitInfo;
                break;
            }
            case ActionCode::ActionCodeMoveOut: {
                if (const auto exitInfo = removeItemFromSnapshot(nodeId); !exitInfo) return exitInfo;
                break;
            }
            default:
                break;
        }
    }

    return ExitCode::Ok;
}

ExitInfo RemoteFileSystemObserverWorker::extractActionInfo(const Poco::JSON::Object::Ptr actionObj, ActionInfo &actionInfo) {
    std::string tmpStr;
    if (!JsonParserUtility::extractValue(actionObj, actionKey, tmpStr)) {
        return ExitCode::BackError;
    }
    actionInfo.actionCode = getActionCode(tmpStr);

    RemoteFileId tmpInt = 0;
    if (!JsonParserUtility::extractValue(actionObj, fileIdKey, tmpInt)) {
        return ExitCode::BackError;
    }
    if (const auto exitInfo = actionInfo.snapshotItem.setId(_driveDbId, std::to_string(tmpInt)); !exitInfo) return exitInfo;

    if (!JsonParserUtility::extractValue(actionObj, parentIdKey, tmpInt)) {
        return ExitCode::BackError;
    }
    if (const auto exitInfo = actionInfo.snapshotItem.setParentId(_driveDbId, std::to_string(tmpInt)); !exitInfo) return exitInfo;

    SyncName tmpDestPathStr;
    if (!JsonParserUtility::extractValue(actionObj, destinationKey, tmpDestPathStr, false)) {
        return ExitCode::BackError;
    }
    if (!tmpDestPathStr.empty()) {
        // This a move operation. Get the name from the `destination` field
        actionInfo.snapshotItem.setName(tmpDestPathStr.substr(tmpDestPathStr.find_last_of('/') + 1)); // +1 to ignore the last "/"
        actionInfo.setPath(tmpDestPathStr);
    } else {
        // Otherwise, get the name from the `path` field
        SyncName actionPath;
        if (!JsonParserUtility::extractValue(actionObj, pathKey, actionPath)) {
            return ExitCode::BackError;
        }
        actionInfo.setPath(actionPath);
        actionInfo.snapshotItem.setName(
                actionInfo.path().substr(actionInfo.path().find_last_of('/') + 1)); // +1 to ignore the last "/"
    }

    SyncTime tmpTime = 0;
    if (!JsonParserUtility::extractValue(actionObj, createdAtKey, tmpTime, false)) {
        return ExitCode::BackError;
    }
    actionInfo.snapshotItem.setCreatedAt(tmpTime);

    if (!JsonParserUtility::extractValue(actionObj, lastModifiedAtKey, tmpTime, false)) {
        return ExitCode::BackError;
    }
    actionInfo.snapshotItem.setLastModified(tmpTime);

    return ExitCode::Ok;
}

ExitInfo RemoteFileSystemObserverWorker::extractActionFileInfo(const Poco::JSON::Object::Ptr actionFileObj,
                                                               ActionInfoList &actionInfoList) {
    RemoteFileId fileId = 0;
    if (!JsonParserUtility::extractValue(actionFileObj, idKey, fileId)) return ExitCode::BackError;

    std::string fileTypeString;
    if (!JsonParserUtility::extractValue(actionFileObj, typeKey, fileTypeString)) return ExitCode::BackError;

    uint64_t fileSize = 0;
    if (fileTypeString == fileKey && !JsonParserUtility::extractValue(actionFileObj, sizeKey, fileSize, false))
        return ExitCode::BackError;

    SyncTime createdAtTime = 0;
    if (!JsonParserUtility::extractValue(actionFileObj, createdAtKey, createdAtTime, false)) return ExitCode::BackError;

    SyncTime lastModifiedAtTime = 0;
    if (!JsonParserUtility::extractValue(actionFileObj, lastModifiedAtKey, lastModifiedAtTime, false)) return ExitCode::BackError;

    std::string symbolicKeyString;
    if (!JsonParserUtility::extractValue(actionFileObj, symbolicLinkKey, symbolicKeyString, false)) return ExitCode::BackError;

    bool canWrite = false;
    if (auto capabilitiesObj = actionFileObj->getObject(capabilitiesKey); capabilitiesObj) {
        if (!JsonParserUtility::extractValue(capabilitiesObj, canWriteKey, canWrite)) return ExitCode::BackError;
    }

    for (auto &actionInfo: actionInfoList) {
        if (actionInfo.snapshotItem.id() != std::to_string(fileId)) continue;

        actionInfo.snapshotItem.setType(fileTypeString == fileKey ? NodeType::File : NodeType::Directory);
        actionInfo.snapshotItem.setSize(static_cast<int64_t>(fileSize));
        actionInfo.snapshotItem.setCreatedAt(createdAtTime);
        actionInfo.snapshotItem.setLastModified(lastModifiedAtTime);
        actionInfo.snapshotItem.setIsLink(!symbolicKeyString.empty());
        actionInfo.snapshotItem.setCanWrite(canWrite);
    }

    return ExitCode::Ok;
}

ExitInfo RemoteFileSystemObserverWorker::processAction(ActionInfo &actionInfo, MoveItemMap &movedItems) {
    _syncPal->removeItemFromTmpBlacklist(actionInfo.snapshotItem.id(), ReplicaSide::Remote);

    // Process action
    switch (actionInfo.actionCode) {
        // Item added
        case ActionCode::ActionCodeAccessRightInsert:
        case ActionCode::ActionCodeAccessRightUpdate:
        case ActionCode::ActionCodeAccessRightUserInsert:
        case ActionCode::ActionCodeAccessRightUserUpdate:
        case ActionCode::ActionCodeAccessRightTeamInsert:
        case ActionCode::ActionCodeAccessRightTeamUpdate:
        case ActionCode::ActionCodeAccessRightMainUsersInsert:
        case ActionCode::ActionCodeAccessRightMainUsersUpdate: {
            bool rightsOk = false;
            if (const ExitInfo exitInfo =
                        checkRightsAndUpdateItem(actionInfo.snapshotItem.id(), rightsOk, actionInfo.snapshotItem);
                !exitInfo) {
                return exitInfo;
            }
            if (!rightsOk) break; // Current user does not have the right to access this item, ignore action.
            [[fallthrough]];
        }
        case ActionCode::ActionCodeMoveIn:
        case ActionCode::ActionCodeRestore:
        case ActionCode::ActionCodeCreate:
        case ActionCode::ActionCodeRename: {
            const bool directoryExplorationRequired =
                    isDirectoryExplorationRequired(actionInfo, movedItems); // Must be called before updateItem
            (void) _liveSnapshot.updateItem(actionInfo.snapshotItem);
            if (directoryExplorationRequired) {
                // Retrieve all children
                const auto exitInfo = exploreDirectory(actionInfo.snapshotItem.id());
                switch (exitInfo.code()) {
                    case ExitCode::NetworkError:
                        if (exitCause() == ExitCause::NetworkTimeout) {
                            _syncPal->addError(Error(_syncPal->syncDbId(), ERR_ID, exitInfo));
                        }
                        break;
                    case ExitCode::LogicError:
                        if (exitCause() == ExitCause::FullListParsingError) {
                            _syncPal->addError(Error(_syncPal->syncDbId(), shortName(), exitInfo));
                        }
                        break;
                    default:
                        break;
                }

                if (!exitInfo) return exitInfo;
            }
            if (actionInfo.actionCode == ActionCode::ActionCodeMoveIn) {
                keepTrackOfMovedItem(actionInfo, movedItems);
            }
            break;
        }

        case ActionCode::ActionCodeMoveOut:
            keepTrackOfMovedItem(actionInfo, movedItems);
            break;

        // Item edited
        case ActionCode::ActionCodeEdit:
            (void) _liveSnapshot.updateItem(actionInfo.snapshotItem);
            break;

        // Item removed
        case ActionCode::ActionCodeAccessRightRemove:
        case ActionCode::ActionCodeAccessRightUserRemove:
        case ActionCode::ActionCodeAccessRightTeamRemove:
        case ActionCode::ActionCodeAccessRightMainUsersRemove: {
            bool rightsOk = false;
            if (const ExitInfo exitInfo =
                        checkRightsAndUpdateItem(actionInfo.snapshotItem.id(), rightsOk, actionInfo.snapshotItem);
                !exitInfo) {
                return exitInfo;
            }
            if (rightsOk) break; // Current user still have the right to access this item, ignore action.
            [[fallthrough]];
        }
        case ActionCode::ActionCodeTrash:
            if (const auto exitInfo = removeItemFromSnapshot(actionInfo.snapshotItem.id()); !exitInfo) return exitInfo;
            break;

        // Ignored actions
        case ActionCode::ActionCodeAccess:
        case ActionCode::ActionCodeDelete:
        case ActionCode::ActionCodeRestoreFileShareCreate:
        case ActionCode::ActionCodeRestoreFileShareDelete:
        case ActionCode::ActionCodeRestoreShareLinkCreate:
        case ActionCode::ActionCodeRestoreShareLinkDelete:
            // Ignore these actions
            break;

        default:
            LOGW_SYNCPAL_DEBUG(_logger, L"Unknown operation received on item with "
                                                << Utility::formatSyncName(actionInfo.snapshotItem.name()) << L" ("
                                                << CommonUtility::s2ws(actionInfo.snapshotItem.id()) << L")");
    }

    return ExitCode::Ok;
}

void RemoteFileSystemObserverWorker::keepTrackOfMovedItem(const ActionInfo &actionInfo, MoveItemMap &movedItems) const {
    if (!movedItems.contains(actionInfo.snapshotItem.id())) {
        // Keep track of moved items
        (void) movedItems.try_emplace(actionInfo.snapshotItem.id(), actionInfo.actionCode);
    } else {
        // Move out event already received, no need to keep track anymore
        (void) movedItems.erase(actionInfo.snapshotItem.id());
    }
}

bool RemoteFileSystemObserverWorker::isDirectoryExplorationRequired(const ActionInfo &actionInfo,
                                                                    const MoveItemMap &movedItems) const {
    if (actionInfo.snapshotItem.type() != NodeType::Directory) return false;
    if (_liveSnapshot.exists(actionInfo.snapshotItem.id()) || movedItems.contains(actionInfo.snapshotItem.id())) {
        // The item is already synchronized, exploration is not required.
        return false;
    }

    if (actionInfo.actionCode == ActionCode::ActionCodeCreate) {
        // Create events are sent on each child, exploration is not required.
        return false;
    }

    if (actionInfo.actionCode == ActionCode::ActionCodeMoveIn) {
        // Explore directory for move_in actions is managed later.
        return false;
    }

    return true;
}

ExitInfo RemoteFileSystemObserverWorker::removeItemFromSnapshot(const NodeId &id) {
    if (_liveSnapshot.removeItem(id)) return ExitCode::Ok;

    LOG_SYNCPAL_WARN(_logger, "Fail to remove item for ID: " << id);
    tryToInvalidateSnapshot();
    return ExitCode::BackError;
}

ExitInfo RemoteFileSystemObserverWorker::checkRightsAndUpdateItem(const NodeId &nodeId, bool &hasRights,
                                                                  SnapshotItem &snapshotItem) {
    std::unique_ptr<GetFileInfoJob> job;
    try {
        job = std::make_unique<GetFileInfoJob>(_syncPal->driveDbId(), nodeId);
    } catch (const std::exception &e) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in GetFileInfoJob::GetFileInfoJob for driveDbId=" << _syncPal->driveDbId() << " nodeId=" << nodeId.c_str()
                                                                          << " error=" << e.what());
        return exception2ExitCode(e);
    }

    job->runSynchronously();
    if (job->hasHttpError() || !job->exitInfo()) {
        if (job->getStatusCode() == Poco::Net::HTTPResponse::HTTP_FORBIDDEN ||
            job->getStatusCode() == Poco::Net::HTTPResponse::HTTP_NOT_FOUND) {
            hasRights = false;

            return ExitCode::Ok;
        }

        LOGW_SYNCPAL_WARN(_logger, L"Error while determining access rights on item: "
                                           << SyncName2WStr(snapshotItem.name()) << L" ("
                                           << CommonUtility::s2ws(snapshotItem.id()) << L")");
        tryToInvalidateSnapshot();

        return ExitCode::BackError;
    }

    snapshotItem.setCreatedAt(job->creationTime());
    snapshotItem.setLastModified(job->modificationTime());
    snapshotItem.setSize(job->size());
    snapshotItem.setIsLink(job->isLink());

    hasRights = true;

    return ExitCode::Ok;
}

ExitInfo RemoteFileSystemObserverWorker::checkForUnsupportedCharacters(const SyncName &name, const NodeId &nodeId,
                                                                       const NodeType type) {
    ExitInfo exitInfo = ExitCode::Ok;
#if defined(KD_MACOS)
    // Check that the name doesn't contain a character not yet supported by the filesystem (ex: U+1FA77 on pre macOS 13.4).
    switch (type) {
        case NodeType::File:
            exitInfo = Utility::tryCreateTmpFile(_syncPal->cacheDirectory(), name);
            break;
        case NodeType::Directory:
            exitInfo = Utility::tryCreateTmpDir(_syncPal->cacheDirectory(), name);
            break;
        default:
            LOG_MSG_IF_FAIL(false, "Unknown NodeType.");
            exitInfo = ExitCode::LogicError;
    }

    if (exitInfo.cause() == ExitCause::TmpDirAccessError) {
        LOGW_SYNCPAL_ERROR(_logger, L"Cannot access tmp directory.");
        _syncPal->addError(Error(ERR_ID, exitInfo.code(), exitInfo.cause()));
    } else if (!exitInfo) {
        LOGW_SYNCPAL_DEBUG(_logger,
                           L"The file name or directory name contains a character not yet supported by the filesystem: exitInfo="
                                   << exitInfo << L", " << Utility::formatSyncName(name) << L". Item is ignored.");
        _syncPal->addError(
                Error(_syncPal->syncDbId(), "", nodeId, type, name, ConflictType::None, InconsistencyType::NotYetSupportedChar));
        exitInfo = {ExitCode::SystemError, ExitCause::InvalidName};
    }
#else
    (void) name;
    (void) nodeId;
    (void) type;
#endif
    return exitInfo;
}

void RemoteFileSystemObserverWorker::countListingRequests() {
    const auto listingFullTimerEnd = std::chrono::steady_clock::now();
    const std::chrono::duration<double> elapsedTime = listingFullTimerEnd - _listingFullTimer;
    bool resetTimer = elapsedTime.count() > 3600; // 1h
    if (_listingFullCounter > 60) {
        // If there is more then 1 listing/full request per minute for an hour -> send a sentry
        sentry::Handler::captureMessage(sentry::Level::Warning, "RemoteFileSystemObserverWorker::generateInitialSnapshot",
                                        "Too many listing/full requests, sync is looping");
        resetTimer = true;
    }

    if (resetTimer) {
        _listingFullCounter = 0;
        _listingFullTimer = listingFullTimerEnd;
    }

    _listingFullCounter++;
}

void RemoteFileSystemObserverWorker::ActionInfo::setPath(const KDC::SyncName &remotePath) {
    SyncPath remotePath_{remotePath};
    ApiTranslator::translateV3ToV2(remotePath_);
    _path = remotePath_.string();
}

ExitInfo RemoteFileSystemObserverWorker::getMainDirectoriesRemoteIds(std::vector<RemoteNodeId> &mainDirectoriesRemoteIds) const {
    mainDirectoriesRemoteIds.clear();

    RemoteNodeId userPrivateFolderRemoteId;
    if (const auto exitInfo = ApiTranslator::getUserPrivateFolderRemoteId(_driveDbId, userPrivateFolderRemoteId); !exitInfo)
        return exitInfo;

    RemoteNodeId commonDocumentsFolderRemoteId;
    if (const auto exitInfo = ApiTranslator::getCommonDocumentsRemoteId(_driveDbId, commonDocumentsFolderRemoteId); !exitInfo)
        return exitInfo;

    RemoteNodeId sharedFolderRemoteId;
    if (const auto exitInfo = ApiTranslator::getSharedRemoteId(_driveDbId, sharedFolderRemoteId); !exitInfo) return exitInfo;

    mainDirectoriesRemoteIds = {userPrivateFolderRemoteId, commonDocumentsFolderRemoteId, sharedFolderRemoteId};
    std::erase_if(mainDirectoriesRemoteIds, [](const std::string_view s) { return s.empty(); });

    return ExitCode::Ok;
}

ExitInfo RemoteFileSystemObserverWorker::listingCursor(const NodeId &remoteDirId, Cursor &cursor, Timestamp timestamp) {
    RemoteNodeId userPrivateFolderRemoteId;
    if (const auto exitInfo = ApiTranslator::getUserPrivateFolderRemoteId(_driveDbId, userPrivateFolderRemoteId); !exitInfo)
        return exitInfo;

    if (remoteDirId == userPrivateFolderRemoteId) return _syncPal->userPrivateFolderCursor(cursor, timestamp);

    RemoteNodeId commonDocumentsFolderRemoteId;
    if (const auto exitInfo = ApiTranslator::getCommonDocumentsRemoteId(_driveDbId, commonDocumentsFolderRemoteId); !exitInfo)
        return exitInfo;

    if (remoteDirId == commonDocumentsFolderRemoteId) return _syncPal->commonDocumentsFolderCursor(cursor, timestamp);

    RemoteNodeId sharedFolderRemoteId;
    if (const auto exitInfo = ApiTranslator::getSharedRemoteId(_driveDbId, sharedFolderRemoteId); !exitInfo) return exitInfo;

    if (remoteDirId == sharedFolderRemoteId) return _syncPal->sharedFolderCursor(cursor, timestamp);

    return {ExitCode::LogicError, ExitCause::InvalidArgument};
}

ExitInfo RemoteFileSystemObserverWorker::saveListingCursor(const NodeId &remoteDirId, const Cursor &cursor,
                                                           const Timestamp timestamp) {
    RemoteNodeId userPrivateFolderRemoteId;
    if (const auto exitInfo = ApiTranslator::getUserPrivateFolderRemoteId(_driveDbId, userPrivateFolderRemoteId); !exitInfo)
        return exitInfo;

    if (remoteDirId == userPrivateFolderRemoteId) return _syncPal->setUserPrivateFolderCursor(cursor, timestamp);

    RemoteNodeId commonDocumentsFolderRemoteId;
    if (const auto exitInfo = ApiTranslator::getCommonDocumentsRemoteId(_driveDbId, commonDocumentsFolderRemoteId); !exitInfo)
        return exitInfo;

    if (remoteDirId == commonDocumentsFolderRemoteId) return _syncPal->setCommonDocumentsFolderCursor(cursor, timestamp);

    RemoteNodeId sharedFolderRemoteId;
    if (const auto exitInfo = ApiTranslator::getSharedRemoteId(_driveDbId, sharedFolderRemoteId); !exitInfo) return exitInfo;

    if (remoteDirId == sharedFolderRemoteId) return _syncPal->setSharedFolderCursor(cursor, timestamp);


    return {ExitCode::LogicError, ExitCause::InvalidArgument};
}

ExitInfo RemoteFileSystemObserverWorker::getV3RemoteFolderName(const RemoteNodeId &remoteDirId, SyncName &folderName) {
    folderName = "";

    RemoteNodeId userPrivateFolderRemoteId;
    if (const auto exitInfo = ApiTranslator::getUserPrivateFolderRemoteId(_driveDbId, userPrivateFolderRemoteId); !exitInfo)
        return exitInfo;

    if (remoteDirId == userPrivateFolderRemoteId) {
        folderName = ApiTranslator::v3UserPrivate;
        return ExitCode::Ok;
    }

    RemoteNodeId commonDocumentsFolderRemoteId;
    if (const auto exitInfo = ApiTranslator::getCommonDocumentsRemoteId(_driveDbId, commonDocumentsFolderRemoteId); !exitInfo)
        return exitInfo;

    if (remoteDirId == commonDocumentsFolderRemoteId) {
        folderName = ApiTranslator::v3CommonDocuments;
        return ExitCode::Ok;
    }

    RemoteNodeId sharedFolderRemoteId;
    if (const auto exitInfo = ApiTranslator::getSharedRemoteId(_driveDbId, sharedFolderRemoteId); !exitInfo) return exitInfo;

    if (remoteDirId == sharedFolderRemoteId) {
        folderName = ApiTranslator::v3Shared;
        return ExitCode::Ok;
    }

    return {ExitCode::LogicError, ExitCause::InvalidArgument};
}

} // namespace KDC
