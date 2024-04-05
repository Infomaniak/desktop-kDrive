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
#include "jobs/network/networkjobsparams.h"
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
    int64_t timestamp;
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
            if (errorCode == forbiddenError) {
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
    while (job->getItem(item, error, ignore)) {
        if (ignore) {
            continue;
        }

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
            _snapshot->removeItem(*nodeIdIt);
        }
        nodeIdIt++;
    }

    std::chrono::duration<double> elapsed_seconds = std::chrono::steady_clock::now() - start;
    LOG_SYNCPAL_DEBUG(_logger,
                      "End reply parsing in " << elapsed_seconds.count() << "s for " << _snapshot->nbItems() << " items");

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

// ExitCode RemoteFileSystemObserverWorker::processFiles(Poco::JSON::Array::Ptr filesArray)
//{
//     if (filesArray) {
//         for (auto it = filesArray->begin(); it != filesArray->end(); ++it) {
//             if (stopAsked()) {
//                 return ExitCodeOk;
//             }

//            Poco::JSON::Object::Ptr fileObj = it->extract<Poco::JSON::Object::Ptr>();

//            // Ignore root directory
//            std::string val;
//            if (JsonParserUtility::extractValue(fileObj, visibilityKey, val) && val == isRootKey) {
//                continue;
//            }

//            if (!JsonParserUtility::extractValue(fileObj, typeKey, val)) {
//                return ExitCodeBackError;
//            }
//            bool isDir = val == dirKey;

//            int idInt = 0;
//            if (!JsonParserUtility::extractValue(fileObj, fileIdKey, idInt)) {
//                return ExitCodeBackError;
//            }
//            NodeId id = std::to_string(idInt);

//            int parentIdInt = 0;
//            if (!JsonParserUtility::extractValue(fileObj, parentIdKey, parentIdInt)) {
//                return ExitCodeBackError;
//            }
//            NodeId parentId = std::to_string(parentIdInt);

//            SyncTime createdAt = 0;
//            if (!JsonParserUtility::extractValue(fileObj, createdAtKey, createdAt, false)) {
//                return ExitCodeBackError;
//            }

//            SyncTime modtime = 0;
//            if (!JsonParserUtility::extractValue(fileObj, lastModifiedAtKey, modtime, false)) {
//                return ExitCodeBackError;
//            }

//            SyncName name;
//            if (!JsonParserUtility::extractValue(fileObj, nameKey, name)) {
//                return ExitCodeBackError;
//            }

//            int64_t size = 0;
//            if (!isDir) {
//                if (!JsonParserUtility::extractValue(fileObj, sizeKey, size)) {
//                    return ExitCodeBackError;
//                }
//            }

//            bool canWrite = true;
//            Poco::JSON::Object::Ptr capabilitiesObj = fileObj->getObject(capabilitiesKey);
//            if (capabilitiesObj) {
//                if (!JsonParserUtility::extractValue(capabilitiesObj, canWriteKey, canWrite)) {
//                    return ExitCodeBackError;
//                }
//            }

//            SnapshotItem item(id, parentId, name, createdAt, modtime, isDir ? NodeType::NodeTypeDirectory :
//            NodeType::NodeTypeFile, size, canWrite);

//            bool isWarning = false;
//            if (ExclusionTemplateCache::instance()->isExcluded("", item.name(), isWarning)) {
//                if (isWarning) {
//                    Error error(_syncPal->syncDbId()
//                            , ""
//                            , id
//                            , isDir ? NodeType::NodeTypeDirectory : NodeType::NodeTypeFile
//                            , name
//                            , ConflictTypeNone
//                            , InconsistencyTypeNone
//                            , CancelTypeExcludedByTemplate);
//                    _syncPal->addError(error);
//                }
//                continue;
//            }

//            if (!isDir) {
//                // Compute time/size checksum
//                item.setTimeSizeChecksum(computeTimeSizeChecksum(modtime, size));
//            }

// #ifdef _WIN32
//             SyncName newName;
//             if (PlatformInconsistencyCheckerUtility::instance()->fixNameWithBackslash(name, newName)) {
//                 name = newName;
//             }
// #endif

//            if (_snapshot->updateItem(item)) {
//                if (ParametersCache::instance()->parameters().extendedLog()) {
//                    LOGW_SYNCPAL_DEBUG(_logger, L"Item inserted in remote snapshot: name:" << SyncName2WStr(name).c_str()
//                                                                                    << L", inode:" << Utility::s2ws(id).c_str()
//                                                                                    << L", parent inode:" <<
//                                                                                    Utility::s2ws(parentId).c_str()
//                                                                                    << L", createdAt:" << createdAt
//                                                                                    << L", modtime:" << modtime
//                                                                                    << L", isDir:" << isDir
//                                                                                    << L", size:" << size);
//                }
//            }
//            else {
//                LOGW_SYNCPAL_WARN(_logger, L"Fail to insert item: " << SyncName2WStr(name).c_str() << L" (" <<
//                Utility::s2ws(id).c_str() << L")"); invalidateSnapshot(); return ExitCodeDataError;
//            }
//        }
//    }

//    return ExitCodeOk;
//}

ExitCode RemoteFileSystemObserverWorker::processActions(Poco::JSON::Array::Ptr actionArray) {
    if (actionArray) {
        std::unordered_set<NodeId> movedItems;

        for (auto it = actionArray->begin(); it != actionArray->end(); ++it) {
            if (stopAsked()) {
                return ExitCodeOk;
            }

            // Check new actions
            std::string action;
            Poco::JSON::Object::Ptr actionObj = it->extract<Poco::JSON::Object::Ptr>();
            if (!JsonParserUtility::extractValue(actionObj, actionKey, action)) {
                return ExitCodeBackError;
            }

            int idInt = 0;
            if (!JsonParserUtility::extractValue(actionObj, fileIdKey, idInt)) {
                return ExitCodeBackError;
            }
            NodeId id = std::to_string(idInt);

            int parentIdInt = 0;
            if (!JsonParserUtility::extractValue(actionObj, parentIdKey, parentIdInt)) {
                return ExitCodeBackError;
            }
            NodeId parentId = std::to_string(parentIdInt);

            SyncName path;
            if (!JsonParserUtility::extractValue(actionObj, pathKey, path)) {
                return ExitCodeBackError;
            }
            SyncName name = path.substr(path.find_last_of('/') + 1);  // +1 to ignore the last "/"

            SyncName destPath;
            if (!JsonParserUtility::extractValue(actionObj, destinationKey, destPath, false)) {
                return ExitCodeBackError;
            }
            SyncName destName = destPath.substr(destPath.find_last_of('/') + 1);  // +1 to ignore the last "/"

            SyncTime createdAt = 0;
            if (!JsonParserUtility::extractValue(actionObj, createdAtKey, createdAt, false)) {
                return ExitCodeBackError;
            }

            SyncTime modtime = 0;
            if (!JsonParserUtility::extractValue(actionObj, lastModifiedAtKey, modtime, false)) {
                return ExitCodeBackError;
            }

            std::string tmp;
            if (!JsonParserUtility::extractValue(actionObj, fileTypeKey, tmp)) {
                return ExitCodeBackError;
            }
            NodeType type = tmp == fileKey ? NodeTypeFile : NodeTypeDirectory;

            int64_t size = 0;
            if (type == NodeTypeFile) {
                if (!JsonParserUtility::extractValue(actionObj, sizeKey, size, false)) {
                    return ExitCodeBackError;
                }
            }

            // Check unsupported characters
            if (hasUnsupportedCharacters(name, id, type)) {
                continue;
            }

            bool canWrite = true;
            Poco::JSON::Object::Ptr capabilitiesObj = actionObj->getObject(capabilitiesKey);
            if (capabilitiesObj) {
                if (!JsonParserUtility::extractValue(capabilitiesObj, canWriteKey, canWrite)) {
                    return ExitCodeBackError;
                }
            }

            // Check access rights
            bool rightsAdded = false;
            if (accessRightsAdded(action, id, rightsAdded, createdAt, modtime) != ExitCodeOk) {
                LOGW_SYNCPAL_WARN(_logger, L"Error while determining access rights on item: "
                                               << SyncName2WStr(name).c_str() << L" (" << Utility::s2ws(id).c_str() << L")");
                invalidateSnapshot();
                return ExitCodeBackError;
            }
            bool rightsRemoved = false;
            if (accessRightsRemoved(action, id, rightsRemoved) != ExitCodeOk) {
                LOGW_SYNCPAL_WARN(_logger, L"Error while determining access rights on item: "
                                               << SyncName2WStr(name).c_str() << L" (" << Utility::s2ws(id).c_str() << L")");
                invalidateSnapshot();
                return ExitCodeBackError;
            }

            SyncName usedName = name;
            if (!destName.empty()) {
                usedName = destName;
            }
            SnapshotItem item(id, parentId, usedName, createdAt, modtime, type, size, canWrite);

            bool isWarning = false;
            if (ExclusionTemplateCache::instance()->isExcludedTemplate(item.name(), isWarning)) {
                if (isWarning) {
                    Error error(_syncPal->syncDbId(), "", id, type, path, ConflictTypeNone, InconsistencyTypeNone,
                                CancelTypeExcludedByTemplate);
                    _syncPal->addError(error);
                }
                // Remove it from snapshot
                _snapshot->removeItem(id);
                continue;
            }

#ifdef _WIN32
            SyncName newName;
            if (PlatformInconsistencyCheckerUtility::instance()->fixNameWithBackslash(usedName, newName)) {
                usedName = newName;
            }
#endif

            // Process action
            if (action == createAction || action == restoreAction || action == moveInAction || rightsAdded) {
                if (_snapshot->updateItem(item)) {
                    LOGW_SYNCPAL_INFO(_logger, L"File/directory updated: " << SyncName2WStr(usedName).c_str() << L" ("
                                                                           << Utility::s2ws(id).c_str() << L")"
                                                                           << L" with action: " << Utility::s2ws(action).c_str());
                }

                if (action == moveInAction) {
                    // Keep track of moved items
                    movedItems.insert(id);
                }

                if (type == NodeTypeDirectory && (action == moveInAction || action == restoreAction || rightsAdded)) {
                    // Retrieve all children
                    ExitCode exitCode = exploreDirectory(id);
                    ExitCause exitCause = this->exitCause();

                    if (exitCode == ExitCodeNetworkError && exitCause == ExitCauseNetworkTimeout) {
                        _syncPal->addError(Error(ERRID, exitCode, exitCause));
                    }

                    if (exitCode != ExitCodeOk) {
                        return exitCode;
                    }
                }
            } else if (action == renameAction) {
                _syncPal->removeItemFromTmpBlacklist(id, ReplicaSideRemote);
                if (_snapshot->updateItem(item)) {
                    LOGW_SYNCPAL_INFO(_logger, L"File/directory: " << SyncName2WStr(name).c_str() << L" ("
                                                                   << Utility::s2ws(id).c_str() << L") renamed");
                }
            } else if (action == editAction) {
                if (_snapshot->updateItem(item)) {
                    LOGW_SYNCPAL_INFO(_logger, L"File/directory: " << SyncName2WStr(name).c_str() << L" ("
                                                                   << Utility::s2ws(id).c_str() << L") edited at:" << modtime);
                }
            } else if (action == trashAction || action == moveOutAction || action == deleteAction || rightsRemoved) {
                if (action == moveOutAction && movedItems.find(id) != movedItems.end()) {
                    // Ignore move out action
                    continue;
                }

                _syncPal->removeItemFromTmpBlacklist(id, ReplicaSideRemote);
                if (_snapshot->removeItem(id)) {
                    if (ParametersCache::instance()->parameters().extendedLog()) {
                        LOGW_SYNCPAL_DEBUG(_logger, L"Item removed from remote snapshot: " << SyncName2WStr(name).c_str() << L" ("
                                                                                           << Utility::s2ws(id).c_str() << L")");
                    }
                } else {
                    LOGW_SYNCPAL_WARN(_logger, L"Fail to remove item: " << SyncName2WStr(name).c_str() << L" ("
                                                                        << Utility::s2ws(id).c_str() << L")");
                    invalidateSnapshot();
                    return ExitCodeBackError;
                }
            } else if (action == accessAction || action == restoreFileShareCreate || action == restoreFileShareDelete ||
                       action == restoreShareLinkCreate || action == restoreShareLinkDelete) {
                // Ignore these actions
            } else {
                LOGW_SYNCPAL_DEBUG(_logger, L"Unknown operation received on file: " << SyncName2WStr(name).c_str() << L" ("
                                                                                    << Utility::s2ws(id).c_str() << L")");
            }
        }
    }

    return ExitCodeOk;
}

ExitCode RemoteFileSystemObserverWorker::accessRightsAdded(const std::string &action, const NodeId &nodeId, bool &rightsAdded,
                                                           SyncTime &createdAt, SyncTime &modtime) {
    if (action == accessRightInsert || action == accessRightUpdate || action == accessRightUserInsert ||
        action == accessRightUserUpdate || action == accessRightTeamInsert || action == accessRightTeamUpdate ||
        action == accessRightMainUsersInsert || action == accessRightMainUsersUpdate) {
        bool hasRights = false;
        ExitCode exitCode = hasAccessRights(nodeId, hasRights, createdAt, modtime);
        rightsAdded = hasRights;
        return exitCode;
    }

    rightsAdded = false;
    return ExitCodeOk;
}

ExitCode RemoteFileSystemObserverWorker::accessRightsRemoved(const std::string &action, const NodeId &nodeId,
                                                             bool &rightsRemoved) {
    if (action == accessRightRemove || action == accessRightUserRemove || action == accessRightTeamRemove ||
        action == accessRightMainUsersRemove) {
        bool hasRights = false;
        SyncTime createdAt = 0;
        SyncTime modtime = 0;
        ExitCode exitCode = hasAccessRights(nodeId, hasRights, createdAt, modtime);
        rightsRemoved = !hasRights;
        return exitCode;
    }

    rightsRemoved = false;
    return ExitCodeOk;
}

ExitCode RemoteFileSystemObserverWorker::hasAccessRights(const NodeId &nodeId, bool &hasRights, SyncTime &createdAt,
                                                         SyncTime &modtime) {
    GetFileInfoJob job(_syncPal->driveDbId(), nodeId);
    job.runSynchronously();
    if (job.hasHttpError()) {
        if (job.getStatusCode() == Poco::Net::HTTPResponse::HTTP_FORBIDDEN ||
            job.getStatusCode() == Poco::Net::HTTPResponse::HTTP_NOT_FOUND) {
            hasRights = false;
            return ExitCodeOk;
        }

        return ExitCodeBackError;
    }

    createdAt = job.creationTime();
    modtime = job.modtime();
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
