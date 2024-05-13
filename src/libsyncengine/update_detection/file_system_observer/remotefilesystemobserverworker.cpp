/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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
#include "jobs/jobmanager.h"
#include "jobs/network/csvfullfilelistwithcursorjob.h"
#include "jobs/network/getfileinfojob.h"
#include "jobs/network/longpolljob.h"
#include "jobs/network/continuefilelistwithcursorjob.h"
#ifdef _WIN32
#include "reconciliation/platform_inconsistency_checker/platforminconsistencycheckerutility.h"
#endif
#include "libcommonserver/utility/utility.h"
#include "requests/syncnodecache.h"
#include "requests/parameterscache.h"
#include "requests/exclusiontemplatecache.h"
#include "utility/jsonparserutility.h"
#ifdef __APPLE__
#include "utility/utility.h"
#endif

#include <log4cplus/loggingmacros.h>

#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>
#include <Poco/Dynamic/Var.h>

#include <queue>

namespace KDC {

RemoteFileSystemObserverWorker::RemoteFileSystemObserverWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name,
                                                               const std::string &shortName)
    : FileSystemObserverWorker(syncPal, name, shortName, ReplicaSideRemote), _driveDbId(syncPal->_driveDbId) {}

RemoteFileSystemObserverWorker::~RemoteFileSystemObserverWorker() {
    LOG_SYNCPAL_DEBUG(_logger, "~RemoteFileSystemObserverWorker");
}

void RemoteFileSystemObserverWorker::execute() {
    ExitCode exitCode(ExitCodeUnknown);

    LOG_SYNCPAL_DEBUG(_logger, "Worker started: name=" << name().c_str());

    // Sync loop
    for (;;) {
        if (stopAsked()) {
            exitCode = ExitCodeOk;
            invalidateSnapshot();
            break;
        }
        // We never pause this thread

        if (!_snapshot->isValid()) {
            exitCode = generateInitialSnapshot();
            if (exitCode != ExitCodeOk) {
                LOG_SYNCPAL_DEBUG(_logger, "Error in generateInitialSnapshot : " << exitCode);
                break;
            }
        }

        exitCode = processEvents();
        if (exitCode != ExitCodeOk) {
            LOG_SYNCPAL_DEBUG(_logger, "Error in processEvents : " << exitCode);
            break;
        }

        Utility::msleep(LOOP_EXEC_SLEEP_PERIOD);
    }

    setDone(exitCode);
    LOG_SYNCPAL_DEBUG(_logger, "Worker stopped: name=" << name().c_str());
}

ExitCode RemoteFileSystemObserverWorker::generateInitialSnapshot() {
    LOG_SYNCPAL_INFO(_logger, "Starting remote snapshot generation");
    auto start = std::chrono::steady_clock::now();

    _snapshot->init();
    _updating = true;

    {
        auto listingFullTimerEnd = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsedTime = listingFullTimerEnd - _listingFullTimer;
        if (elapsedTime.count() > 3600) {  // 1h
            _listingFullCounter = 0;
            _listingFullTimer = listingFullTimerEnd;
        } else if (_listingFullCounter > 60) {
            // If there is more then 1 listing/full request per minute for an hour -> send a sentry
#ifdef NDEBUG
            sentry_capture_event(sentry_value_new_message_event(SENTRY_LEVEL_WARNING,
                                                                "RemoteFileSystemObserverWorker::generateInitialSnapshot",
                                                                "Too many listing/full requests, sync is looping"));
#endif
            _listingFullCounter = 0;
            _listingFullTimer = listingFullTimerEnd;
        }
    }
    _listingFullCounter++;

    ExitCode exitCode = initWithCursor();
    ExitCause exitCause = ExitCause();

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsedSeconds = end - start;
    if (exitCode == ExitCodeOk && !stopAsked()) {
        _snapshot->setValid(true);
        LOG_SYNCPAL_INFO(
            _logger, "Remote snapshot generated in: " << elapsedSeconds.count() << "s for " << _snapshot->nbItems() << " items");
    } else {
        invalidateSnapshot();
        LOG_SYNCPAL_WARN(_logger, "Remote snapshot generation stopped or failed after: " << elapsedSeconds.count() << "s");

        if (exitCode == ExitCodeNetworkError && exitCause == ExitCauseNetworkTimeout) {
            _syncPal->addError(Error(ERRID, exitCode, exitCause));
        }
    }
    _updating = false;
    return exitCode;
}

ExitCode RemoteFileSystemObserverWorker::processEvents() {
    if (stopAsked()) {
        return ExitCodeOk;
    }

    // Get last listing cursor used
    int64_t timestamp = 0;
    ExitCode exitCode = _syncPal->listingCursor(_cursor, timestamp);
    if (exitCode != ExitCodeOk) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncPal::listingCursor");
        setExitCause(ExitCauseDbAccessError);
        return exitCode;
    }

    if (!_updating) {
        // Send long poll request
        bool changes = false;
        exitCode = sendLongPoll(changes);
        if (exitCode != ExitCodeOk) {
            setExitCause(ExitCauseApiErr);
            return exitCode;
        }
        if (!changes) {
            // No change, return
            return ExitCodeOk;
        }
    }

    // Retrieve changes
    _updating = true;
    bool hasMore = true;
    while (hasMore) {
        hasMore = false;

        if (stopAsked()) {
            exitCode = ExitCodeOk;
            break;
        }

        std::shared_ptr<ContinueFileListWithCursorJob> job = nullptr;
        if (_cursor.empty()) {
            LOG_SYNCPAL_WARN(_logger, "Cursor is empty for driveDbId=" << _driveDbId << ", invalidating snapshot");
            invalidateSnapshot();
            exitCode = ExitCodeDataError;
            break;
        }

        try {
            job = std::make_shared<ContinueFileListWithCursorJob>(_driveDbId, _cursor);
        } catch (std::exception const &e) {
            LOG_SYNCPAL_WARN(_logger, "Error in ContinueFileListWithCursorJob::ContinueFileListWithCursorJob for driveDbId="
                                          << _driveDbId << " : " << e.what());
            exitCode = ExitCodeDataError;
            break;
        }

        exitCode = job->runSynchronously();
        if (exitCode != ExitCodeOk) {
            LOG_SYNCPAL_WARN(_logger, "Error in ContinuousCursorListingJob::runSynchronously : " << exitCode);
            break;
        }

        Poco::JSON::Object::Ptr resObj = job->jsonRes();
        if (!resObj) {
            LOG_SYNCPAL_WARN(_logger, "Continue cursor listing request failed for drive: " << std::to_string(_driveDbId).c_str()
                                                                                           << " and cursor: " << _cursor.c_str());
            exitCode = ExitCodeBackError;
            break;
        }

        std::string errorCode;
        if (job->hasErrorApi(&errorCode)) {
            if (getNetworkErrorCode(errorCode) == NetworkErrorCode::forbiddenError) {
                LOG_SYNCPAL_WARN(_logger, "Access forbidden");
                exitCode = ExitCodeOk;
                break;
            } else {
                std::ostringstream os;
                resObj->stringify(os);
                LOGW_SYNCPAL_WARN(_logger, L"Continue cursor listing request failed: " << Utility::s2ws(os.str()).c_str());
                exitCode = ExitCodeBackError;
                break;
            }
        }

        Poco::JSON::Object::Ptr dataObj = resObj->getObject(dataKey);
        if (dataObj) {
            std::string cursor;
            if (!JsonParserUtility::extractValue(dataObj, cursorKey, cursor)) {
                exitCode = ExitCodeBackError;
                break;
            }

            if (cursor != _cursor) {
                _cursor = cursor;
                LOG_SYNCPAL_DEBUG(_logger, "Sync cursor updated: " << _cursor.c_str());
                int64_t timestamp = static_cast<long int>(time(0));
                exitCode = _syncPal->setListingCursor(_cursor, timestamp);
                if (exitCode != ExitCodeOk) {
                    LOG_SYNCPAL_WARN(_logger, "Error in SyncPal::setListingCursor");
                    setExitCause(ExitCauseDbAccessError);
                    break;
                }
            }

            if (!JsonParserUtility::extractValue(dataObj, hasMoreKey, hasMore)) {
                return ExitCodeBackError;
            }

            // Look for new actions
            exitCode = processActions(dataObj->getArray(actionsKey));
            if (exitCode != ExitCodeOk) {
                invalidateSnapshot();
                break;
            }
        }
    }

    _updating = false;

    return ExitCodeOk;
}

ExitCode RemoteFileSystemObserverWorker::initWithCursor() {
    if (stopAsked()) {
        return ExitCodeOk;
    }

    return getItemsInDir(_snapshot->rootFolderId(), true);
}

ExitCode RemoteFileSystemObserverWorker::exploreDirectory(const NodeId &nodeId) {
    if (stopAsked()) {
        return ExitCodeOk;
    }

    return getItemsInDir(nodeId, false);
}

ExitCode RemoteFileSystemObserverWorker::getItemsInDir(const NodeId &dirId, const bool saveCursor) {
    // Send request
    std::shared_ptr<CsvFullFileListWithCursorJob> job = nullptr;
    try {
        std::unordered_set<NodeId> blackList;
        SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), SyncNodeTypeBlackList, blackList);
        job = std::make_shared<CsvFullFileListWithCursorJob>(_driveDbId, dirId, blackList, true);
    } catch (std::exception const &e) {
        std::string what = e.what();
        LOG_SYNCPAL_WARN(_logger, "Error in InitFileListWithCursorJob::InitFileListWithCursorJob for driveDbId="
                                      << _driveDbId << " what =" << what.c_str());
        if (what == invalidToken) {
            return ExitCodeInvalidToken;
        }
        return ExitCodeDataError;
    }

    JobManager::instance()->queueAsyncJob(job, Poco::Thread::PRIO_HIGHEST);
    while (!JobManager::instance()->isJobFinished(job->jobId())) {
        if (stopAsked()) {
            return ExitCodeOk;
        }

        // Wait a little before checking again
        Utility::msleep(100);
    }

    if (job->exitCode() != ExitCodeOk) {
        LOG_SYNCPAL_WARN(_logger, "Error in GetFileListWithCursorJob::runSynchronously : " << job->exitCode());
        setExitCause(job->getExitCause());
        return job->exitCode();
    }

    if (saveCursor) {
        std::string cursor = job->getCursor();
        if (cursor != _cursor) {
            _cursor = cursor;
            LOG_SYNCPAL_DEBUG(_logger, "Cursor updated: " << _cursor.c_str());
            int64_t timestamp = static_cast<long int>(time(0));
            ExitCode exitCode = _syncPal->setListingCursor(_cursor, timestamp);
            if (exitCode != ExitCodeOk) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncPal::setListingCursor");
                setExitCause(ExitCauseDbAccessError);
                return exitCode;
            }
        }
    }

    // Parse reply
    LOG_SYNCPAL_DEBUG(_logger, "Begin reply parsing");
    auto start = std::chrono::steady_clock::now();
    SnapshotItem item;
    bool error = false;
    bool ignore = false;
    std::unordered_set<SyncName> existingFiles;
    uint64_t itemCount = 0;
    while (job->getItem(item, error, ignore)) {
        if (ignore) {
            continue;
        }

        itemCount++;

        if (error) {
            LOG_SYNCPAL_WARN(_logger, "Failed to parse CSV reply");
            setExitCause(ExitCauseUnknown);
            return ExitCodeDataError;
        }

        if (stopAsked()) {
            return ExitCodeOk;
        }

        bool isWarning = false;
        if (ExclusionTemplateCache::instance()->isExcludedTemplate(item.name(), isWarning)) {
            continue;
        }

        // Check unsupported characters
        if (hasUnsupportedCharacters(item.name(), item.id(), item.type())) {
            continue;
        }

        auto insertInfo = existingFiles.insert(Str2SyncName(item.parentId()) + item.name());
        if (!insertInfo.second) {
            // Item with exact same name already exist in parent folder
            LOG_SYNCPAL_DEBUG(Log::instance()->getLogger(),
                              L"Item \"" << SyncName2WStr(item.name()).c_str() << L"\" already exist in directory \""
                                         << SyncName2WStr(_snapshot->name(item.parentId())).c_str() << L"\"");

            SyncPath path;
            _snapshot->path(item.parentId(), path);
            path /= item.name();

            Error err(_syncPal->syncDbId(), "", item.id(), NodeTypeDirectory, path, ConflictTypeNone, InconsistencyTypeNone,
                      CancelTypeAlreadyExistLocal);
            _syncPal->addError(err);

            continue;
        }

        if (_snapshot->updateItem(item)) {
            if (ParametersCache::instance()->parameters().extendedLog()) {
                LOGW_SYNCPAL_DEBUG(_logger, L"Item inserted in remote snapshot: name:"
                                                << SyncName2WStr(item.name()).c_str() << L", inode:"
                                                << Utility::s2ws(item.id()).c_str() << L", parent inode:"
                                                << Utility::s2ws(item.parentId()).c_str() << L", createdAt:" << item.createdAt()
                                                << L", modtime:" << item.lastModified() << L", isDir:"
                                                << (item.type() == NodeTypeDirectory) << L", size:" << item.size());
            }
        }
    }

    // Delete orphans
    std::unordered_set<NodeId> nodeIds;
    _snapshot->ids(nodeIds);
    auto nodeIdIt = nodeIds.begin();
    while (nodeIdIt != nodeIds.end()) {
        if (_snapshot->isOrphan(*nodeIdIt)) {
            LOG_SYNCPAL_DEBUG(_logger,
                              L"Node " << SyncName2WStr(_snapshot->name(*nodeIdIt)).c_str()
                                       << L" (" << Utility::s2ws(*nodeIdIt).c_str()
                                       << L") is orphan. Removing it from "
                                       << Utility::s2ws(Utility::side2Str(_snapshot->side())).c_str() << L" snapshot.");
            _snapshot->removeItem(*nodeIdIt);
        }
        nodeIdIt++;
    }

    std::chrono::duration<double> elapsed_seconds = std::chrono::steady_clock::now() - start;
    LOG_SYNCPAL_DEBUG(_logger,
                      "End reply parsing in " << elapsed_seconds.count() << "s for " << itemCount << " items");

    return ExitCodeOk;
}

ExitCode RemoteFileSystemObserverWorker::sendLongPoll(bool &changes) {
    if (_snapshot->isValid()) {
        std::shared_ptr<LongPollJob> notifyJob = nullptr;
        try {
            notifyJob = std::make_shared<LongPollJob>(_driveDbId, _cursor);
        } catch (std::exception const &e) {
            LOG_SYNCPAL_WARN(_logger, "Error in LongPollJob::LongPollJob for driveDbId=" << _driveDbId << " : " << e.what());
            return ExitCodeDataError;
        }

        JobManager::instance()->queueAsyncJob(notifyJob, Poco::Thread::PRIO_HIGHEST);
        while (!JobManager::instance()->isJobFinished(notifyJob->jobId())) {
            if (stopAsked()) {
                notifyJob->abort();
                LOG_DEBUG(_logger, "Request " << notifyJob->jobId() << ": aborting LongPoll job");
                return ExitCodeOk;
            }

            {
                const std::lock_guard<std::mutex> lock(_mutex);
                if (_updating) {  // We want to update snapshot immediatly, cancel LongPoll job and send a listing/continue
                                  // request
                    notifyJob->abort();
                    changes = true;
                    return ExitCodeOk;
                }
            }

            // Wait until job finished
            Utility::msleep(100);
        }

        if (notifyJob->exitCode() == ExitCodeNetworkError) {
            LOG_SYNCPAL_DEBUG(_logger, "Notify changes request failed for drive: " << std::to_string(_driveDbId).c_str()
                                                                                   << " and cursor: " << _cursor.c_str());
            if (notifyJob->exitCause() == ExitCauseNetworkTimeout) {
                _syncPal->addError(Error(ERRID, notifyJob->exitCode(), notifyJob->exitCause()));
            }
            return ExitCodeNetworkError;
        } else if (notifyJob->hasHttpError()) {
            if (notifyJob->getStatusCode() == Poco::Net::HTTPResponse::HTTP_BAD_GATEWAY) {
                // Ignore this error and check for changes anyway
                LOG_SYNCPAL_INFO(_logger, "Notify changes request failed with error "
                                              << Poco::Net::HTTPResponse::HTTP_BAD_GATEWAY
                                              << "for drive: " << std::to_string(_driveDbId).c_str()
                                              << " and cursor: " << _cursor.c_str() << ". Check for changes anyway.");
                changes = true;  // TODO: perhaps not a good idea... what if longpoll crashed and not reachable for a long time???
            } else {
                LOG_SYNCPAL_WARN(_logger, "Notify changes request failed for drive: " << std::to_string(_driveDbId).c_str()
                                                                                      << " and cursor: " << _cursor.c_str());
                return notifyJob->exitCode();
            }
        } else {
            Poco::JSON::Object::Ptr resObj = notifyJob->jsonRes();
            if (!resObj) {
                // If error, fall
                LOG_SYNCPAL_DEBUG(_logger, "Notify changes request failed for drive: " << std::to_string(_driveDbId).c_str()
                                                                                       << " and cursor: " << _cursor.c_str());
                return ExitCodeBackError;
            }

            // If no changes, return
            if (!JsonParserUtility::extractValue(resObj, changesKey, changes)) {
                return ExitCodeBackError;
            }
        }
    }

    return ExitCodeOk;
}

ExitCode RemoteFileSystemObserverWorker::processActions(Poco::JSON::Array::Ptr actionArray) {
    if (!actionArray) return ExitCodeOk;

    std::unordered_set<NodeId> movedItems;

    for (auto it = actionArray->begin(); it != actionArray->end(); ++it) {
        if (stopAsked()) {
            return ExitCodeOk;
        }

        Poco::JSON::Object::Ptr actionObj = it->extract<Poco::JSON::Object::Ptr>();
        ActionInfo actionInfo;
        if (ExitCode exitCode = extractActionInfo(actionObj, actionInfo); exitCode != ExitCodeOk) {
            return exitCode;
        }

        // Check unsupported characters
        if (hasUnsupportedCharacters(actionInfo.name, actionInfo.nodeId, actionInfo.type)) {
            continue;
        }

        SyncName usedName = actionInfo.name;
        if (!actionInfo.destName.empty()) {
            usedName = actionInfo.destName;
        }
        SnapshotItem item(actionInfo.nodeId, actionInfo.parentNodeId, usedName, actionInfo.createdAt, actionInfo.modtime
                          , actionInfo.type, actionInfo.size, actionInfo.canWrite);

        bool isWarning = false;
        if (ExclusionTemplateCache::instance()->isExcludedTemplate(item.name(), isWarning)) {
            if (isWarning) {
                Error error(_syncPal->syncDbId(), "", actionInfo.nodeId, actionInfo.type, actionInfo.path, ConflictTypeNone, InconsistencyTypeNone,
                            CancelTypeExcludedByTemplate);
                _syncPal->addError(error);
            }
            // Remove it from snapshot
            _snapshot->removeItem(actionInfo.nodeId);
            continue;
        }

#ifdef _WIN32
        SyncName newName;
        if (PlatformInconsistencyCheckerUtility::instance()->fixNameWithBackslash(usedName, newName)) {
            usedName = newName;
        }
#endif
        bool rightsAdded = false;

        // Process action
        switch (actionInfo.actionCode) {
            // Item added
            case ActionCode::actionCodeAccessRightInsert:
            case ActionCode::actionCodeAccessRightUpdate:
            case ActionCode::actionCodeAccessRightUserInsert:
            case ActionCode::actionCodeAccessRightUserUpdate:
            case ActionCode::actionCodeAccessRightTeamInsert:
            case ActionCode::actionCodeAccessRightTeamUpdate:
            case ActionCode::actionCodeAccessRightMainUsersInsert:
            case ActionCode::actionCodeAccessRightMainUsersUpdate:
            {
                bool hasRights = false;
                SyncTime createdAt = 0;
                SyncTime modtime = 0;
                int64_t size = 0;
                if (getFileInfo(actionInfo.nodeId, hasRights, createdAt, modtime, size) != ExitCodeOk) {
                    LOGW_SYNCPAL_WARN(_logger, L"Error while determining access rights on item: "
                                                   << SyncName2WStr(actionInfo.name).c_str() << L" (" << Utility::s2ws(actionInfo.nodeId).c_str()
                                                   << L")");
                    invalidateSnapshot();
                    return ExitCodeBackError;
                }
                if (!hasRights) break;  // Current user does not have the right to access this item, ignore action.

                item.setCreatedAt(createdAt);
                item.setLastModified(modtime);
                item.setSize(size);
                [[fallthrough]];
            }
            case ActionCode::actionCodeMoveIn:
            case ActionCode::actionCodeRestore:
            case ActionCode::actionCodeCreate:
                if (_snapshot->updateItem(item)) {
                    LOGW_SYNCPAL_INFO(_logger, L"File/directory updated: " << SyncName2WStr(usedName).c_str() << L" ("
                                                                           << Utility::s2ws(actionInfo.nodeId).c_str() << L")");
                }
                if (actionInfo.type == NodeTypeDirectory && actionInfo.actionCode != ActionCode::actionCodeCreate) {
                    // Retrieve all children
                    ExitCode exitCode = exploreDirectory(actionInfo.nodeId);
                    ExitCause exitCause = this->exitCause();

                    if (exitCode == ExitCodeNetworkError && exitCause == ExitCauseNetworkTimeout) {
                        _syncPal->addError(Error(ERRID, exitCode, exitCause));
                    }

                    if (exitCode != ExitCodeOk) {
                        return exitCode;
                    }
                }
                if (actionInfo.actionCode == ActionCode::actionCodeMoveIn) {
                    // Keep track of moved items
                    movedItems.insert(actionInfo.nodeId);
                }
                break;

            // Item renamed
            case ActionCode::actionCodeRename:
                _syncPal->removeItemFromTmpBlacklist(actionInfo.nodeId, ReplicaSideRemote);
                if (_snapshot->updateItem(item)) {
                    LOGW_SYNCPAL_INFO(_logger, L"File/directory: " << SyncName2WStr(actionInfo.name).c_str() << L" ("
                                                                   << Utility::s2ws(actionInfo.nodeId).c_str() << L") renamed");
                }
                break;

            // Item edited
            case ActionCode::actionCodeEdit:
                if (_snapshot->updateItem(item)) {
                    LOGW_SYNCPAL_INFO(_logger, L"File/directory: " << SyncName2WStr(actionInfo.name).c_str() << L" ("
                                                                   << Utility::s2ws(actionInfo.nodeId).c_str()
                                                                   << L") edited at:" << actionInfo.modtime);
                }
                break;

            // Item removed
            case ActionCode::actionCodeAccessRightRemove:
            case ActionCode::actionCodeAccessRightUserRemove:
            case ActionCode::actionCodeAccessRightTeamRemove:
            case ActionCode::actionCodeAccessRightMainUsersRemove:
            {
                bool hasRights = false;
                if (getFileInfo(actionInfo.nodeId, hasRights, actionInfo.createdAt, actionInfo.modtime, actionInfo.size) != ExitCodeOk) {
                    LOGW_SYNCPAL_WARN(_logger, L"Error while determining access rights on item: "
                                                   << SyncName2WStr(actionInfo.name).c_str() << L" (" << Utility::s2ws(actionInfo.nodeId).c_str()
                                                   << L")");
                    invalidateSnapshot();
                    return ExitCodeBackError;
                }
                if (hasRights) break;  // Current user still have the right to access this item, ignore action.
                [[fallthrough]];
            }
            case ActionCode::actionCodeMoveOut:
                if (movedItems.find(actionInfo.nodeId) != movedItems.end()) {
                    // Ignore move out action if destination is inside the synced folder.
                    break;
                }
                [[fallthrough]];
            case ActionCode::actionCodeTrash:
                _syncPal->removeItemFromTmpBlacklist(actionInfo.nodeId, ReplicaSideRemote);
                if (_snapshot->removeItem(actionInfo.nodeId)) {
                    if (ParametersCache::instance()->parameters().extendedLog()) {
                        LOGW_SYNCPAL_DEBUG(_logger, L"Item removed from remote snapshot: " << SyncName2WStr(actionInfo.name).c_str() << L" ("
                                                                                           << Utility::s2ws(actionInfo.nodeId).c_str() << L")");
                    }
                } else {
                    LOGW_SYNCPAL_WARN(_logger, L"Fail to remove item: " << SyncName2WStr(actionInfo.name).c_str() << L" ("
                                                                        << Utility::s2ws(actionInfo.nodeId).c_str() << L")");
                    invalidateSnapshot();
                    return ExitCodeBackError;
                }
                break;

            // Ignored actions
            case ActionCode::actionCodeAccess:
            case ActionCode::actionCodeDelete:
            case ActionCode::actionCodeRestoreFileShareCreate:
            case ActionCode::actionCodeRestoreFileShareDelete:
            case ActionCode::actionCodeRestoreShareLinkCreate:
            case ActionCode::actionCodeRestoreShareLinkDelete:
                // Ignore these actions
                break;

            default:
                LOGW_SYNCPAL_DEBUG(_logger, L"Unknown operation received on file: " << SyncName2WStr(actionInfo.name).c_str() << L" ("
                                                                                    << Utility::s2ws(actionInfo.nodeId).c_str() << L")");
        }
    }

    return ExitCodeOk;
}

ExitCode RemoteFileSystemObserverWorker::extractActionInfo(const Poco::JSON::Object::Ptr actionObj, ActionInfo &actionInfo) {
    std::string tmpStr;
    if (!JsonParserUtility::extractValue(actionObj, actionKey, tmpStr)) {
        return ExitCodeBackError;
    }
    actionInfo.actionCode = getActionCode(tmpStr);

    int64_t tmpInt = 0;
    if (!JsonParserUtility::extractValue(actionObj, fileIdKey, tmpInt)) {
        return ExitCodeBackError;
    }
    actionInfo.nodeId = std::to_string(tmpInt);

    if (!JsonParserUtility::extractValue(actionObj, parentIdKey, tmpInt)) {
        return ExitCodeBackError;
    }
    actionInfo.parentNodeId = std::to_string(tmpInt);

    if (!JsonParserUtility::extractValue(actionObj, pathKey, actionInfo.path)) {
        return ExitCodeBackError;
    }
    actionInfo.name = actionInfo.path.substr(actionInfo.path.find_last_of('/') + 1);  // +1 to ignore the last "/"

    SyncName tmpSyncName;
    if (!JsonParserUtility::extractValue(actionObj, destinationKey, tmpSyncName, false)) {
        return ExitCodeBackError;
    }
    actionInfo.destName = tmpSyncName.substr(tmpSyncName.find_last_of('/') + 1);  // +1 to ignore the last "/"

    if (!JsonParserUtility::extractValue(actionObj, createdAtKey, actionInfo.createdAt, false)) {
        return ExitCodeBackError;
    }

    if (!JsonParserUtility::extractValue(actionObj, lastModifiedAtKey, actionInfo.modtime, false)) {
        return ExitCodeBackError;
    }

    if (!JsonParserUtility::extractValue(actionObj, fileTypeKey, tmpStr)) {
        return ExitCodeBackError;
    }
    actionInfo.type = tmpStr == fileKey ? NodeTypeFile : NodeTypeDirectory;

    if (actionInfo.type == NodeTypeFile) {
        if (!JsonParserUtility::extractValue(actionObj, sizeKey, actionInfo.size, false)) {
            return ExitCodeBackError;
        }
    }

    Poco::JSON::Object::Ptr capabilitiesObj = actionObj->getObject(capabilitiesKey);
    if (capabilitiesObj) {
        if (!JsonParserUtility::extractValue(capabilitiesObj, canWriteKey, actionInfo.canWrite)) {
            return ExitCodeBackError;
        }
    }

    return ExitCodeOk;
}

ExitCode RemoteFileSystemObserverWorker::getFileInfo(const NodeId &nodeId, bool &hasRights, SyncTime &createdAt,
                                                         SyncTime &modtime, int64_t &size) {
    GetFileInfoJob job(_syncPal->driveDbId(), nodeId);
    job.runSynchronously();
    if (job.hasHttpError() || job.exitCode() != ExitCodeOk) {
        if (job.getStatusCode() == Poco::Net::HTTPResponse::HTTP_FORBIDDEN ||
            job.getStatusCode() == Poco::Net::HTTPResponse::HTTP_NOT_FOUND) {
            hasRights = false;
            return ExitCodeOk;
        }

        return ExitCodeBackError;
    }

    createdAt = job.creationTime();
    modtime = job.modtime();
    size = job.size();
    hasRights = true;
    return ExitCodeOk;
}

bool RemoteFileSystemObserverWorker::hasUnsupportedCharacters(const SyncName &name, const NodeId &nodeId, NodeType type) {
#ifdef __APPLE__
    // Check that the name doesn't contain a character not yet supported by the filesystem (ex: U+1FA77 on pre macOS 13.4)
    bool valid = false;
    if (type == NodeTypeFile) {
        valid = CommonUtility::fileNameIsValid(name);
    } else if (type == NodeTypeDirectory) {
        valid = CommonUtility::dirNameIsValid(name);
    }

    if (!valid) {
        LOGW_SYNCPAL_DEBUG(_logger, L"The file/directory name contains a character not yet supported by the filesystem "
                                        << SyncName2WStr(name).c_str() << L". Item is ignored.");

        Error err(_syncPal->syncDbId(), "", nodeId, type, name, ConflictTypeNone, InconsistencyTypeNotYetSupportedChar);
        _syncPal->addError(err);
        return true;
    }
#else
    (void)name;
    (void)nodeId;
    (void)type;
#endif
    return false;
}

}  // namespace KDC
