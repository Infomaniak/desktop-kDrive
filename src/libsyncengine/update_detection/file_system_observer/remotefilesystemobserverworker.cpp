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
#include "jobs/network/API_v2/listing/continuefilelistwithcursorjob.h"
#include "jobs/network/API_v2/listing/csvfullfilelistwithcursorjob.h"
#include "jobs/network/API_v2/listing/longpolljob.h"
#include "jobs/network/API_v2/getfileinfojob.h"
#if defined(KD_WINDOWS)
#include "reconciliation/platform_inconsistency_checker/platforminconsistencycheckerutility.h"
#endif
#include "libcommon/log/sentry/ptraces.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/utility/utility.h"
#include "requests/syncnodecache.h"
#include "requests/parameterscache.h"
#include "requests/exclusiontemplatecache.h"
#include "utility/jsonparserutility.h"

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
            if (exitInfo = generateInitialSnapshot(); exitInfo.code() != ExitCode::Ok) {
                LOG_SYNCPAL_DEBUG(_logger, "Error in generateInitialSnapshot: " << exitInfo);
                break;
            }
        }

        if (exitInfo = processEvents(); exitInfo.code() != ExitCode::Ok) {
            LOG_SYNCPAL_DEBUG(_logger, "Error in processEvents: " << exitInfo);
            break;
        }
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
    auto start = std::chrono::steady_clock::now();
    sentry::pTraces::scoped::RFSOGenerateInitialSnapshot perfMonitor(syncDbId());

    // Retrieve the list of blacklisted folders.
    (void) SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), SyncNodeType::BlackList, _blackList);

    _liveSnapshot.init();
    _updating = true;
    countListingRequests();

    const auto end = std::chrono::steady_clock::now();
    const std::chrono::duration<double> elapsedSeconds = end - start;
    if (exitInfo = initWithCursor(); exitInfo.code() == ExitCode::Ok && !stopAsked()) {
        _liveSnapshot.setValid(true);
        LOG_SYNCPAL_INFO(_logger, "Remote snapshot generated in: " << elapsedSeconds.count() << "s for "
                                                                   << _liveSnapshot.nbItems() << " items");
        perfMonitor.stop();
    } else {
        tryToInvalidateSnapshot();
        LOG_SYNCPAL_WARN(_logger, "Remote snapshot generation stopped or failed after: " << elapsedSeconds.count() << "s");

        switch (exitInfo.code()) {
            case ExitCode::NetworkError:
                _syncPal->addError(Error(errId(), exitInfo));
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

ExitInfo RemoteFileSystemObserverWorker::processEvents() {
    if (stopAsked()) {
        return ExitCode::Ok;
    }

    // Get last listing cursor used
    int64_t timestamp = 0;
    if (const auto exitInfo = _syncPal->listingCursor(_cursor, timestamp); exitInfo.code() != ExitCode::Ok) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncPal::listingCursor: " << exitInfo);
        return exitInfo;
    }

    if (!_updating && !_initializing) {
        // Send long poll request
        bool changes = false;
        if (const auto exitInfo = sendLongPoll(changes); exitInfo.code() != ExitCode::Ok) {
            LOG_SYNCPAL_WARN(_logger, "Error in RemoteFileSystemObserverWorker::sendLongPoll: " << exitInfo);
            return exitInfo;
        }
        if (!changes) {
            // No change, return
            return ExitCode::Ok;
        }
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
        if (_cursor.empty()) {
            LOG_SYNCPAL_WARN(_logger, "Cursor is empty for driveDbId=" << _driveDbId << ", invalidating snapshot");
            tryToInvalidateSnapshot();
            exitInfo = ExitCode::DataError;
            break;
        }

        try {
            job = std::make_shared<ContinueFileListWithCursorJob>(_driveDbId, _cursor, _blackList);
        } catch (const std::exception &e) {
            LOG_SYNCPAL_WARN(_logger, "Error in ContinueFileListWithCursorJob::ContinueFileListWithCursorJob for driveDbId="
                                              << _driveDbId << " error=" << e.what());
            exitInfo = AbstractTokenNetworkJob::exception2ExitCode(e);
            break;
        }

        if (exitInfo = job->runSynchronously(); exitInfo.code() != ExitCode::Ok) {
            LOG_SYNCPAL_WARN(_logger, "Error in ContinuousCursorListingJob: " << exitInfo);
            break;
        }

        Poco::JSON::Object::Ptr resObj = job->jsonRes();
        if (!resObj) {
            LOG_SYNCPAL_WARN(_logger, "Continue cursor listing request failed for drive: " << std::to_string(_driveDbId).c_str()
                                                                                           << " and cursor: " << _cursor);
            exitInfo = ExitCode::BackError;
            break;
        }

        if (std::string errorCode; job->hasErrorApi(&errorCode)) {
            if (getNetworkErrorCode(errorCode) == NetworkErrorCode::ForbiddenError) {
                LOG_SYNCPAL_WARN(_logger, "Access forbidden");
                exitInfo = ExitCode::Ok;
                break;
            } else {
                std::ostringstream os;
                resObj->stringify(os);
                LOGW_SYNCPAL_WARN(_logger, L"Continue cursor listing request failed: " << Utility::s2ws(os.str()));
                exitInfo = ExitCode::BackError;
                break;
            }
        }

        Poco::JSON::Object::Ptr dataObj = resObj->getObject(dataKey);
        if (dataObj) {
            std::string cursor;
            if (!JsonParserUtility::extractValue(dataObj, cursorKey, cursor)) {
                exitInfo = ExitCode::BackError;
                break;
            }

            if (cursor != _cursor) {
                _cursor = cursor;
                LOG_SYNCPAL_DEBUG(_logger, "Sync cursor updated: " << _cursor);
                int64_t timestamp = static_cast<long int>(time(0));
                if (exitInfo = _syncPal->setListingCursor(_cursor, timestamp); exitInfo.code() != ExitCode::Ok) {
                    LOG_SYNCPAL_WARN(_logger, "Error in SyncPal::setListingCursor: " << exitInfo);
                    break;
                }
            }

            if (!JsonParserUtility::extractValue(dataObj, hasMoreKey, hasMore)) {
                exitInfo = ExitCode::BackError;
            }

            // Look for new actions
            if (exitInfo = processActions(dataObj->getArray(actionsKey)); exitInfo.code() != ExitCode::Ok) {
                LOG_SYNCPAL_WARN(_logger, "Error in RemoteFileSystemObserverWorker::processActions: " << exitInfo);
                tryToInvalidateSnapshot();
                break;
            }
        }
    }

    _updating = false;

    return exitInfo;
}

ExitInfo RemoteFileSystemObserverWorker::initWithCursor() {
    if (stopAsked()) {
        return ExitCode::Ok;
    }

    return getItemsInDir(_liveSnapshot.rootFolderId(), true);
}

ExitInfo RemoteFileSystemObserverWorker::exploreDirectory(const NodeId &nodeId) {
    if (stopAsked()) {
        return ExitCode::Ok;
    }

    return getItemsInDir(nodeId, false);
}

ExitInfo RemoteFileSystemObserverWorker::getItemsInDir(const NodeId &dirId, const bool saveCursor) {
    // Send request
    sentry::pTraces::scoped::RFSOBackRequest perfMonitorBackRequest(!saveCursor, syncDbId());
    std::shared_ptr<CsvFullFileListWithCursorJob> job = nullptr;
    try {
        job = std::make_shared<CsvFullFileListWithCursorJob>(_driveDbId, dirId, _blackList, true);
    } catch (const std::exception &e) {
        LOG_SYNCPAL_WARN(_logger, "Error in InitFileListWithCursorJob::InitFileListWithCursorJob for driveDbId="
                                          << _driveDbId << " error=" << e.what());
        return AbstractTokenNetworkJob::exception2ExitCode(e);
    }

    SyncJobManager::instance()->queueAsyncJob(job, Poco::Thread::PRIO_LOW);
    while (!SyncJobManager::instance()->isJobFinished(job->jobId())) {
        if (stopAsked()) {
            return ExitCode::Ok;
        }

        // Wait a little before checking again
        Utility::msleep(100);
    }

    if (job->exitInfo().code() != ExitCode::Ok) {
        LOG_SYNCPAL_WARN(_logger, "Error in GetFileListWithCursorJob: " << job->exitInfo());
        return job->exitInfo();
    }

    if (saveCursor) {
        const std::string cursor = job->getCursor();
        if (cursor != _cursor) {
            _cursor = cursor;
            LOG_SYNCPAL_DEBUG(_logger, "Cursor updated: " << _cursor);
            int64_t timestamp = static_cast<long int>(time(0));
            ExitCode exitCode = _syncPal->setListingCursor(_cursor, timestamp);
            if (exitCode != ExitCode::Ok) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncPal::setListingCursor");
                setExitCause(ExitCause::DbAccessError);
                return exitCode;
            }
        }
    }

    // Parse reply
    LOG_SYNCPAL_DEBUG(_logger, "Begin parsing of the CSV reply");
    const TimerUtility timer;
    SnapshotItem item;
    bool error = false;
    bool ignore = false;
    bool eof = false;
    SyncNameSet existingFiles;
    uint64_t itemCount = 0;
    perfMonitorBackRequest.stop();
    sentry::pTraces::counterScoped::RFSOExploreItem perfMonitorExploreItem(!saveCursor, syncDbId());
    while (job->getItem(item, error, ignore, eof)) {
        if (ignore) {
            continue;
        }

        if (eof) break;
        perfMonitorExploreItem.start();

        itemCount++;
        if (error) {
            LOG_SYNCPAL_WARN(_logger, "Logic error: failed to parse CSV reply.");
            setExitCause(ExitCause::FullListParsingError);
            return ExitCode::LogicError;
        }

        if (stopAsked()) {
            return ExitCode::Ok;
        }

        bool isWarning = false;
        if (ExclusionTemplateCache::instance()->isExcluded(item.name(), isWarning)) {
            continue;
        }

        // Check unsupported characters
        if (hasUnsupportedCharacters(item.name(), item.id(), item.type())) {
            continue;
        }

        if (const auto &[_, inserted] = existingFiles.insert(Str2SyncName(item.parentId()) + item.name()); !inserted) {
            // An item with the exact same name already exists in the parent folder.
            LOGW_SYNCPAL_DEBUG(_logger, L"Item \"" << SyncName2WStr(item.name()) << L"\" already exists in directory \""
                                                   << SyncName2WStr(_liveSnapshot.name(item.parentId())) << L"\"");

            SyncPath path;
            _liveSnapshot.path(item.parentId(), path, ignore);
            path /= item.name();

            Error err(_syncPal->syncDbId(), "", item.id(), NodeType::Directory, path, ConflictType::None, InconsistencyType::None,
                      CancelType::AlreadyExistLocal);
            _syncPal->addError(err);

            continue;
        }

        if (_liveSnapshot.updateItem(item)) {
            if (ParametersCache::isExtendedLogEnabled()) {
                LOGW_SYNCPAL_DEBUG(_logger, L"Item inserted in remote snapshot: name:"
                                                    << Utility::quotedSyncName(item.name()) << L", inode:"
                                                    << Utility::s2ws(item.id()) << L", parent inode:"
                                                    << Utility::s2ws(item.parentId()) << L", createdAt:" << item.createdAt()
                                                    << L", modtime:" << item.lastModified() << L", isDir:"
                                                    << (item.type() == NodeType::Directory) << L", size:" << item.size()
                                                    << L", isLink:" << item.isLink());
            }
        }
    }

    if (!eof) {
        const std::string msg = "Failed to parse CSV reply: missing EOF delimiter";
        LOG_SYNCPAL_WARN(_logger, msg);
        sentry::Handler::captureMessage(sentry::Level::Warning, "RemoteFileSystemObserverWorker::getItemsInDir", msg);
        setExitCause(ExitCause::FullListParsingError);
        return ExitCode::NetworkError;
    }

    // Delete orphans
    NodeSet nodeIds;
    _liveSnapshot.ids(nodeIds);
    auto nodeIdIt = nodeIds.begin();
    while (nodeIdIt != nodeIds.end()) {
        if (_liveSnapshot.isOrphan(*nodeIdIt)) {
            LOGW_SYNCPAL_DEBUG(_logger, L"Node '" << SyncName2WStr(_liveSnapshot.name(*nodeIdIt)) << L"' ("
                                                  << Utility::s2ws(*nodeIdIt) << L") is orphan. Removing it from "
                                                  << _liveSnapshot.side() << L" snapshot.");
            _liveSnapshot.removeItem(*nodeIdIt);
        }
        nodeIdIt++;
    }

    LOG_SYNCPAL_DEBUG(_logger,
                      "End reply parsing in " << timer.elapsed<DoubleSeconds>().count() << "s for " << itemCount << " items");

    return ExitCode::Ok;
}

ExitInfo RemoteFileSystemObserverWorker::sendLongPoll(bool &changes) {
    if (_liveSnapshot.isValid()) {
        std::shared_ptr<LongPollJob> notifyJob = nullptr;
        try {
            notifyJob = std::make_shared<LongPollJob>(_driveDbId, _cursor);
        } catch (const std::exception &e) {
            LOG_SYNCPAL_WARN(_logger, "Error in LongPollJob::LongPollJob for driveDbId=" << _driveDbId << " error=" << e.what());
            return AbstractTokenNetworkJob::exception2ExitCode(e);
        }

        SyncJobManager::instance()->queueAsyncJob(notifyJob, Poco::Thread::PRIO_LOW);
        while (!SyncJobManager::instance()->isJobFinished(notifyJob->jobId())) {
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

        if (notifyJob->exitInfo().code() != ExitCode::Ok) {
            LOG_SYNCPAL_WARN(_logger, "Error in LongPollJob: " << notifyJob->exitInfo() << " for drive: "
                                                               << std::to_string(_driveDbId) << " and cursor: " << _cursor);

            if (notifyJob->exitInfo() == ExitInfo(ExitCode::NetworkError, ExitCause::NetworkTimeout)) {
                _syncPal->addError(Error(errId(), notifyJob->exitInfo()));
            } else if (notifyJob->exitInfo() == ExitInfo(ExitCode::NetworkError, ExitCause::BadGateway)) {
                // Ignore this error and check for changes anyway
                LOG_SYNCPAL_INFO(_logger, "Bad gateway error, check for changes anyway.");
                changes = true; // TODO: perhaps not a good idea... what if longpoll crashed and not reachable for a long time???
                return ExitCode::Ok;
            }

            return notifyJob->exitInfo();
        }

        Poco::JSON::Object::Ptr resObj = notifyJob->jsonRes();
        if (!resObj) {
            // If error, fall
            LOG_SYNCPAL_DEBUG(_logger, "Notify changes request failed for drive: " << std::to_string(_driveDbId).c_str()
                                                                                   << " and cursor: " << _cursor);
            return {ExitCode::BackError, ExitCause::ApiErr};
        }

        // If no changes, return
        if (!JsonParserUtility::extractValue(resObj, changesKey, changes)) {
            return {ExitCode::BackError, ExitCause::ApiErr};
        }
    }

    return ExitCode::Ok;
}

ExitInfo RemoteFileSystemObserverWorker::processActions(Poco::JSON::Array::Ptr actionArray) {
    if (!actionArray) return ExitCode::Ok;

    std::set<NodeId, std::less<>> movedItems;

    for (auto it = actionArray->begin(); it != actionArray->end(); ++it) {
        sentry::pTraces::scoped::RFSOChangeDetected perfMonitor(syncDbId());
        if (stopAsked()) {
            return ExitCode::Ok;
        }

        Poco::JSON::Object::Ptr actionObj = it->extract<Poco::JSON::Object::Ptr>();
        ActionInfo actionInfo;
        if (ExitCode exitCode = extractActionInfo(actionObj, actionInfo); exitCode != ExitCode::Ok) {
            return exitCode;
        }

        // Check unsupported characters
        if (hasUnsupportedCharacters(actionInfo.snapshotItem.name(), actionInfo.snapshotItem.id(),
                                     actionInfo.snapshotItem.type())) {
            continue;
        }

        bool isWarning = false;
        if (ExclusionTemplateCache::instance()->isExcluded(actionInfo.snapshotItem.name(), isWarning)) {
            if (isWarning) {
                Error error(_syncPal->syncDbId(), "", actionInfo.snapshotItem.id(), actionInfo.snapshotItem.type(),
                            actionInfo.path, ConflictType::None, InconsistencyType::None, CancelType::ExcludedByTemplate);
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

        if (const auto exitInfo = processAction(actionInfo, movedItems); exitInfo.code() != ExitCode::Ok) {
            LOG_SYNCPAL_WARN(_logger, "Error in RemoteFileSystemObserverWorker::processAction: " << exitInfo);
            return exitInfo;
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

    int64_t tmpInt = 0;
    if (!JsonParserUtility::extractValue(actionObj, fileIdKey, tmpInt)) {
        return ExitCode::BackError;
    }
    actionInfo.snapshotItem.setId(std::to_string(tmpInt));

    if (!JsonParserUtility::extractValue(actionObj, parentIdKey, tmpInt)) {
        return ExitCode::BackError;
    }
    actionInfo.snapshotItem.setParentId(std::to_string(tmpInt));

    SyncName tmpDestPathStr;
    if (!JsonParserUtility::extractValue(actionObj, destinationKey, tmpDestPathStr, false)) {
        return ExitCode::BackError;
    }
    if (!tmpDestPathStr.empty()) {
        // This a move operation. Get the name from the `destination` field
        actionInfo.snapshotItem.setName(tmpDestPathStr.substr(tmpDestPathStr.find_last_of('/') + 1)); // +1 to ignore the last "/"
        actionInfo.path = tmpDestPathStr;
    } else {
        // Otherwise, get the name from the `path` field
        if (!JsonParserUtility::extractValue(actionObj, pathKey, actionInfo.path)) {
            return ExitCode::BackError;
        }
        actionInfo.snapshotItem.setName(
                actionInfo.path.substr(actionInfo.path.find_last_of('/') + 1)); // +1 to ignore the last "/"
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

    if (!JsonParserUtility::extractValue(actionObj, fileTypeKey, tmpStr)) {
        return ExitCode::BackError;
    }
    actionInfo.snapshotItem.setType(tmpStr == fileKey ? NodeType::File : NodeType::Directory);

    if (actionInfo.snapshotItem.type() == NodeType::File) {
        if (!JsonParserUtility::extractValue(actionObj, sizeKey, tmpInt, false)) {
            return ExitCode::BackError;
        }
        actionInfo.snapshotItem.setSize(tmpInt);
    }

    if (!JsonParserUtility::extractValue(actionObj, symbolicLinkKey, tmpStr, false)) {
        return ExitCode::BackError;
    }
    actionInfo.snapshotItem.setIsLink(!tmpStr.empty());

    Poco::JSON::Object::Ptr capabilitiesObj = actionObj->getObject(capabilitiesKey);
    if (capabilitiesObj) {
        bool tmpBool = false;
        if (!JsonParserUtility::extractValue(capabilitiesObj, canWriteKey, tmpBool)) {
            return ExitCode::BackError;
        }
        actionInfo.snapshotItem.setCanWrite(tmpBool);
    }

    return ExitCode::Ok;
}

ExitInfo RemoteFileSystemObserverWorker::processAction(ActionInfo &actionInfo, std::set<NodeId, std::less<>> &movedItems) {
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
            if (const ExitCode exitCode =
                        checkRightsAndUpdateItem(actionInfo.snapshotItem.id(), rightsOk, actionInfo.snapshotItem);
                exitCode != ExitCode::Ok) {
                return exitCode;
            }
            if (!rightsOk) break; // Current user does not have the right to access this item, ignore action.
            [[fallthrough]];
        }
        case ActionCode::ActionCodeMoveIn:
        case ActionCode::ActionCodeRestore:
        case ActionCode::ActionCodeCreate:
        case ActionCode::ActionCodeRename: {
            const bool exploreDir = actionInfo.snapshotItem.type() == NodeType::Directory &&
                                    actionInfo.actionCode != ActionCode::ActionCodeCreate &&
                                    !_liveSnapshot.exists(actionInfo.snapshotItem.id());
            _liveSnapshot.updateItem(actionInfo.snapshotItem);
            if (exploreDir) {
                // Retrieve all children
                const auto exitInfo = exploreDirectory(actionInfo.snapshotItem.id());
                switch (exitInfo.code()) {
                    case ExitCode::NetworkError:
                        if (exitCause() == ExitCause::NetworkTimeout) {
                            _syncPal->addError(Error(errId(), exitInfo));
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

                if (exitInfo.code() != ExitCode::Ok) return exitInfo;
            }
            if (actionInfo.actionCode == ActionCode::ActionCodeMoveIn) {
                // Keep track of moved items
                movedItems.insert(actionInfo.snapshotItem.id());
            }
            break;
        }
        // Item edited
        case ActionCode::ActionCodeEdit:
            _liveSnapshot.updateItem(actionInfo.snapshotItem);
            break;

        // Item removed
        case ActionCode::ActionCodeAccessRightRemove:
        case ActionCode::ActionCodeAccessRightUserRemove:
        case ActionCode::ActionCodeAccessRightTeamRemove:
        case ActionCode::ActionCodeAccessRightMainUsersRemove: {
            bool rightsOk = false;
            if (const ExitCode exitCode =
                        checkRightsAndUpdateItem(actionInfo.snapshotItem.id(), rightsOk, actionInfo.snapshotItem);
                exitCode != ExitCode::Ok) {
                return exitCode;
            }
            if (rightsOk) break; // Current user still have the right to access this item, ignore action.
            [[fallthrough]];
        }
        case ActionCode::ActionCodeMoveOut:
            // Ignore move out action if destination is inside the synced folder.
            if (movedItems.find(actionInfo.snapshotItem.id()) != movedItems.end()) break;
            [[fallthrough]];
        case ActionCode::ActionCodeTrash:
            if (!_liveSnapshot.removeItem(actionInfo.snapshotItem.id())) {
                LOGW_SYNCPAL_WARN(_logger, L"Fail to remove item: " << SyncName2WStr(actionInfo.snapshotItem.name()) << L" ("
                                                                    << Utility::s2ws(actionInfo.snapshotItem.id()) << L")");
                tryToInvalidateSnapshot();
                return ExitCode::BackError;
            }
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
            LOGW_SYNCPAL_DEBUG(_logger, L"Unknown operation received on item: "
                                                << SyncName2WStr(actionInfo.snapshotItem.name()) << L" ("
                                                << Utility::s2ws(actionInfo.snapshotItem.id()) << L")");
    }


    return ExitCode::Ok;
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
        return AbstractTokenNetworkJob::exception2ExitCode(e);
    }

    job->runSynchronously();
    if (job->hasHttpError() || job->exitInfo().code() != ExitCode::Ok) {
        if (job->getStatusCode() == Poco::Net::HTTPResponse::HTTP_FORBIDDEN ||
            job->getStatusCode() == Poco::Net::HTTPResponse::HTTP_NOT_FOUND) {
            hasRights = false;
            return ExitCode::Ok;
        }

        LOGW_SYNCPAL_WARN(_logger, L"Error while determining access rights on item: " << SyncName2WStr(snapshotItem.name())
                                                                                      << L" (" << Utility::s2ws(snapshotItem.id())
                                                                                      << L")");
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

bool RemoteFileSystemObserverWorker::hasUnsupportedCharacters(const SyncName &name, const NodeId &nodeId, NodeType type) {
#if defined(KD_MACOS)
    // Check that the name doesn't contain a character not yet supported by the filesystem (ex: U+1FA77 on pre macOS 13.4)
    bool valid = false;
    if (type == NodeType::File) {
        valid = CommonUtility::fileNameIsValid(name);
    } else if (type == NodeType::Directory) {
        valid = CommonUtility::dirNameIsValid(name);
    }

    if (!valid) {
        LOGW_SYNCPAL_DEBUG(_logger, L"The file/directory name contains a character not yet supported by the filesystem "
                                            << SyncName2WStr(name) << L". Item is ignored.");

        Error err(_syncPal->syncDbId(), "", nodeId, type, name, ConflictType::None, InconsistencyType::NotYetSupportedChar);
        _syncPal->addError(err);
        return true;
    }
#else
    (void) name;
    (void) nodeId;
    (void) type;
#endif
    return false;
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

} // namespace KDC
