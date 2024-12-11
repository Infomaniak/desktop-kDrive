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
#include "../../jobs/network/API_v2/csvfullfilelistwithcursorjob.h"
#include "../../jobs/network/API_v2/getfileinfojob.h"
#include "../../jobs/network/API_v2/longpolljob.h"
#include "../../jobs/network/API_v2/continuefilelistwithcursorjob.h"
#ifdef _WIN32
#include "reconciliation/platform_inconsistency_checker/platforminconsistencycheckerutility.h"
#endif
#include "libcommon/log/sentry/ptraces.h"
#include "libcommon/utility/utility.h"
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

namespace KDC {

RemoteFileSystemObserverWorker::RemoteFileSystemObserverWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name,
                                                               const std::string &shortName) :
    FileSystemObserverWorker(syncPal, name, shortName, ReplicaSide::Remote),
    _driveDbId(syncPal->driveDbId()) {}

RemoteFileSystemObserverWorker::~RemoteFileSystemObserverWorker() {
    LOG_SYNCPAL_DEBUG(_logger, "~RemoteFileSystemObserverWorker");
}

void RemoteFileSystemObserverWorker::execute() {
    ExitCode exitCode(ExitCode::Unknown);

    LOG_SYNCPAL_DEBUG(_logger, "Worker started: name=" << name().c_str());

    // Sync loop
    for (;;) {
        if (stopAsked()) {
            exitCode = ExitCode::Ok;
            invalidateSnapshot();
            break;
        }
        // We never pause this thread

        if (!_snapshot->isValid()) {
            exitCode = generateInitialSnapshot();
            if (exitCode != ExitCode::Ok) {
                LOG_SYNCPAL_DEBUG(_logger, "Error in generateInitialSnapshot: code=" << exitCode);
                break;
            }
        }

        exitCode = processEvents();
        if (exitCode != ExitCode::Ok) {
            LOG_SYNCPAL_DEBUG(_logger, "Error in processEvents: code=" << exitCode);
            break;
        }

        Utility::msleep(LOOP_EXEC_SLEEP_PERIOD);
    }

    LOG_SYNCPAL_DEBUG(_logger, "Worker stopped: name=" << name().c_str());
    setDone(exitCode);
}

ExitCode RemoteFileSystemObserverWorker::generateInitialSnapshot() {
    LOG_SYNCPAL_INFO(_logger, "Starting remote snapshot generation");
    auto start = std::chrono::steady_clock::now();
    sentry::pTraces::scoped::RFSOGenerateInitialSnapshot perfMonitor(syncDbId());

    _snapshot->init();
    _updating = true;
    countListingRequests();

    const ExitCode exitCode = initWithCursor();

    const auto end = std::chrono::steady_clock::now();
    const std::chrono::duration<double> elapsedSeconds = end - start;
    if (exitCode == ExitCode::Ok && !stopAsked()) {
        _snapshot->setValid(true);
        LOG_SYNCPAL_INFO(_logger, "Remote snapshot generated in: " << elapsedSeconds.count() << "s for " << _snapshot->nbItems()
                                                                   << " items");
        perfMonitor.stop();
    } else {
        invalidateSnapshot();
        LOG_SYNCPAL_WARN(_logger, "Remote snapshot generation stopped or failed after: " << elapsedSeconds.count() << "s");

        switch (exitCode) {
            case ExitCode::NetworkError:
                _syncPal->addError(Error(errId(), exitCode, exitCause()));
                break;
            case ExitCode::LogicError:
                if (exitCause() == ExitCause::FullListParsingError) {
                    _syncPal->addError(Error(_syncPal->syncDbId(), name(), exitCode, exitCause()));
                }
                break;
            default:
                break;
        }
    }
    _updating = false;
    return exitCode;
}

ExitCode RemoteFileSystemObserverWorker::processEvents() {
    if (stopAsked()) {
        return ExitCode::Ok;
    }

    // Get last listing cursor used
    int64_t timestamp = 0;
    ExitCode exitCode = _syncPal->listingCursor(_cursor, timestamp);
    if (exitCode != ExitCode::Ok) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncPal::listingCursor");

        setExitCause(ExitCause::DbAccessError);
        return exitCode;
    }

    if (!_updating) {
        // Send long poll request
        bool changes = false;
        exitCode = sendLongPoll(changes);
        if (exitCode != ExitCode::Ok) {
            setExitCause(ExitCause::ApiErr);
            return exitCode;
        }
        if (!changes) {
            // No change, return
            return ExitCode::Ok;
        }
    }

    // Retrieve changes
    _updating = true;
    bool hasMore = true;
    while (hasMore) {
        hasMore = false;

        if (stopAsked()) {
            exitCode = ExitCode::Ok;
            break;
        }

        std::shared_ptr<ContinueFileListWithCursorJob> job = nullptr;
        if (_cursor.empty()) {
            LOG_SYNCPAL_WARN(_logger, "Cursor is empty for driveDbId=" << _driveDbId << ", invalidating snapshot");
            invalidateSnapshot();
            exitCode = ExitCode::DataError;
            break;
        }

        try {
            job = std::make_shared<ContinueFileListWithCursorJob>(_driveDbId, _cursor);
        } catch (const std::exception &e) {
            LOG_SYNCPAL_WARN(_logger, "Error in ContinueFileListWithCursorJob::ContinueFileListWithCursorJob for driveDbId="
                                              << _driveDbId << " error=" << e.what());
            exitCode = ExitCode::DataError;
            break;
        }

        exitCode = job->runSynchronously();
        if (exitCode != ExitCode::Ok) {
            LOG_SYNCPAL_WARN(_logger, "Error in ContinuousCursorListingJob::runSynchronously: code=" << exitCode);
            break;
        }

        Poco::JSON::Object::Ptr resObj = job->jsonRes();
        if (!resObj) {
            LOG_SYNCPAL_WARN(_logger, "Continue cursor listing request failed for drive: " << std::to_string(_driveDbId).c_str()
                                                                                           << " and cursor: " << _cursor.c_str());
            exitCode = ExitCode::BackError;
            break;
        }

        if (std::string errorCode; job->hasErrorApi(&errorCode)) {
            if (getNetworkErrorCode(errorCode) == NetworkErrorCode::forbiddenError) {
                LOG_SYNCPAL_WARN(_logger, "Access forbidden");
                exitCode = ExitCode::Ok;
                break;
            } else {
                std::ostringstream os;
                resObj->stringify(os);
                LOGW_SYNCPAL_WARN(_logger, L"Continue cursor listing request failed: " << Utility::s2ws(os.str()).c_str());
                exitCode = ExitCode::BackError;
                break;
            }
        }

        Poco::JSON::Object::Ptr dataObj = resObj->getObject(dataKey);
        if (dataObj) {
            std::string cursor;
            if (!JsonParserUtility::extractValue(dataObj, cursorKey, cursor)) {
                exitCode = ExitCode::BackError;
                break;
            }

            if (cursor != _cursor) {
                _cursor = cursor;
                LOG_SYNCPAL_DEBUG(_logger, "Sync cursor updated: " << _cursor.c_str());
                int64_t timestamp = static_cast<long int>(time(0));
                exitCode = _syncPal->setListingCursor(_cursor, timestamp);
                if (exitCode != ExitCode::Ok) {
                    LOG_SYNCPAL_WARN(_logger, "Error in SyncPal::setListingCursor");
                    setExitCause(ExitCause::DbAccessError);
                    break;
                }
            }

            if (!JsonParserUtility::extractValue(dataObj, hasMoreKey, hasMore)) {
                return ExitCode::BackError;
            }

            // Look for new actions
            exitCode = processActions(dataObj->getArray(actionsKey));
            if (exitCode != ExitCode::Ok) {
                invalidateSnapshot();
                break;
            }
        }
    }

    _updating = false;

    return ExitCode::Ok;
}

ExitCode RemoteFileSystemObserverWorker::initWithCursor() {
    if (stopAsked()) {
        return ExitCode::Ok;
    }

    return getItemsInDir(_snapshot->rootFolderId(), true);
}

ExitCode RemoteFileSystemObserverWorker::exploreDirectory(const NodeId &nodeId) {
    if (stopAsked()) {
        return ExitCode::Ok;
    }

    return getItemsInDir(nodeId, false);
}

ExitCode RemoteFileSystemObserverWorker::getItemsInDir(const NodeId &dirId, const bool saveCursor) {
    // Send request
    sentry::pTraces::scoped::RFSOBackRequest perfMonitorBackRequest(!saveCursor, syncDbId());
    std::shared_ptr<CsvFullFileListWithCursorJob> job = nullptr;
    try {
        std::unordered_set<NodeId> blackList;
        SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), SyncNodeType::BlackList, blackList);
        job = std::make_shared<CsvFullFileListWithCursorJob>(_driveDbId, dirId, blackList, true);
    } catch (const std::exception &e) {
        std::string what = e.what();
        LOG_SYNCPAL_WARN(_logger, "Error in InitFileListWithCursorJob::InitFileListWithCursorJob for driveDbId="
                                          << _driveDbId << " error=" << what.c_str());
        if (what == invalidToken) {
            return ExitCode::InvalidToken;
        }
        return ExitCode::DataError;
    }

    JobManager::instance()->queueAsyncJob(job, Poco::Thread::PRIO_LOW);
    while (!JobManager::instance()->isJobFinished(job->jobId())) {
        if (stopAsked()) {
            return ExitCode::Ok;
        }

        // Wait a little before checking again
        Utility::msleep(100);
    }

    if (job->exitCode() != ExitCode::Ok) {
        LOG_SYNCPAL_WARN(_logger, "Error in GetFileListWithCursorJob::runSynchronously : " << job->exitCode());
        setExitCause(job->getExitCause());
        return job->exitCode();
    }

    if (saveCursor) {
        const std::string cursor = job->getCursor();
        if (cursor != _cursor) {
            _cursor = cursor;
            LOG_SYNCPAL_DEBUG(_logger, "Cursor updated: " << _cursor.c_str());
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
    const auto start = std::chrono::steady_clock::now();
    SnapshotItem item;
    bool error = false;
    bool ignore = false;
    bool eof = false;
    std::unordered_set<SyncName> existingFiles;
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
        if (ExclusionTemplateCache::instance()->isExcludedByTemplate(item.name(), isWarning)) {
            continue;
        }

        // Check unsupported characters
        if (hasUnsupportedCharacters(item.name(), item.id(), item.type())) {
            continue;
        }

        if (const auto &[_, inserted] = existingFiles.insert(Str2SyncName(item.parentId()) + item.name()); !inserted) {
            // An item with the exact same name already exists in the parent folder.
            LOGW_SYNCPAL_DEBUG(_logger, L"Item \"" << SyncName2WStr(item.name()).c_str() << L"\" already exists in directory \""
                                                   << SyncName2WStr(_snapshot->name(item.parentId())).c_str() << L"\"");

            SyncPath path;
            _snapshot->path(item.parentId(), path, ignore);
            path /= item.name();

            Error err(_syncPal->syncDbId(), "", item.id(), NodeType::Directory, path, ConflictType::None, InconsistencyType::None,
                      CancelType::AlreadyExistLocal);
            _syncPal->addError(err);

            continue;
        }

        if (_snapshot->updateItem(item)) {
            if (ParametersCache::isExtendedLogEnabled()) {
                LOGW_SYNCPAL_DEBUG(_logger, L"Item inserted in remote snapshot: name:"
                                                    << SyncName2WStr(item.name()).c_str() << L", inode:"
                                                    << Utility::s2ws(item.id()).c_str() << L", parent inode:"
                                                    << Utility::s2ws(item.parentId()).c_str() << L", createdAt:"
                                                    << item.createdAt() << L", modtime:" << item.lastModified() << L", isDir:"
                                                    << (item.type() == NodeType::Directory) << L", size:" << item.size());
            }
        }
    }

    if (!eof) {
        const std::string msg = "Failed to parse CSV reply: missing EOF delimiter";
        LOG_SYNCPAL_WARN(_logger, msg.c_str());
        sentry::Handler::captureMessage(sentry::Level::Warning, "RemoteFileSystemObserverWorker::getItemsInDir", msg);
        setExitCause(ExitCause::FullListParsingError);
        return ExitCode::NetworkError;
    }

    // Delete orphans
    std::unordered_set<NodeId> nodeIds;
    _snapshot->ids(nodeIds);
    auto nodeIdIt = nodeIds.begin();
    while (nodeIdIt != nodeIds.end()) {
        if (_snapshot->isOrphan(*nodeIdIt)) {
            LOGW_SYNCPAL_DEBUG(_logger, L"Node '" << SyncName2WStr(_snapshot->name(*nodeIdIt)).c_str() << L"' ("
                                                  << Utility::s2ws(*nodeIdIt).c_str() << L") is orphan. Removing it from "
                                                  << _snapshot->side() << L" snapshot.");
            _snapshot->removeItem(*nodeIdIt);
        }
        nodeIdIt++;
    }

    std::chrono::duration<double> elapsed_seconds = std::chrono::steady_clock::now() - start;
    LOG_SYNCPAL_DEBUG(_logger, "End reply parsing in " << elapsed_seconds.count() << "s for " << itemCount << " items");

    return ExitCode::Ok;
}

ExitCode RemoteFileSystemObserverWorker::sendLongPoll(bool &changes) {
    if (_snapshot->isValid()) {
        std::shared_ptr<LongPollJob> notifyJob = nullptr;
        try {
            notifyJob = std::make_shared<LongPollJob>(_driveDbId, _cursor);
        } catch (const std::exception &e) {
            LOG_SYNCPAL_WARN(_logger, "Error in LongPollJob::LongPollJob for driveDbId=" << _driveDbId << " error=" << e.what());
            return ExitCode::DataError;
        }

        JobManager::instance()->queueAsyncJob(notifyJob, Poco::Thread::PRIO_LOW);
        while (!JobManager::instance()->isJobFinished(notifyJob->jobId())) {
            if (stopAsked()) {
                LOG_DEBUG(_logger, "Request " << notifyJob->jobId() << ": aborting LongPoll job");
                notifyJob->abort();
                return ExitCode::Ok;
            }

            {
                const std::lock_guard<std::mutex> lock(_mutex);
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

        if (notifyJob->exitCode() == ExitCode::NetworkError) {
            LOG_SYNCPAL_DEBUG(_logger, "Notify changes request failed for drive: " << std::to_string(_driveDbId).c_str()
                                                                                   << " and cursor: " << _cursor.c_str());
            if (notifyJob->exitCause() == ExitCause::NetworkTimeout) {
                _syncPal->addError(Error(errId(), notifyJob->exitCode(), notifyJob->exitCause()));
            }
            return ExitCode::NetworkError;
        } else if (notifyJob->hasHttpError()) {
            if (notifyJob->getStatusCode() == Poco::Net::HTTPResponse::HTTP_BAD_GATEWAY) {
                // Ignore this error and check for changes anyway
                LOG_SYNCPAL_INFO(_logger, "Notify changes request failed with error "
                                                  << Poco::Net::HTTPResponse::HTTP_BAD_GATEWAY
                                                  << "for drive: " << std::to_string(_driveDbId).c_str()
                                                  << " and cursor: " << _cursor.c_str() << ". Check for changes anyway.");
                changes = true; // TODO: perhaps not a good idea... what if longpoll crashed and not reachable for a long time???
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
                return ExitCode::BackError;
            }

            // If no changes, return
            if (!JsonParserUtility::extractValue(resObj, changesKey, changes)) {
                return ExitCode::BackError;
            }
        }
    }

    return ExitCode::Ok;
}

ExitCode RemoteFileSystemObserverWorker::processActions(Poco::JSON::Array::Ptr actionArray) {
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
        if (ExclusionTemplateCache::instance()->isExcludedByTemplate(actionInfo.snapshotItem.name(), isWarning)) {
            if (isWarning) {
                Error error(_syncPal->syncDbId(), "", actionInfo.snapshotItem.id(), actionInfo.snapshotItem.type(),
                            actionInfo.path, ConflictType::None, InconsistencyType::None, CancelType::ExcludedByTemplate);
                _syncPal->addError(error);
            }
            // Remove it from snapshot
            _snapshot->removeItem(actionInfo.snapshotItem.id());
            continue;
        }

#ifdef _WIN32
        SyncName newName;
        if (PlatformInconsistencyCheckerUtility::instance()->fixNameWithBackslash(actionInfo.snapshotItem.name(), newName)) {
            actionInfo.snapshotItem.setName(newName);
        }
#endif

        if (const ExitCode exitCode = processAction(actionInfo, movedItems); exitCode != ExitCode::Ok) {
            return exitCode;
        }
    }

    return ExitCode::Ok;
}

ExitCode RemoteFileSystemObserverWorker::extractActionInfo(const Poco::JSON::Object::Ptr actionObj, ActionInfo &actionInfo) {
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

ExitCode RemoteFileSystemObserverWorker::processAction(ActionInfo &actionInfo, std::set<NodeId, std::less<>> &movedItems) {
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
        case ActionCode::actionCodeAccessRightMainUsersUpdate: {
            bool rightsOk = false;
            if (const ExitCode exitCode =
                        checkRightsAndUpdateItem(actionInfo.snapshotItem.id(), rightsOk, actionInfo.snapshotItem);
                exitCode != ExitCode::Ok) {
                return exitCode;
            }
            if (!rightsOk) break; // Current user does not have the right to access this item, ignore action.
            [[fallthrough]];
        }
        case ActionCode::actionCodeMoveIn:
        case ActionCode::actionCodeRestore:
        case ActionCode::actionCodeCreate:
        case ActionCode::actionCodeRename: {
            const bool exploreDir = actionInfo.snapshotItem.type() == NodeType::Directory &&
                                    actionInfo.actionCode != ActionCode::actionCodeCreate &&
                                    !_snapshot->exists(actionInfo.snapshotItem.id());
            _syncPal->removeItemFromTmpBlacklist(actionInfo.snapshotItem.id(), ReplicaSide::Remote);
            _snapshot->updateItem(actionInfo.snapshotItem);
            if (exploreDir) {
                // Retrieve all children
                const ExitCode exitCode = exploreDirectory(actionInfo.snapshotItem.id());
                switch (exitCode) {
                    case ExitCode::NetworkError:
                        if (exitCause() == ExitCause::NetworkTimeout) {
                            _syncPal->addError(Error(errId(), exitCode, exitCause()));
                        }
                        break;
                    case ExitCode::LogicError:
                        if (exitCause() == ExitCause::FullListParsingError) {
                            _syncPal->addError(Error(_syncPal->syncDbId(), name(), exitCode, exitCause()));
                        }
                        break;
                    default:
                        break;
                }

                if (exitCode != ExitCode::Ok) return exitCode;
            }
            if (actionInfo.actionCode == ActionCode::actionCodeMoveIn) {
                // Keep track of moved items
                movedItems.insert(actionInfo.snapshotItem.id());
            }
            break;
        }
        // Item edited
        case ActionCode::actionCodeEdit:
            _snapshot->updateItem(actionInfo.snapshotItem);
            break;

        // Item removed
        case ActionCode::actionCodeAccessRightRemove:
        case ActionCode::actionCodeAccessRightUserRemove:
        case ActionCode::actionCodeAccessRightTeamRemove:
        case ActionCode::actionCodeAccessRightMainUsersRemove: {
            bool rightsOk = false;
            if (const ExitCode exitCode =
                        checkRightsAndUpdateItem(actionInfo.snapshotItem.id(), rightsOk, actionInfo.snapshotItem);
                exitCode != ExitCode::Ok) {
                return exitCode;
            }
            if (rightsOk) break; // Current user still have the right to access this item, ignore action.
            [[fallthrough]];
        }
        case ActionCode::actionCodeMoveOut:
            // Ignore move out action if destination is inside the synced folder.
            if (movedItems.find(actionInfo.snapshotItem.id()) != movedItems.end()) break;
            [[fallthrough]];
        case ActionCode::actionCodeTrash:
            _syncPal->removeItemFromTmpBlacklist(actionInfo.snapshotItem.id(), ReplicaSide::Remote);
            if (!_snapshot->removeItem(actionInfo.snapshotItem.id())) {
                LOGW_SYNCPAL_WARN(_logger, L"Fail to remove item: "
                                                   << SyncName2WStr(actionInfo.snapshotItem.name()).c_str() << L" ("
                                                   << Utility::s2ws(actionInfo.snapshotItem.id()).c_str() << L")");
                invalidateSnapshot();
                return ExitCode::BackError;
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
            LOGW_SYNCPAL_DEBUG(_logger, L"Unknown operation received on item: "
                                                << SyncName2WStr(actionInfo.snapshotItem.name()).c_str() << L" ("
                                                << Utility::s2ws(actionInfo.snapshotItem.id()).c_str() << L")");
    }


    return ExitCode::Ok;
}

ExitCode RemoteFileSystemObserverWorker::checkRightsAndUpdateItem(const NodeId &nodeId, bool &hasRights,
                                                                  SnapshotItem &snapshotItem) {
    std::unique_ptr<GetFileInfoJob> job;
    try {
        job = std::make_unique<GetFileInfoJob>(_syncPal->driveDbId(), nodeId);
    } catch (const std::exception &e) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in GetFileInfoJob::GetFileInfoJob for driveDbId=" << _syncPal->driveDbId() << " nodeId=" << nodeId.c_str()
                                                                          << " error=" << e.what());
        return ExitCode::DataError;
    }

    job->runSynchronously();
    if (job->hasHttpError() || job->exitCode() != ExitCode::Ok) {
        if (job->getStatusCode() == Poco::Net::HTTPResponse::HTTP_FORBIDDEN ||
            job->getStatusCode() == Poco::Net::HTTPResponse::HTTP_NOT_FOUND) {
            hasRights = false;
            return ExitCode::Ok;
        }

        LOGW_SYNCPAL_WARN(_logger, L"Error while determining access rights on item: "
                                           << SyncName2WStr(snapshotItem.name()).c_str() << L" ("
                                           << Utility::s2ws(snapshotItem.id()).c_str() << L")");
        invalidateSnapshot();
        return ExitCode::BackError;
    }

    snapshotItem.setCreatedAt(job->creationTime());
    snapshotItem.setLastModified(job->modtime());
    snapshotItem.setSize(job->size());
    snapshotItem.setIsLink(job->isLink());

    hasRights = true;
    return ExitCode::Ok;
}

bool RemoteFileSystemObserverWorker::hasUnsupportedCharacters(const SyncName &name, const NodeId &nodeId, NodeType type) {
#ifdef __APPLE__
    // Check that the name doesn't contain a character not yet supported by the filesystem (ex: U+1FA77 on pre macOS 13.4)
    bool valid = false;
    if (type == NodeType::File) {
        valid = CommonUtility::fileNameIsValid(name);
    } else if (type == NodeType::Directory) {
        valid = CommonUtility::dirNameIsValid(name);
    }

    if (!valid) {
        LOGW_SYNCPAL_DEBUG(_logger, L"The file/directory name contains a character not yet supported by the filesystem "
                                            << SyncName2WStr(name).c_str() << L". Item is ignored.");

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
