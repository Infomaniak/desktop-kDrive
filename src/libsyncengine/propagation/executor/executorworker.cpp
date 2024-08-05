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

#include "executorworker.h"
#include "jobs/local/localcreatedirjob.h"
#include "jobs/local/localdeletejob.h"
#include "jobs/local/localmovejob.h"
#include "jobs/network/createdirjob.h"
#include "jobs/network/deletejob.h"
#include "jobs/network/downloadjob.h"
#include "jobs/network/movejob.h"
#include "jobs/network/renamejob.h"
#include "jobs/network/uploadjob.h"
#include "jobs/network/upload_session/driveuploadsession.h"
#include "reconciliation/platform_inconsistency_checker/platforminconsistencycheckerutility.h"
#include "update_detection/file_system_observer/filesystemobserverworker.h"
#include "update_detection/update_detector/updatetree.h"
#include "jobs/jobmanager.h"
#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"
#include "requests/parameterscache.h"
#include "requests/syncnodecache.h"
#include "utility/jsonparserutility.h"

#include <iostream>
#include <log4cplus/loggingmacros.h>

#include <jobs/network/getfilelistjob.h>

namespace KDC {

#define SEND_PROGRESS_DELAY 1                // 1 sec
#define SNAPSHOT_INVALIDATION_THRESHOLD 100  // Changes

ExecutorWorker::ExecutorWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName)
    : OperationProcessor(syncPal, name, shortName) {}

void ExecutorWorker::executorCallback(UniqueId jobId) {
    _terminatedJobs.push(jobId);
}

void ExecutorWorker::execute() {
    _executorExitCode = ExitCodeUnknown;
    _executorExitCause = ExitCauseUnknown;
    _snapshotToInvalidate = false;

    _jobToSyncOpMap.clear();
    _syncOpToJobMap.clear();

    LOG_SYNCPAL_DEBUG(_logger, "Worker started: name=" << name().c_str());

    LOG_SYNCPAL_DEBUG(_logger, "JobManager::instance()->countManagedJobs(): " << JobManager::instance()->countManagedJobs());
    LOG_SYNCPAL_DEBUG(_logger, "JobManager::instance()->maxNbThreads() * 2: " << JobManager::instance()->maxNbThreads() * 2);

    // Keep a copy of the sorted list
    _opList = _syncPal->_syncOps->opSortedList();

    initProgressManager();

    uint64_t changesCounter = 0;
    bool hasError = false;
    while (!_opList.empty()) {  // Same loop twice because we might reschedule the jobs after a pause TODO : refactor double loop
        // Create all the jobs
        while (!_opList.empty()) {
            if (!deleteFinishedAsyncJobs()) {
                hasError = true;
                cancelAllOngoingJobs();
                break;
            }

            sendProgress();

            if (stopAsked()) {
                cancelAllOngoingJobs();
                break;
            }

            while (pauseAsked() || isPaused()) {
                if (!isPaused()) {
                    setPauseDone();
                    cancelAllOngoingJobs(true);
                }

                Utility::msleep(LOOP_PAUSE_SLEEP_PERIOD);

                if (unpauseAsked()) {
                    setUnpauseDone();
                }
            }

            if (JobManager::instance()->countManagedJobs() > JobManager::instance()->maxNbThreads() * 2) {
                if (ParametersCache::isExtendedLogEnabled()) {
                    LOG_SYNCPAL_DEBUG(_logger, "Maximum number of jobs reached");
                }
                Utility::msleep(LOOP_PAUSE_SLEEP_PERIOD);
                continue;
            }

            UniqueId opId = _opList.front();
            _opList.pop_front();
            SyncOpPtr syncOp = _syncPal->_syncOps->getOp(opId);

            if (!syncOp) {
                LOG_SYNCPAL_WARN(_logger, "Operation doesn't exist anymore: id=" << opId);
                continue;
            }

            changesCounter++;

            std::shared_ptr<AbstractJob> job = nullptr;
            switch (syncOp->type()) {
                case OperationTypeCreate: {
                    handleCreateOp(syncOp, job, hasError);
                    break;
                }
                case OperationTypeEdit: {
                    handleEditOp(syncOp, job, hasError);
                    break;
                }
                case OperationTypeMove: {
                    handleMoveOp(syncOp, hasError);
                    break;
                }
                case OperationTypeDelete: {
                    handleDeleteOp(syncOp, hasError);
                    break;
                }
                default: {
                    LOGW_SYNCPAL_WARN(_logger, L"Unknown operation type: "
                                                   << Utility::s2ws(Utility::opType2Str(syncOp->type())).c_str() << L" on file "
                                                   << SyncName2WStr(syncOp->affectedNode()->name()).c_str());
                    _executorExitCode = ExitCodeDataError;
                    _executorExitCause = ExitCauseUnknown;
                    hasError = true;
                    cancelAllOngoingJobs();
                    break;
                }
            }

            if (hasError) {
                increaseErrorCount(syncOp);
                cancelAllOngoingJobs();
                break;
            }

            if (job) {
                manageJobDependencies(syncOp, job);
                std::function<void(UniqueId)> callback =
                    std::bind(&ExecutorWorker::executorCallback, this, std::placeholders::_1);
                JobManager::instance()->queueAsyncJob(job, Poco::Thread::PRIO_NORMAL, callback);
                _ongoingJobs.insert({job->jobId(), job});
                _jobToSyncOpMap.insert({job->jobId(), syncOp});
            } else {
                if (syncOp->affectedNode()->id().has_value()) {
                    std::unordered_set<NodeId> whiteList;
                    SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), SyncNodeTypeWhiteList, whiteList);
                    if (whiteList.find(syncOp->affectedNode()->id().value()) != whiteList.end()) {
                        // This item has been synchronized, it can now be removed from white list
                        whiteList.erase(syncOp->affectedNode()->id().value());
                        SyncNodeCache::instance()->update(_syncPal->syncDbId(), SyncNodeTypeWhiteList, whiteList);
                    }
                }
            }
        }

        waitForAllJobsToFinish(hasError);
    }

    if (!hasError) {
        _executorExitCode = ExitCodeOk;
    }

    _syncPal->_syncOps->clear();
    _syncPal->_remoteFSObserverWorker->forceUpdate();

    if (changesCounter > SNAPSHOT_INVALIDATION_THRESHOLD) {
        // If there are too many changes on the local filesystem, the OS stops sending events at some point.
        LOG_SYNCPAL_INFO(_logger, "Local snapshot is potentially invalid");
        _snapshotToInvalidate = true;
    }

    if (_snapshotToInvalidate) {
        LOG_SYNCPAL_INFO(_logger, "Invalidate local snapshot");
        _syncPal->_localFSObserverWorker->invalidateSnapshot();
    }

    _syncPal->vfsCleanUpStatuses();

    setExitCause(_executorExitCause);
    setDone(_executorExitCode);
    LOG_SYNCPAL_DEBUG(_logger, "Worker stopped: name=" << name().c_str());
}

void ExecutorWorker::initProgressManager() {
    for (const auto syncOpId : _opList) {
        SyncFileItem syncItem;
        SyncOpPtr syncOp = _syncPal->_syncOps->getOp(syncOpId);
        if (!syncOp) {
            LOG_SYNCPAL_WARN(_logger, "Operation doesn't exist anymore: id=" << syncOpId);
            continue;
        }

        if (syncOp->omit()) {
            continue;  // Do not notify UI of progress in case of pseudo conflicts
        }

        if (initSyncFileItem(syncOp, syncItem)) {
            _syncPal->initProgress(syncItem);
        }
    }
}

bool ExecutorWorker::initSyncFileItem(SyncOpPtr syncOp, SyncFileItem &syncItem) {
    syncItem.setType(syncOp->affectedNode()->type());
    syncItem.setConflict(syncOp->conflict().type());
    syncItem.setInconsistency(syncOp->affectedNode()->inconsistencyType());
    syncItem.setSize(syncOp->affectedNode()->size());
    syncItem.setModTime(syncOp->affectedNode()->lastmodified().has_value() ? syncOp->affectedNode()->lastmodified().value() : 0);
    syncItem.setCreationTime(syncOp->affectedNode()->createdAt().has_value() ? syncOp->affectedNode()->createdAt().value() : 0);

    if (syncOp->type() & OperationTypeMove) {
        syncItem.setInstruction(SyncFileInstructionMove);
        syncItem.setPath(syncOp->affectedNode()->moveOrigin().has_value() ? *syncOp->affectedNode()->moveOrigin() : SyncPath());
        syncItem.setNewPath(syncOp->affectedNode()->getPath());
    } else {
        syncItem.setPath(syncOp->affectedNode()->getPath());

        if (syncOp->type() & OperationTypeEdit) {
            syncItem.setInstruction(SyncFileInstructionUpdate);
        } else if (syncOp->type() & OperationTypeDelete) {
            syncItem.setInstruction(SyncFileInstructionRemove);
        }
    }

    if (syncOp->targetSide() == ReplicaSideLocal) {
        syncItem.setLocalNodeId(syncOp->correspondingNode() ? syncOp->correspondingNode()->id() : std::nullopt);
        syncItem.setRemoteNodeId(syncOp->affectedNode()->id());
        syncItem.setDirection(SyncDirectionDown);
        if (syncOp->type() & OperationTypeCreate) {
            syncItem.setInstruction(SyncFileInstructionGet);
        }
    } else {
        syncItem.setLocalNodeId(syncOp->affectedNode()->id());
        syncItem.setRemoteNodeId(syncOp->correspondingNode() ? syncOp->correspondingNode()->id() : std::nullopt);
        syncItem.setDirection(SyncDirectionUp);
        if (syncOp->type() & OperationTypeCreate) {
            syncItem.setInstruction(SyncFileInstructionPut);
        }
    }

    return true;
}

void ExecutorWorker::handleCreateOp(SyncOpPtr syncOp, std::shared_ptr<AbstractJob> &job, bool &hasError) {
    // The execution of the create operation consists of three steps:
    // 1. If omit-flag is False, propagate the file or directory to target replica, because the object is missing there.
    // 2. Insert a new entry into the database, to avoid that the object is detected again by compute_ops() on the next sync
    // iteration.
    // 3. Update the update tree structures to ensure that follow-up operations can execute correctly, as they are based on the
    // information in these structures.
    SyncPath relativeLocalFilePath = syncOp->affectedNode()->getPath();
    SyncPath absoluteLocalFilePath = _syncPal->_localPath / relativeLocalFilePath;
    if (isLiteSyncActivated() && !syncOp->omit()) {
        bool isDehydratedPlaceholder = false;
        if (!checkLiteSyncInfoForCreate(syncOp, absoluteLocalFilePath, isDehydratedPlaceholder)) {
            hasError = true;
            return;
        }

        if (isDehydratedPlaceholder) {
            // Blacklist dehydrated placeholder
            PlatformInconsistencyCheckerUtility::renameLocalFile(absoluteLocalFilePath,
                                                                 PlatformInconsistencyCheckerUtility::SuffixTypeBlacklisted);

            // Remove from update tree
            affectedUpdateTree(syncOp)->deleteNode(syncOp->affectedNode());
            targetUpdateTree(syncOp)->deleteNode(syncOp->correspondingNode());

            return;
        }
    }

    if (syncOp->omit()) {
        // Do not generate job, only push changes in DB and update tree
        std::shared_ptr<Node> node;
        if (!propagateCreateToDbAndTree(
                syncOp, syncOp->correspondingNode()->id().has_value() ? *syncOp->correspondingNode()->id() : std::string(),
                syncOp->affectedNode()->lastmodified(), node)) {
            LOGW_SYNCPAL_WARN(_logger, L"Failed to propagate changes in DB or update tree for: "
                                           << SyncName2WStr(syncOp->affectedNode()->name()).c_str());
            _syncPal->setProgressComplete(relativeLocalFilePath, SyncFileStatusError);
            hasError = true;
            return;
        }

        _syncPal->setProgressComplete(relativeLocalFilePath, SyncFileStatusSuccess);
    } else {
        if (!isLiteSyncActivated() && !enoughLocalSpace(syncOp)) {
            _executorExitCode = ExitCodeSystemError;
            _executorExitCause = ExitCauseNotEnoughDiskSpace;
            _syncPal->addError(Error(_syncPal->syncDbId(), name(), _executorExitCode, _executorExitCause));
            hasError = true;
            return;
        }

        bool exists = false;
        if (!hasRight(syncOp, exists)) {
            if (syncOp->targetSide() == ReplicaSideLocal) {
                // Ignore operation
                _syncPal->setProgressComplete(relativeLocalFilePath,
                                              SyncFileStatusIgnored);  // TODO : do not exclude item from sync, rollback
            } else if (syncOp->targetSide() == ReplicaSideRemote) {
                if (!exists) {
                    _syncPal->setProgressComplete(relativeLocalFilePath, SyncFileStatusError);
                    _executorExitCode = ExitCodeDataError;
                    _executorExitCause = ExitCauseUnexpectedFileSystemEvent;
                    hasError = true;
                    return;
                }

                bool needRestart = false;
                if (!convertToPlaceholder(relativeLocalFilePath, true, needRestart)) {
                    LOGW_SYNCPAL_WARN(
                        _logger, L"Failed to create placeholder for: " << SyncName2WStr(syncOp->affectedNode()->name()).c_str());
                    _syncPal->setProgressComplete(relativeLocalFilePath, SyncFileStatusError);
                    _executorExitCode = needRestart ? ExitCodeDataError : ExitCodeSystemError;
                    _executorExitCause = needRestart ? ExitCauseUnexpectedFileSystemEvent : ExitCauseUnknown;
                    hasError = true;
                    return;
                }

                // Ignore operation
                SyncFileItem syncItem;
                if (_syncPal->getSyncFileItem(relativeLocalFilePath, syncItem)) {
                    Error err(_syncPal->syncDbId(), syncItem.localNodeId().has_value() ? syncItem.localNodeId().value() : "",
                              syncItem.remoteNodeId().has_value() ? syncItem.remoteNodeId().value() : "", syncItem.type(),
                              syncOp->affectedNode()->moveOrigin().has_value() ? syncOp->affectedNode()->moveOrigin().value()
                                                                               : syncItem.path(),
                              syncItem.conflict(), syncItem.inconsistency(), CancelTypeCreate);
                    _syncPal->addError(err);
                }

                _syncPal->setProgressComplete(relativeLocalFilePath, SyncFileStatusIgnored);
                std::shared_ptr<UpdateTree> sourceUpdateTree = affectedUpdateTree(syncOp);
                sourceUpdateTree->deleteNode(syncOp->affectedNode());
            }

            return;
        }

        if (!generateCreateJob(syncOp, job)) {
            hasError = true;
            return;
        }

        if (job && syncOp->affectedNode()->type() == NodeTypeDirectory) {
            // Propagate the directory creation immediately in order to avoid blocking other dependant job creation
            if (!runCreateDirJob(syncOp, job)) {
                _syncPal->setProgressComplete(relativeLocalFilePath, SyncFileStatusError);

                std::shared_ptr<CreateDirJob> createDirJob = std::dynamic_pointer_cast<CreateDirJob>(job);
                if (createDirJob && (createDirJob->getStatusCode() == Poco::Net::HTTPResponse::HTTP_BAD_REQUEST ||
                                     createDirJob->getStatusCode() == Poco::Net::HTTPResponse::HTTP_FORBIDDEN)) {
                    checkAlreadyExcluded(absoluteLocalFilePath, createDirJob->parentDirId());
                }

                hasError = true;

                if (syncOp->targetSide() == ReplicaSideLocal) {
                    _executorExitCode = job->exitCode() == ExitCodeNeedRestart ? ExitCodeDataError : job->exitCode();
                    _executorExitCause = job->exitCode() == ExitCodeNeedRestart ? ExitCauseFileAlreadyExist : job->exitCause();
                } else if (syncOp->targetSide() == ReplicaSideRemote) {
                    _executorExitCode = job->exitCode() == ExitCodeNeedRestart ? ExitCodeBackError : job->exitCode();
                    _executorExitCause = job->exitCode() == ExitCodeNeedRestart ? ExitCauseFileAlreadyExist : job->exitCause();
                }

                return;
            }

            bool needRestart = false;
            if (!convertToPlaceholder(relativeLocalFilePath, syncOp->targetSide() == ReplicaSideRemote, needRestart)) {
                LOGW_SYNCPAL_WARN(
                    _logger, L"Failed to convert to placeholder for: " << SyncName2WStr(syncOp->affectedNode()->name()).c_str());
                if (needRestart) {
                    _executorExitCode = ExitCodeDataError;
                    _executorExitCause = ExitCauseUnexpectedFileSystemEvent;
                } else {
                    _syncPal->setProgressComplete(relativeLocalFilePath, SyncFileStatusError);
                    _executorExitCode = ExitCodeSystemError;
                    _executorExitCause = ExitCauseUnknown;
                }

                hasError = true;
                return;
            }

            job.reset();
            _syncPal->setProgressComplete(relativeLocalFilePath, SyncFileStatusSuccess);
        }
    }
}

void ExecutorWorker::checkAlreadyExcluded(const SyncPath &absolutePath, const NodeId &parentId) {
    bool alreadyExist = false;

    // List all items in parent dir
    GetFileListJob job(_syncPal->driveDbId(), parentId);
    job.runSynchronously();

    try {
        Poco::JSON::Object::Ptr resObj = job.jsonRes();
        Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
        for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
            Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
            if (obj) {
                std::string name;
                if (!JsonParserUtility::extractValue(obj, nameKey, name)) {
                    return;
                }

                if (name == absolutePath.filename().string()) {
                    alreadyExist = true;
                }
            }
        }
    } catch (...) {
        LOGW_SYNCPAL_WARN(_logger,
                          L"Failed to check if file: " << Utility::formatSyncPath(absolutePath).c_str() << L" already exist.");
    }

    if (!alreadyExist) {
        return;
    }

    // The item already exist, exclude it
    PlatformInconsistencyCheckerUtility::renameLocalFile(absolutePath,
                                                         PlatformInconsistencyCheckerUtility::SuffixTypeBlacklisted);

    _executorExitCode = ExitCodeDataError;
    _executorExitCause = ExitCauseFileAlreadyExist;
}

bool ExecutorWorker::generateCreateJob(SyncOpPtr syncOp, std::shared_ptr<AbstractJob> &job) noexcept {
    // 1. If omit-flag is False, propagate the file or directory to replica Y, because the object is missing there.
    std::shared_ptr<Node> newCorrespondingParentNode = nullptr;
    if (affectedUpdateTree(syncOp)->rootNode() == syncOp->affectedNode()->parentNode()) {
        newCorrespondingParentNode = targetUpdateTree(syncOp)->rootNode();

        if (!newCorrespondingParentNode) {
            LOGW_SYNCPAL_WARN(_logger, L"Failed to get target root node");
            _syncPal->setProgressComplete(syncOp->affectedNode()->getPath(), SyncFileStatusError);
            _executorExitCode = ExitCodeDataError;
            _executorExitCause = ExitCauseUnknown;
            return false;
        }
    } else {
        newCorrespondingParentNode = correspondingNodeInOtherTree(syncOp->affectedNode()->parentNode());

        if (!newCorrespondingParentNode) {
            LOGW_SYNCPAL_WARN(
                _logger, L"Failed to get corresponding parent node: " << SyncName2WStr(syncOp->affectedNode()->name()).c_str());
            _syncPal->setProgressComplete(syncOp->affectedNode()->getPath(), SyncFileStatusError);
            _executorExitCode = ExitCodeDataError;
            _executorExitCause = ExitCauseUnknown;
            return false;
        }
    }

    if (syncOp->targetSide() == ReplicaSideLocal) {
        SyncPath relativeLocalFilePath = newCorrespondingParentNode->getPath() / syncOp->newName();
        SyncPath absoluteLocalFilePath = _syncPal->_localPath / relativeLocalFilePath;

        bool placeholderCreation = isLiteSyncActivated() && syncOp->affectedNode()->type() == NodeTypeFile;
        if (placeholderCreation && syncOp->affectedNode()->id().has_value()) {
            const bool isLink = _syncPal->_remoteSnapshot->isLink(*syncOp->affectedNode()->id());
            placeholderCreation = !isLink;
        }

        if (placeholderCreation) {
            if (!createPlaceholder(relativeLocalFilePath)) {
                LOGW_SYNCPAL_WARN(_logger,
                                  L"Failed to create placeholder for: " << SyncName2WStr(syncOp->affectedNode()->name()).c_str());
                _syncPal->setProgressComplete(relativeLocalFilePath, SyncFileStatusError);

                bool parentExists = false;
                IoError ioError = IoErrorSuccess;
                if (!IoHelper::checkIfPathExists(absoluteLocalFilePath.parent_path(), parentExists, ioError)) {
                    LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: "
                                           << Utility::formatIoError(absoluteLocalFilePath.parent_path(), ioError).c_str());
                    _executorExitCode = ExitCodeSystemError;
                    _executorExitCause = ExitCauseFileAccessError;
                    return false;
                }

                if (ioError == IoErrorAccessDenied) {
                    LOGW_WARN(_logger, L"Item misses search permission: "
                                           << Utility::formatSyncPath(absoluteLocalFilePath.parent_path()).c_str());
                    _executorExitCode = ExitCodeSystemError;
                    _executorExitCause = ExitCauseNoSearchPermission;
                    return false;
                }

                bool exists = false;
                if (!IoHelper::checkIfPathExists(absoluteLocalFilePath, exists, ioError)) {
                    LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: "
                                           << Utility::formatIoError(absoluteLocalFilePath, ioError).c_str());
                    _executorExitCode = ExitCodeSystemError;
                    _executorExitCause = ExitCauseFileAccessError;
                    return false;
                }

                if (ioError == IoErrorAccessDenied) {
                    LOGW_WARN(_logger,
                              L"Item misses search permission: " << Utility::formatSyncPath(absoluteLocalFilePath).c_str());
                    _executorExitCode = ExitCodeSystemError;
                    _executorExitCause = ExitCauseNoSearchPermission;
                    return false;
                }

                bool restart = !parentExists  // Parent path does not exist on local replica. Rebuild the snapshots.
                               || exists;     // Item already exist. Rebuild the snapshots.

                _executorExitCode = restart ? ExitCodeDataError : ExitCodeSystemError;
                _executorExitCause = restart ? ExitCauseInvalidSnapshot : ExitCauseFileAccessError;
                return false;
            }

            FileStat fileStat;
            IoError ioError = IoErrorSuccess;
            if (!IoHelper::getFileStat(absoluteLocalFilePath, &fileStat, ioError)) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in IoHelper::getFileStat: "
                                               << Utility::formatIoError(absoluteLocalFilePath, ioError).c_str());
                _syncPal->setProgressComplete(relativeLocalFilePath, SyncFileStatusError);
                _executorExitCode = ExitCodeSystemError;
                _executorExitCause = ExitCauseUnknown;
                return false;
            }

            if (ioError == IoErrorNoSuchFileOrDirectory) {
                LOGW_WARN(_logger, L"Item does not exist anymore: " << Utility::formatSyncPath(absoluteLocalFilePath).c_str());
                _executorExitCode = ExitCodeDataError;
                _executorExitCause = ExitCauseInvalidSnapshot;
                return false;
            } else if (ioError == IoErrorAccessDenied) {
                LOGW_WARN(_logger, L"Item misses search permission: " << Utility::formatSyncPath(absoluteLocalFilePath).c_str());
                _executorExitCode = ExitCodeSystemError;
                _executorExitCause = ExitCauseNoSearchPermission;
                return false;
            }

            std::shared_ptr<Node> newNode = nullptr;
            if (!propagateCreateToDbAndTree(syncOp, std::to_string(fileStat.inode), syncOp->affectedNode()->lastmodified(),
                                            newNode)) {
                LOGW_SYNCPAL_WARN(_logger, L"Failed to propagate changes in DB or update tree for: "
                                               << SyncName2WStr(syncOp->affectedNode()->name()).c_str());
                _syncPal->setProgressComplete(relativeLocalFilePath, SyncFileStatusError);
                return false;
            }

            SyncFileStatus status = SyncFileStatusSuccess;

            // Check for inconsistency
            if (syncOp->affectedNode()->inconsistencyType() != InconsistencyTypeNone) {
                // Notify user that the file has been renamed
                SyncFileItem syncItem;
                if (_syncPal->getSyncFileItem(relativeLocalFilePath, syncItem)) {
                    status = SyncFileStatusInconsistency;

                    Error err(_syncPal->syncDbId(), syncItem.localNodeId().has_value() ? syncItem.localNodeId().value() : "",
                              syncItem.remoteNodeId().has_value() ? syncItem.remoteNodeId().value() : "", syncItem.type(),
                              syncItem.newPath().has_value() ? syncItem.newPath().value() : syncItem.path(), syncItem.conflict(),
                              syncItem.inconsistency());
                    _syncPal->addError(err);
                }
            }

            _syncPal->setProgressComplete(relativeLocalFilePath, status);
        } else {
            if (syncOp->affectedNode()->type() == NodeTypeDirectory) {
                job = std::make_shared<LocalCreateDirJob>(absoluteLocalFilePath);
            } else {
                bool exists = false;
                IoError ioError = IoErrorSuccess;
                if (!IoHelper::checkIfPathExists(absoluteLocalFilePath, exists, ioError)) {
                    LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: "
                                           << Utility::formatIoError(absoluteLocalFilePath, ioError).c_str());
                    _executorExitCode = ExitCodeSystemError;
                    _executorExitCause = ExitCauseFileAccessError;
                    return false;
                }

                if (exists) {
                    // Normal in lite sync mode
                    // or in case where a remote item and a local item have same name with different cases
                    // (ex: 'test.txt' on local replica needs to be uploaded, 'Text.txt' on remote replica needs to be downloaded)
                    if (ParametersCache::isExtendedLogEnabled()) {
                        LOGW_SYNCPAL_DEBUG(_logger, L"File: " << Utility::formatSyncPath(absoluteLocalFilePath).c_str()
                                                              << L" already exists on local replica");
                    }
                    // Do not propagate this file
                    return true;
                } else {
                    try {
                        job = std::make_shared<DownloadJob>(
                            _syncPal->_driveDbId,
                            syncOp->affectedNode()->id().has_value() ? *syncOp->affectedNode()->id() : std::string(),
                            absoluteLocalFilePath, syncOp->affectedNode()->size(),
                            syncOp->affectedNode()->createdAt().has_value() ? *syncOp->affectedNode()->createdAt() : 0,
                            syncOp->affectedNode()->lastmodified().has_value() ? *syncOp->affectedNode()->lastmodified() : 0,
                            true);
                    } catch (std::exception const &e) {
                        LOGW_SYNCPAL_WARN(_logger, L"Error in DownloadJob::DownloadJob for driveDbId="
                                                       << _syncPal->_driveDbId << L" : " << Utility::s2ws(e.what()).c_str());
                        _executorExitCode = ExitCodeDataError;
                        _executorExitCause = ExitCauseUnknown;
                        return false;
                    }

                    job->setAffectedFilePath(relativeLocalFilePath);
                }
            }
        }
    } else {
        SyncPath relativeLocalFilePath = syncOp->affectedNode()->getPath();
        SyncPath absoluteLocalFilePath = _syncPal->_localPath / relativeLocalFilePath;
        if (syncOp->affectedNode()->type() == NodeTypeDirectory) {
            bool needRestart = false;
            if (!convertToPlaceholder(relativeLocalFilePath, syncOp->targetSide() == ReplicaSideRemote, needRestart)) {
                if (needRestart) {
                    LOGW_SYNCPAL_DEBUG(_logger, SyncName2WStr(syncOp->affectedNode()->name()).c_str()
                                                    << L" does not exist anymore. Aborting jobs and restarting sync.");
                } else {
                    LOGW_SYNCPAL_WARN(
                        _logger, L"Failed to create placeholder for: " << SyncName2WStr(syncOp->affectedNode()->name()).c_str());
                }
                _syncPal->setProgressComplete(relativeLocalFilePath, SyncFileStatusError);
                _executorExitCode = needRestart ? ExitCodeOk : ExitCodeSystemError;
                _executorExitCause = ExitCauseUnknown;
                _syncPal->_restart = true;
                _syncPal->updateTree(ReplicaSideLocal)->deleteNode(syncOp->affectedNode());
                return false;
            }

            try {
                job = std::make_shared<CreateDirJob>(
                    _syncPal->_driveDbId, absoluteLocalFilePath,
                    newCorrespondingParentNode->id().has_value() ? *newCorrespondingParentNode->id() : std::string(),
                    syncOp->newName());
            } catch (std::exception const &e) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in CreateDirJob::CreateDirJob for driveDbId="
                                               << _syncPal->_driveDbId << L" : " << Utility::s2ws(e.what()).c_str());
                _executorExitCode = ExitCodeDataError;
                _executorExitCause = ExitCauseUnknown;
                return false;
            }
        } else {
            bool needRestart = false;
            if (!convertToPlaceholder(relativeLocalFilePath, true, needRestart)) {
                LOGW_SYNCPAL_WARN(_logger,
                                  L"Failed to create placeholder for: " << SyncName2WStr(syncOp->affectedNode()->name()).c_str());
                _syncPal->setProgressComplete(relativeLocalFilePath, SyncFileStatusError);
                _executorExitCode = needRestart
                                        ? ExitCodeDataError
                                        : ExitCodeSystemError;  // TODO : data error here leads to a rebuild of the snapshots
                // while just running the sync again should be enough
                _executorExitCause = needRestart ? ExitCauseUnexpectedFileSystemEvent : ExitCauseUnknown;
                return false;
            }

            uint64_t filesize = 0;
            if (!getFileSize(absoluteLocalFilePath, filesize)) {
                LOGW_WARN(_logger,
                          L"Error in ExecutorWorker::getFileSize for " << Utility::formatSyncPath(absoluteLocalFilePath).c_str());
                return false;
            }

            if (filesize > useUploadSessionThreshold) {
                try {
                    int uploadSessionParallelJobs = ParametersCache::instance()->parameters().uploadSessionParallelJobs();
                    job = std::make_shared<DriveUploadSession>(
                        _syncPal->_driveDbId, _syncPal->_syncDb, absoluteLocalFilePath, syncOp->affectedNode()->name(),
                        newCorrespondingParentNode->id().has_value() ? *newCorrespondingParentNode->id() : std::string(),
                        syncOp->affectedNode()->lastmodified() ? *syncOp->affectedNode()->lastmodified() : 0,
                        isLiteSyncActivated(), uploadSessionParallelJobs);
                } catch (std::exception const &e) {
                    LOGW_SYNCPAL_WARN(_logger,
                                      L"Error in DriveUploadSession::DriveUploadSession: " << Utility::s2ws(e.what()).c_str());
                    _executorExitCode = ExitCodeDataError;
                    _executorExitCause = ExitCauseUnknown;
                    return false;
                };
            } else {
                try {
                    job = std::make_shared<UploadJob>(
                        _syncPal->_driveDbId, absoluteLocalFilePath, syncOp->affectedNode()->name(),
                        newCorrespondingParentNode->id().has_value() ? *newCorrespondingParentNode->id() : std::string(),
                        syncOp->affectedNode()->lastmodified() ? *syncOp->affectedNode()->lastmodified() : 0);
                } catch (std::exception const &e) {
                    LOGW_SYNCPAL_WARN(_logger, L"Error in UploadJob::UploadJob for driveDbId="
                                                   << _syncPal->_driveDbId << L" : " << Utility::s2ws(e.what()).c_str());
                    _executorExitCode = ExitCodeDataError;
                    _executorExitCause = ExitCauseUnknown;
                    return false;
                }
            }

            job->setAffectedFilePath(relativeLocalFilePath);
        }
    }

    if (job) {
        if (_syncPal->_vfsMode == VirtualFileModeMac || _syncPal->_vfsMode == VirtualFileModeWin) {
            // Set VFS callbacks
            std::function<bool(const SyncPath &, PinState)> vfsSetPinStateCallback =
                std::bind(&SyncPal::vfsSetPinState, _syncPal, std::placeholders::_1, std::placeholders::_2);
            job->setVfsSetPinStateCallback(vfsSetPinStateCallback);

            std::function<bool(const SyncPath &, const SyncTime &, const SyncTime &, const int64_t, const NodeId &,
                               std::string &)>
                vfsUpdateMetadataCallback =
                    std::bind(&SyncPal::vfsUpdateMetadata, _syncPal, std::placeholders::_1, std::placeholders::_2,
                              std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6);
            job->setVfsUpdateMetadataCallback(vfsUpdateMetadataCallback);

            std::function<bool(const SyncPath &)> vfsCancelHydrateCallback =
                std::bind(&SyncPal::vfsCancelHydrate, _syncPal, std::placeholders::_1);
            job->setVfsCancelHydrateCallback(vfsCancelHydrateCallback);
        }

        std::function<bool(const SyncPath &, bool, int, bool)> vfsForceStatusCallback =
            std::bind(&SyncPal::vfsForceStatus, _syncPal, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                      std::placeholders::_4);
        job->setVfsForceStatusCallback(vfsForceStatusCallback);
    }

    return true;
}

bool ExecutorWorker::checkLiteSyncInfoForCreate(SyncOpPtr syncOp, const SyncPath &path, bool &isDehydratedPlaceholder) {
    isDehydratedPlaceholder = false;

    if (syncOp->targetSide() == ReplicaSideRemote) {
        if (syncOp->affectedNode()->type() == NodeTypeDirectory) {
            return true;
        }

        bool isPlaceholder = false;
        bool isHydrated = false;
        bool isSyncing = false;
        int progress = 0;
        if (!_syncPal->vfsStatus(path, isPlaceholder, isHydrated, isSyncing, progress)) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in vfsStatus: " << Utility::formatSyncPath(path).c_str());
            _executorExitCode = ExitCodeSystemError;
            _executorExitCause = ExitCauseFileAccessError;
            return false;
        }

        if (isPlaceholder && !isHydrated && !isSyncing) {
            LOGW_SYNCPAL_INFO(_logger, L"Do not upload dehydrated placeholders: " << Utility::formatSyncPath(path).c_str());
            isDehydratedPlaceholder = true;
        }
    }

    return true;
}

bool ExecutorWorker::createPlaceholder(const SyncPath &relativeLocalPath) {
    SyncFileItem syncItem;
    if (!_syncPal->getSyncFileItem(relativeLocalPath, syncItem)) {
        LOGW_SYNCPAL_WARN(_logger, L"Failed to retrieve SyncFileItem associated to item: "
                                       << Utility::formatSyncPath(relativeLocalPath).c_str());
        _executorExitCode = ExitCodeDataError;
        _executorExitCause = ExitCauseUnknown;
        return false;
    }
    return _syncPal->vfsCreatePlaceholder(relativeLocalPath, syncItem);  // TODO : should not use SyncFileItem
}

bool ExecutorWorker::convertToPlaceholder(const SyncPath &relativeLocalPath, bool hydrated, bool &needRestart) {
    SyncFileItem syncItem;
    if (!_syncPal->getSyncFileItem(relativeLocalPath, syncItem)) {
        LOGW_SYNCPAL_WARN(_logger, L"Failed to retrieve SyncFileItem associated to item: "
                                       << Utility::formatSyncPath(relativeLocalPath).c_str());
        _executorExitCode = ExitCodeDataError;
        _executorExitCause = ExitCauseUnknown;
        return false;
    }

    syncItem.setDehydrated(!hydrated);

    SyncPath absoluteLocalFilePath = _syncPal->_localPath / relativeLocalPath;

    // Update the local ID
    FileStat fileStat;
    IoError ioError = IoErrorSuccess;
    if (!IoHelper::getFileStat(absoluteLocalFilePath, &fileStat, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::getFileStat: " << Utility::formatIoError(absoluteLocalFilePath, ioError).c_str());
        _executorExitCode = ExitCodeSystemError;
        _executorExitCause = ExitCauseUnknown;
        return false;
    }

    if (ioError == IoErrorNoSuchFileOrDirectory) {
        LOGW_WARN(_logger, L"Item does not exist anymore: " << Utility::formatSyncPath(absoluteLocalFilePath).c_str());
        _executorExitCode = ExitCodeDataError;
        _executorExitCause = ExitCauseInvalidSnapshot;
        return false;
    } else if (ioError == IoErrorAccessDenied) {
        LOGW_WARN(_logger, L"Item misses search permission: " << Utility::formatSyncPath(absoluteLocalFilePath).c_str());
        _executorExitCode = ExitCodeSystemError;
        _executorExitCause = ExitCauseNoSearchPermission;
        return false;
    }

    syncItem.setLocalNodeId(std::to_string(fileStat.inode));

    if (!_syncPal->vfsConvertToPlaceholder(absoluteLocalFilePath, syncItem,
                                           needRestart)) {  // TODO : should not use SyncFileItem
        return false;
    }

    if (!_syncPal->vfsSetPinState(absoluteLocalFilePath, hydrated ? PinStateAlwaysLocal : PinStateOnlineOnly)) {
        LOGW_WARN(_logger, L"Error in vfsSetPinState: " << Utility::formatSyncPath(absoluteLocalFilePath).c_str());
        return false;
    }

    return true;
}

void ExecutorWorker::handleEditOp(SyncOpPtr syncOp, std::shared_ptr<AbstractJob> &job, bool &hasError) {
    // The execution of the edit operation consists of three steps:
    // 1. If omit-flag is False, propagate the file to replicaY, replacing the existing one.
    // 2. Insert a new entry into the database, to avoid that the object is detected again by compute_ops() on the next sync
    // iteration.
    // 3. If the omit flag is False, update the updatetreeY structure to ensure that follow-up operations can execute correctly,
    // as they are based on the information in this structure.
    SyncPath relativeLocalFilePath = syncOp->affectedNode()->getPath();

    if (relativeLocalFilePath.empty()) {
        _executorExitCode = ExitCodeDataError;
        _executorExitCause = ExitCauseUnknown;
        hasError = true;
        return;
    }

    if (isLiteSyncActivated()) {
        SyncPath absoluteLocalFilePath = _syncPal->_localPath / relativeLocalFilePath;
        bool ignoreItem = false;
        bool isSyncing = false;
        if (!checkLiteSyncInfoForEdit(syncOp, absoluteLocalFilePath, ignoreItem, isSyncing)) {
            hasError = true;
            return;
        }
        if (ignoreItem || isSyncing) {
            _syncPal->setProgressComplete(relativeLocalFilePath, ignoreItem ? SyncFileStatusIgnored : SyncFileStatusSyncing);
            return;
        }
    }

    if (syncOp->omit()) {
        // Do not generate job, only push changes in DB and update tree
        std::shared_ptr<Node> node;
        if (!propagateEditToDbAndTree(
                syncOp, syncOp->correspondingNode()->id().has_value() ? *syncOp->correspondingNode()->id() : std::string(),
                syncOp->affectedNode()->lastmodified(), node)) {
            LOGW_SYNCPAL_WARN(_logger, L"Failed to propagate changes in DB or update tree for: "
                                           << SyncName2WStr(syncOp->affectedNode()->name()).c_str());
            _syncPal->setProgressComplete(relativeLocalFilePath, SyncFileStatusError);
            hasError = true;
            return;
        }
    } else {
        if (!enoughLocalSpace(syncOp)) {
            _executorExitCode = ExitCodeSystemError;
            _executorExitCause = ExitCauseNotEnoughDiskSpace;
            _syncPal->addError(Error(_syncPal->syncDbId(), name(), _executorExitCode, _executorExitCause));
            hasError = true;
            return;
        }

        bool exists = false;
        if (!hasRight(syncOp, exists)) {
            // Ignore operation
            _syncPal->setProgressComplete(
                relativeLocalFilePath,
                SyncFileStatusIgnored);  // TODO: we do not want to propagate the changes, but they must be kept on local replica.
            // Rename the file and exclude the renamed file
            return;
        }

        if (!generateEditJob(syncOp, job)) {
            hasError = true;
            return;
        }
    }
}

bool ExecutorWorker::generateEditJob(SyncOpPtr syncOp, std::shared_ptr<AbstractJob> &job) {
    // 1. If omit-flag is False, propagate the file to replicaY, replacing the existing one.
    if (syncOp->targetSide() == ReplicaSideLocal) {
        SyncPath relativeLocalFilePath = syncOp->affectedNode()->getPath();
        SyncPath absoluteLocalFilePath = _syncPal->_localPath / relativeLocalFilePath;

        try {
            job = std::make_shared<DownloadJob>(
                _syncPal->_driveDbId, syncOp->affectedNode()->id() ? *syncOp->affectedNode()->id() : std::string(),
                absoluteLocalFilePath, syncOp->affectedNode()->size(),
                syncOp->affectedNode()->createdAt().has_value() ? *syncOp->affectedNode()->createdAt() : 0,
                syncOp->affectedNode()->lastmodified().has_value() ? *syncOp->affectedNode()->lastmodified() : 0, false);
        } catch (std::exception const &e) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in DownloadJob::DownloadJob for driveDbId=" << _syncPal->_driveDbId << L" : "
                                                                                           << Utility::s2ws(e.what()).c_str());
            _executorExitCode = ExitCodeDataError;
            _executorExitCause = ExitCauseUnknown;
            return false;
        }

        job->setAffectedFilePath(relativeLocalFilePath);

        // Set callbacks
        std::shared_ptr<DownloadJob> downloadJob = std::dynamic_pointer_cast<DownloadJob>(job);

        if (_syncPal->_vfsMode == VirtualFileModeMac || _syncPal->_vfsMode == VirtualFileModeWin) {
            std::function<bool(const SyncPath &, PinState)> vfsSetPinStateCallback =
                std::bind(&SyncPal::vfsSetPinState, _syncPal, std::placeholders::_1, std::placeholders::_2);
            downloadJob->setVfsSetPinStateCallback(vfsSetPinStateCallback);

            std::function<bool(const SyncPath &, bool, int, bool)> vfsForceStatusCallback =
                std::bind(&SyncPal::vfsForceStatus, _syncPal, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                          std::placeholders::_4);
            downloadJob->setVfsForceStatusCallback(vfsForceStatusCallback);

            std::function<bool(const SyncPath &, const SyncTime &, const SyncTime &, const int64_t, const NodeId &,
                               std::string &)>
                vfsUpdateMetadataCallback =
                    std::bind(&SyncPal::vfsUpdateMetadata, _syncPal, std::placeholders::_1, std::placeholders::_2,
                              std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6);
            downloadJob->setVfsUpdateMetadataCallback(vfsUpdateMetadataCallback);
        }
    } else {
        SyncPath relativeLocalFilePath = syncOp->affectedNode()->getPath();
        SyncPath absoluteLocalFilePath = _syncPal->_localPath / relativeLocalFilePath;

        uint64_t filesize;
        if (!getFileSize(absoluteLocalFilePath, filesize)) {
            LOGW_WARN(_logger,
                      L"Error in ExecutorWorker::getFileSize for " << Utility::formatSyncPath(absoluteLocalFilePath).c_str());
            return false;
        }

        if (filesize > useUploadSessionThreshold) {
            try {
                int uploadSessionParallelJobs = ParametersCache::instance()->parameters().uploadSessionParallelJobs();
                job = std::make_shared<DriveUploadSession>(
                    _syncPal->_driveDbId, _syncPal->_syncDb, absoluteLocalFilePath, syncOp->affectedNode()->name(),
                    syncOp->correspondingNode()->parentNode()->id() ? *syncOp->correspondingNode()->parentNode()->id()
                                                                    : std::string(),
                    syncOp->affectedNode()->lastmodified() ? *syncOp->affectedNode()->lastmodified() : 0, isLiteSyncActivated(),
                    uploadSessionParallelJobs);
            } catch (std::exception const &e) {
                LOGW_SYNCPAL_WARN(_logger,
                                  L"Error in DriveUploadSession::DriveUploadSession: " << Utility::s2ws(e.what()).c_str());
                _executorExitCode = ExitCodeDataError;
                _executorExitCause = ExitCauseUnknown;
                return false;
            };
        } else {
            if (!syncOp->correspondingNode()->id()) {
                // Should not happen
                LOGW_SYNCPAL_WARN(_logger, L"Edit operation with empty corresponding node id for "
                                               << Utility::formatSyncPath(absoluteLocalFilePath).c_str());
                _executorExitCode = ExitCodeDataError;
                _executorExitCause = ExitCauseUnknown;

#ifdef NDEBUG
                sentry_capture_event(sentry_value_new_message_event(SENTRY_LEVEL_WARNING, "ExecutorWorker::generateEditJob",
                                                                    "Edit operation with empty corresponding node id"));
#endif

                return false;
            }

            try {
                job = std::make_shared<UploadJob>(
                    _syncPal->_driveDbId, absoluteLocalFilePath,
                    syncOp->correspondingNode()->id() ? *syncOp->correspondingNode()->id() : std::string(),
                    syncOp->affectedNode()->lastmodified() ? *syncOp->affectedNode()->lastmodified() : 0);
            } catch (std::exception const &e) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in UploadJob::UploadJob for driveDbId=" << _syncPal->_driveDbId << L" : "
                                                                                           << Utility::s2ws(e.what()).c_str());
                _executorExitCode = ExitCodeDataError;
                _executorExitCause = ExitCauseUnknown;
                return false;
            }

            // Set callbacks
            std::shared_ptr<UploadJob> uploadJob = std::dynamic_pointer_cast<UploadJob>(job);
            if (_syncPal->_vfsMode == VirtualFileModeMac || _syncPal->_vfsMode == VirtualFileModeWin) {
                std::function<bool(const SyncPath &, bool, int, bool)> vfsForceStatusCallback =
                    std::bind(&SyncPal::vfsForceStatus, _syncPal, std::placeholders::_1, std::placeholders::_2,
                              std::placeholders::_3, std::placeholders::_4);
                uploadJob->setVfsForceStatusCallback(vfsForceStatusCallback);
            }
        }

        job->setAffectedFilePath(relativeLocalFilePath);
    }

    return true;
}

bool ExecutorWorker::fixModificationDate(SyncOpPtr syncOp, const SyncPath &absolutePath) {
    const auto id = syncOp->affectedNode()->id().has_value() ? syncOp->affectedNode()->id().value() : "";
    LOGW_SYNCPAL_DEBUG(_logger, L"Do not upload dehydrated placeholders: " << Utility::formatSyncPath(absolutePath).c_str()
                                                                           << L" (" << Utility::s2ws(id).c_str() << L")");

    // Update last modification date in order to avoid generating more EDIT operations.
    bool found = false;
    DbNode dbNode;
    if (!_syncPal->_syncDb->node(*syncOp->correspondingNode()->idb(), dbNode, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::node");
        _executorExitCode = ExitCodeDbError;
        _executorExitCause = ExitCauseDbAccessError;
        return false;
    }
    if (!found) {
        LOG_SYNCPAL_DEBUG(_logger, "Failed to retrieve node for dbId=" << *syncOp->correspondingNode()->idb());
        _executorExitCode = ExitCodeDataError;
        _executorExitCause = ExitCauseDbEntryNotFound;
        return false;
    }

    bool exists = false;
    if (!Utility::setFileDates(absolutePath, dbNode.created(), dbNode.lastModifiedRemote(), false, exists)) {
        LOGW_SYNCPAL_WARN(_logger, L"Error in Utility::setFileDates: " << Utility::formatSyncPath(absolutePath).c_str());
    }
    if (exists) {
        LOGW_SYNCPAL_INFO(_logger,
                          L"Last modification date updated locally to avoid further wrongly generated EDIT operations for file: "
                              << Utility::formatSyncPath(absolutePath).c_str());
    }
    // If file does not exist anymore, do nothing special. This is fine, it will not generate EDIT operations anymore.
    return true;
}

bool ExecutorWorker::checkLiteSyncInfoForEdit(SyncOpPtr syncOp, const SyncPath &absolutePath, bool &ignoreItem, bool &isSyncing) {
    ignoreItem = false;

    bool isPlaceholder = false;
    bool isHydrated = false;
    bool isSyncingTmp = false;
    int progress = 0;
    if (!_syncPal->vfsStatus(absolutePath, isPlaceholder, isHydrated, isSyncingTmp, progress)) {
        LOGW_SYNCPAL_WARN(_logger, L"Error in vfsStatus: " << Utility::formatSyncPath(absolutePath).c_str());
        _executorExitCode = ExitCodeSystemError;
        _executorExitCause = ExitCauseFileAccessError;
        return false;
    }

    if (syncOp->targetSide() == ReplicaSideRemote) {
        if (isPlaceholder && !isHydrated) {
            ignoreItem = true;
            return fixModificationDate(syncOp, absolutePath);
        }
    } else {
        if (isPlaceholder) {
            PinState pinState = PinStateUnspecified;
            if (!_syncPal->vfsPinState(absolutePath, pinState)) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in vfsPinState for file: " << Utility::formatSyncPath(absolutePath).c_str());
                _executorExitCode = ExitCodeSystemError;
                _executorExitCause = ExitCauseInconsistentPinState;
                return false;
            }

            switch (pinState) {
                case PinStateInherited: {
                    // TODO : what do we do in that case??
                    LOG_SYNCPAL_WARN(_logger, "Inherited pin state not implemented yet");
                    _executorExitCode = ExitCodeDataError;
                    _executorExitCause = ExitCauseUnknown;
                    return false;
                }
                case PinStateAlwaysLocal: {
                    if (isSyncingTmp) {
                        // Ignore this item until it is synchronized
                        isSyncing = true;
                    } else if (isHydrated) {
                        // Download
                    }
                    break;
                }
                case PinStateOnlineOnly: {
                    // Update metadata
                    std::string error;
                    _syncPal->vfsUpdateMetadata(
                        absolutePath, syncOp->affectedNode()->createdAt().has_value() ? *syncOp->affectedNode()->createdAt() : 0,
                        syncOp->affectedNode()->lastmodified().has_value() ? *syncOp->affectedNode()->lastmodified() : 0,
                        syncOp->affectedNode()->size(),
                        syncOp->affectedNode()->id().has_value() ? *syncOp->affectedNode()->id() : std::string(), error);
                    syncOp->setOmit(true);  // Do not propagate change in file system, only in DB
                    break;
                }
                case PinStateUnspecified:
                default: {
                    LOGW_SYNCPAL_DEBUG(_logger, L"Ignore EDIT for file: " << Path2WStr(absolutePath).c_str());
                    ignoreItem = true;
                    return true;
                }
            }
        }
    }

    return true;
}

void ExecutorWorker::handleMoveOp(SyncOpPtr syncOp, bool &hasError) {
    // The three execution steps are as follows:
    // 1. If omit-flag is False, move the object on replica Y (where it still needs to be moved) from uY to vY, changing the name
    // to nameX.
    // 2. Update the database entry, to avoid detecting the move operation again.
    // 3. If the omit flag is False, update the updatetreeY structure to ensure that follow-up operations can execute correctly,
    // as they are based on the information in this structure.

    SyncPath relativePath = syncOp->affectedNode()->getPath();

    if (syncOp->omit()) {
        // Do not generate job, only push changes in DB and update tree
        if (syncOp->hasConflict()) {
            bool propagateChange = true;
            hasError = propagateConflictToDbAndTree(syncOp, propagateChange);

            Error err(
                _syncPal->syncDbId(),
                syncOp->conflict().localNode() != nullptr
                    ? (syncOp->conflict().localNode()->id().has_value() ? syncOp->conflict().localNode()->id().value() : "")
                    : "",
                syncOp->conflict().remoteNode() != nullptr
                    ? (syncOp->conflict().remoteNode()->id().has_value() ? syncOp->conflict().remoteNode()->id().value() : "")
                    : "",
                syncOp->conflict().localNode() != nullptr ? syncOp->conflict().localNode()->type() : NodeTypeUnknown,
                syncOp->affectedNode()->moveOrigin().has_value() ? syncOp->affectedNode()->moveOrigin().value()
                                                                 : syncOp->affectedNode()->getPath(),
                syncOp->conflict().type());

            _syncPal->addError(err);

            if (!propagateChange || hasError) {
                return;
            }
        }

        if (!propagateMoveToDbAndTree(syncOp)) {
            LOGW_SYNCPAL_WARN(_logger, L"Failed to propagate changes in DB or update tree for: "
                                           << SyncName2WStr(syncOp->affectedNode()->name()).c_str());
            _syncPal->setProgressComplete(relativePath, SyncFileStatusError);
            hasError = true;
            return;
        }
    } else {
        bool exists = false;
        if (!hasRight(syncOp, exists)) {
            // Ignore operation
            _syncPal->setProgressComplete(relativePath,
                                          SyncFileStatusIgnored);  // TODO : do not exclude the item from sync, rollback changes
            return;
        }

        if (!generateMoveJob(syncOp)) {
            hasError = true;
            return;
        }
    }
}

bool ExecutorWorker::generateMoveJob(SyncOpPtr syncOp) {
    // 1. If omit-flag is False, move the object on replica Y (where it still needs to be moved) from uY to vY, changing the name
    // to nameX.

    std::shared_ptr<Node> correspondingNode =
        syncOp->correspondingNode() ? syncOp->correspondingNode() : syncOp->affectedNode();  // No corresponding node => rename

    // Get the new parent node
    std::shared_ptr<Node> parentNode = syncOp->newParentNode() ? syncOp->newParentNode() : syncOp->affectedNode()->parentNode();
    if (!parentNode) {
        LOGW_SYNCPAL_WARN(_logger, L"Parent node not found for item " << Path2WStr(correspondingNode->getPath()).c_str());
        return false;
    }

    std::shared_ptr<AbstractJob> job = nullptr;
    SyncPath relativeDestLocalFilePath = parentNode->getPath() / syncOp->newName();
    SyncPath relativeSourceLocalFilePath = correspondingNode->getPath();
    SyncPath absoluteDestLocalFilePath = _syncPal->_localPath / relativeDestLocalFilePath;

    if (syncOp->targetSide() == ReplicaSideLocal) {
        SyncPath absoluteSourceLocalFilePath = _syncPal->_localPath / relativeSourceLocalFilePath;
        job = std::make_shared<LocalMoveJob>(absoluteSourceLocalFilePath, absoluteDestLocalFilePath);
    } else {
        try {
            if (relativeSourceLocalFilePath.parent_path() == relativeDestLocalFilePath.parent_path()) {
                // This is just a rename
                job = std::make_shared<RenameJob>(_syncPal->_driveDbId,
                                                  correspondingNode->id().has_value() ? *correspondingNode->id() : std::string(),
                                                  absoluteDestLocalFilePath);
            } else {
                // This is a move

                // For all conflict involving an "undo move" operation, the correct parent is already stored in the syncOp
                std::shared_ptr<Node> remoteParentNode = syncOp->conflict().type() == ConflictTypeMoveParentDelete ||
                                                                 syncOp->conflict().type() == ConflictTypeMoveMoveSource ||
                                                                 syncOp->conflict().type() == ConflictTypeMoveMoveCycle
                                                             ? parentNode
                                                             : correspondingNodeInOtherTree(parentNode);
                if (!remoteParentNode) {
                    LOGW_SYNCPAL_WARN(_logger, L"Parent node not found for item " << Path2WStr(parentNode->getPath()).c_str());
                    return false;
                }
                job = std::make_shared<MoveJob>(_syncPal->_driveDbId, absoluteDestLocalFilePath,
                                                correspondingNode->id().has_value() ? *correspondingNode->id() : std::string(),
                                                remoteParentNode->id().has_value() ? *remoteParentNode->id() : std::string(),
                                                syncOp->newName());
            }

            if (syncOp->hasConflict()) {
                job->setBypassCheck(true);
            }

            // Set callbacks
            if (_syncPal->_vfsMode == VirtualFileModeMac || _syncPal->_vfsMode == VirtualFileModeWin) {
                std::function<bool(const SyncPath &, bool, int, bool)> vfsForceStatusCallback =
                    std::bind(&SyncPal::vfsForceStatus, _syncPal, std::placeholders::_1, std::placeholders::_2,
                              std::placeholders::_3, std::placeholders::_4);
                job->setVfsForceStatusCallback(vfsForceStatusCallback);

                std::function<bool(const SyncPath &, bool &, bool &, bool &, int &)> vfsStatusCallback =
                    std::bind(&SyncPal::vfsStatus, _syncPal, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                              std::placeholders::_4, std::placeholders::_5);
                job->setVfsStatusCallback(vfsStatusCallback);
            }
        } catch (std::exception const &e) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in MoveJob::MoveJob for driveDbId=" << _syncPal->_driveDbId << L" : "
                                                                                   << Utility::s2ws(e.what()).c_str());
            _executorExitCode = ExitCodeDataError;
            _executorExitCause = ExitCauseUnknown;
            return false;
        }
    }

    job->setAffectedFilePath(relativeDestLocalFilePath);
    job->runSynchronously();

    if (job->exitCode() == ExitCodeOk && syncOp->conflict().type() != ConflictTypeNone) {
        // Conflict fixing job finished successfully
        // Propagate changes to DB and update trees
        std::shared_ptr<Node> newNode = nullptr;
        if (!propagateChangeToDbAndTree(syncOp, job, newNode)) {
            cancelAllOngoingJobs();
            _syncPal->setProgressComplete(relativeDestLocalFilePath, SyncFileStatusError);
            return false;
        }

        // Send conflict notification
        SyncFileItem syncItem;
        if (_syncPal->getSyncFileItem(syncOp->affectedNode()->getPath(), syncItem)) {
            NodeId localNodeId = syncOp->correspondingNode()->side() == ReplicaSideLocal
                                     ? syncItem.localNodeId().has_value() ? syncItem.localNodeId().value() : ""
                                     : "";
            NodeId remoteNodeId = syncOp->correspondingNode()->side() == ReplicaSideLocal ? ""
                                  : syncItem.remoteNodeId().has_value()                   ? syncItem.remoteNodeId().value()
                                                                                          : "";

            Error err(_syncPal->syncDbId(), localNodeId, remoteNodeId, syncItem.type(),
                      syncItem.newPath().has_value() ? syncItem.newPath().value() : syncItem.path(), syncItem.conflict(),
                      syncItem.inconsistency(), CancelTypeNone,
                      localNodeId.empty() ? relativeDestLocalFilePath : absoluteDestLocalFilePath);
            _syncPal->addError(err);
        }

        _syncPal->setProgressComplete(relativeSourceLocalFilePath, SyncFileStatusSuccess);
        return true;
    }

    return handleFinishedJob(job, syncOp, syncOp->affectedNode()->getPath());
}

void ExecutorWorker::handleDeleteOp(SyncOpPtr syncOp, bool &hasError) {
    // The three execution steps are as follows:
    // 1. If omit-flag is False, delete the file or directory on replicaY, because the objects till exists there
    // 2. Remove the entry from the database. If nX is a directory node, also remove all entries for each node n ∈ S. This avoids
    // that the object(s) are detected again by compute_ops() on the next sync iteration
    // 3. Update the update tree structures to ensure that follow-up operations can execute correctly, as they are based on the
    // information in these structures
    SyncPath relativePath = syncOp->affectedNode()->getPath();
    if (syncOp->omit()) {
        // Do not generate job, only push changes in DB and update tree
        if (syncOp->hasConflict() &&
            syncOp->conflict().type() !=
                ConflictTypeEditDelete) {  // Error message handled with move operation in case Edit-Delete conflict
            bool propagateChange = true;
            hasError = propagateConflictToDbAndTree(syncOp, propagateChange);

            Error err(
                _syncPal->syncDbId(),
                syncOp->conflict().localNode() != nullptr
                    ? (syncOp->conflict().localNode()->id().has_value() ? syncOp->conflict().localNode()->id().value() : "")
                    : "",
                syncOp->conflict().remoteNode() != nullptr
                    ? (syncOp->conflict().remoteNode()->id().has_value() ? syncOp->conflict().remoteNode()->id().value() : "")
                    : "",
                syncOp->conflict().localNode() != nullptr ? syncOp->conflict().localNode()->type() : NodeTypeUnknown,
                syncOp->affectedNode()->moveOrigin().has_value() ? syncOp->affectedNode()->moveOrigin().value()
                                                                 : syncOp->affectedNode()->getPath(),
                syncOp->conflict().type());

            _syncPal->addError(err);

            if (!propagateChange || hasError) {
                return;
            }
        }

        if (!propagateDeleteToDbAndTree(syncOp)) {
            LOGW_SYNCPAL_WARN(_logger, L"Failed to propagate changes in DB or update tree for: "
                                           << SyncName2WStr(syncOp->affectedNode()->name()).c_str());
            _syncPal->setProgressComplete(relativePath, SyncFileStatusError);
            hasError = true;
            return;
        }
    } else {
        bool exists = false;
        if (!hasRight(syncOp, exists)) {
            // Ignore operation
            _syncPal->setProgressComplete(relativePath,
                                          SyncFileStatusIgnored);  // TODO : do not exclude item from sync, rollback
            return;
        }

        if (!generateDeleteJob(syncOp)) {
            hasError = true;
            return;
        }
    }
}

bool ExecutorWorker::generateDeleteJob(SyncOpPtr syncOp) {
    // 1. If omit-flag is False, delete the file or directory on replicaY, because the objects till exists there
    std::shared_ptr<AbstractJob> job = nullptr;
    SyncPath relativeLocalFilePath = syncOp->correspondingNode()->getPath();
    SyncPath absoluteLocalFilePath = _syncPal->_localPath / relativeLocalFilePath;
    if (syncOp->targetSide() == ReplicaSideLocal) {
        bool isDehydratedPlaceholder = false;
        if (_syncPal->vfsMode() != VirtualFileModeOff) {
            bool isPlaceholder = false;
            bool isHydrated = false;
            bool isSyncing = false;
            int progress = 0;
            if (!_syncPal->vfsStatus(absoluteLocalFilePath, isPlaceholder, isHydrated, isSyncing, progress)) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in vfsStatus: " << Utility::formatSyncPath(absoluteLocalFilePath).c_str());
                _executorExitCode = ExitCodeSystemError;
                _executorExitCause = ExitCauseFileAccessError;
                return false;
            }
            isDehydratedPlaceholder = isPlaceholder && !isHydrated;
        }

        NodeId remoteNodeId = syncOp->affectedNode()->id().has_value() ? syncOp->affectedNode()->id().value() : "";
        if (remoteNodeId.empty()) {
            LOGW_SYNCPAL_WARN(_logger, L"Failed to retrieve node ID");
            _executorExitCode = ExitCodeDataError;
            _executorExitCause = ExitCauseUnknown;
            return false;
        }
        job = std::make_shared<LocalDeleteJob>(_syncPal->_driveDbId, _syncPal->_localPath, relativeLocalFilePath,
                                               isDehydratedPlaceholder, remoteNodeId);
    } else {
        try {
            job = std::make_shared<DeleteJob>(
                _syncPal->_driveDbId, syncOp->correspondingNode()->id() ? *syncOp->correspondingNode()->id() : std::string(),
                syncOp->affectedNode()->id() ? *syncOp->affectedNode()->id() : std::string(), absoluteLocalFilePath);
        } catch (std::exception const &e) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in DeleteJob::DeleteJob for driveDbId=" << _syncPal->_driveDbId << L" : "
                                                                                       << Utility::s2ws(e.what()).c_str());
            _executorExitCode = ExitCodeDataError;
            _executorExitCause = ExitCauseUnknown;
            return false;
        }
    }

    // If affected node has both create and delete events (node deleted and re-created with same name), then do not check
    job->setBypassCheck((syncOp->affectedNode()->hasChangeEvent(OperationTypeCreate) &&
                         syncOp->affectedNode()->hasChangeEvent(OperationTypeDelete)) ||
                        syncOp->affectedNode()->isSharedFolder());

    job->setAffectedFilePath(relativeLocalFilePath);
    job->runSynchronously();
    return handleFinishedJob(job, syncOp, relativeLocalFilePath);
}

bool ExecutorWorker::hasRight(SyncOpPtr syncOp, bool &exists) {
    std::shared_ptr<Node> correspondingNode =
        syncOp->correspondingNode() ? syncOp->correspondingNode() : syncOp->affectedNode();  // No corresponding node => rename

    // Check if file exists
    SyncPath relativeLocalFilePath =
        (syncOp->type() == OperationTypeCreate ? syncOp->affectedNode()->getPath() : correspondingNode->getPath());
    SyncPath absoluteLocalFilePath = _syncPal->_localPath / relativeLocalFilePath;

    bool readPermission = false;
    bool writePermission = false;
    bool execPermission = false;
    IoError ioError = IoErrorSuccess;
    if (!IoHelper::getRights(absoluteLocalFilePath, readPermission, writePermission, execPermission, ioError)) {
        LOGW_WARN(_logger, L"Error in Utility::getRights: " << Utility::formatSyncPath(absoluteLocalFilePath).c_str());
        return false;
    }
    exists = ioError != IoErrorNoSuchFileOrDirectory;

    if (syncOp->targetSide() == ReplicaSideLocal) {
        switch (syncOp->type()) {
            case OperationTypeCreate: {
                if (exists && !writePermission) {
                    LOGW_SYNCPAL_DEBUG(_logger, L"Item: " << Utility::formatSyncPath(absoluteLocalFilePath).c_str()
                                                          << L" already exists but doesn't have write permissions!");
                    Error error(_syncPal->_syncDbId, "", "", NodeTypeDirectory, absoluteLocalFilePath, ConflictTypeNone,
                                InconsistencyTypeNone, CancelTypeNone, "", ExitCodeSystemError, ExitCauseFileAccessError);
                    _syncPal->addError(error);
                    return false;
                }
                break;
            }
            case OperationTypeEdit:
            case OperationTypeMove:
            case OperationTypeDelete: {
                if (!exists) {
                    LOGW_SYNCPAL_WARN(_logger, L"Item: " << Utility::formatSyncPath(absoluteLocalFilePath).c_str()
                                                         << L" doesn't exist anymore!");
                    return false;
                }

                if (!writePermission) {
                    Error error(_syncPal->_syncDbId, "", "", NodeTypeDirectory, absoluteLocalFilePath, ConflictTypeNone,
                                InconsistencyTypeNone, CancelTypeNone, "", ExitCodeSystemError, ExitCauseFileAccessError);
                    _syncPal->addError(error);
                    LOGW_SYNCPAL_DEBUG(_logger, L"Item: " << Utility::formatSyncPath(absoluteLocalFilePath).c_str()
                                                          << L" doesn't have write permissions!");
                    return false;
                }
                break;
            }
            case OperationTypeNone:
            default: {
                break;
            }
        }
    } else if (syncOp->targetSide() == ReplicaSideRemote) {
        switch (syncOp->type()) {
            case OperationTypeCreate: {
                if (!exists) {
                    LOGW_SYNCPAL_WARN(
                        _logger, L"File/directory " << Path2WStr(absoluteLocalFilePath).c_str() << L" doesn't exist anymore!");
                    return false;
                }

                if (!writePermission) {
                    LOGW_SYNCPAL_DEBUG(_logger, L"Item: " << Utility::formatSyncPath(absoluteLocalFilePath).c_str()
                                                          << L" doesn't have write permissions!");
                    return false;
                }

                std::shared_ptr<Node> newCorrespondingParentNode = nullptr;
                if (affectedUpdateTree(syncOp)->rootNode() == syncOp->affectedNode()->parentNode()) {
                    newCorrespondingParentNode = targetUpdateTree(syncOp)->rootNode();
                } else {
                    newCorrespondingParentNode = correspondingNodeInOtherTree(syncOp->affectedNode()->parentNode());
                }

                if (!newCorrespondingParentNode || !newCorrespondingParentNode->id().has_value()) {
                    return false;
                }

                if (newCorrespondingParentNode->isCommonDocumentsFolder() &&
                    syncOp->affectedNode()->type() == NodeTypeDirectory) {
                    return true;
                }

                if (newCorrespondingParentNode->isSharedFolder()) {
                    return false;
                }

                return _syncPal->_remoteSnapshot->canWrite(*newCorrespondingParentNode->id());
            }
            case OperationTypeEdit: {
                if (!exists) {
                    LOGW_SYNCPAL_WARN(_logger, L"Item: " << Utility::formatSyncPath(absoluteLocalFilePath).c_str()
                                                         << L" doesn't exist anymore!");
                    return false;
                }

                if (!correspondingNode->id().has_value()) {
                    return false;
                }

                return _syncPal->_remoteSnapshot->canWrite(*correspondingNode->id());
            }
            case OperationTypeMove:
            case OperationTypeDelete: {
                break;
            }
            case OperationTypeNone:
            default: {
                break;
            }
        }
    }
    return true;
}

bool ExecutorWorker::enoughLocalSpace(SyncOpPtr syncOp) {
    if (syncOp->targetSide() != ReplicaSideLocal) {
        // This is not a download operation
        return true;
    }

    if (syncOp->affectedNode()->type() != NodeTypeFile) {
        return true;
    }

    int64_t newSize = syncOp->affectedNode()->size();
    if (syncOp->type() == OperationTypeEdit) {
        // Keep only the difference between remote size and local size
        newSize -= syncOp->correspondingNode()->size();
    }

    const int64_t freeBytes = Utility::freeDiskSpace(_syncPal->_localPath);
    if (freeBytes >= 0) {
        if (freeBytes < newSize + Utility::freeDiskSpaceLimit()) {
            LOGW_SYNCPAL_WARN(_logger, L"Disk almost full, only " << freeBytes << L"B available at "
                                                                  << Utility::formatSyncPath(_syncPal->_localPath).c_str()
                                                                  << L". Synchronization canceled.");
            return false;
        }
    } else {
        LOGW_SYNCPAL_WARN(
            _logger, L"Could not determine free space available at " << Utility::formatSyncPath(_syncPal->_localPath).c_str());
    }

    return true;
}

void ExecutorWorker::waitForAllJobsToFinish(bool &hasError) {
    while (!_ongoingJobs.empty()) {
        if (stopAsked()) {
            cancelAllOngoingJobs();
            break;
        }

        while (pauseAsked() || isPaused()) {
            if (!isPaused()) {
                setPauseDone();
                cancelAllOngoingJobs(true);
            }

            Utility::msleep(LOOP_PAUSE_SLEEP_PERIOD);

            if (unpauseAsked()) {
                setUnpauseDone();
            }
        }

        if (!deleteFinishedAsyncJobs()) {
            hasError = true;
            cancelAllOngoingJobs();
            break;
        }

        sendProgress();
    }
}

bool ExecutorWorker::deleteFinishedAsyncJobs() {
    bool hasError = false;
    while (!_terminatedJobs.empty()) {
        // Delete all terminated jobs
        if (!hasError && _ongoingJobs.find(_terminatedJobs.front()) != _ongoingJobs.end()) {
            auto onGoingJobIt = _ongoingJobs.find(_terminatedJobs.front());
            if (onGoingJobIt == _ongoingJobs.end()) {
                LOGW_SYNCPAL_WARN(_logger, L"Terminated job not found");
                _terminatedJobs.pop();
                continue;
            }

            std::shared_ptr<AbstractJob> job = onGoingJobIt->second;

            auto jobToSyncOpIt = _jobToSyncOpMap.find(job->jobId());
            if (jobToSyncOpIt == _jobToSyncOpMap.end()) {
                LOGW_SYNCPAL_WARN(_logger, L"Sync Operation not found");
                _ongoingJobs.erase(job->jobId());
                _terminatedJobs.pop();
                continue;
            }

            SyncOpPtr syncOp = jobToSyncOpIt->second;
            SyncPath relativeLocalPath = syncOp->affectedNode()->getPath();
            if (!handleFinishedJob(job, syncOp, relativeLocalPath)) {
                increaseErrorCount(syncOp);
                hasError = true;
            }

            if (!hasError) {
                if (syncOp->affectedNode()->id().has_value()) {
                    std::unordered_set<NodeId> whiteList;
                    SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), SyncNodeTypeWhiteList, whiteList);
                    if (whiteList.find(syncOp->affectedNode()->id().value()) != whiteList.end()) {
                        // This item has been synchronized, it can now be removed from white list
                        whiteList.erase(syncOp->affectedNode()->id().value());
                        SyncNodeCache::instance()->update(_syncPal->syncDbId(), SyncNodeTypeWhiteList, whiteList);
                    }
                }
            }

            // Delete job
            _ongoingJobs.erase(_terminatedJobs.front());
        }
        _terminatedJobs.pop();
    }

    return !hasError;
}

bool ExecutorWorker::handleManagedBackError(ExitCause jobExitCause, SyncOpPtr syncOp, bool isInconsistencyIssue,
                                            bool downloadImpossible) {
    _executorExitCode = ExitCodeOk;

    if (jobExitCause == ExitCauseNotFound && !downloadImpossible) {
        // The operation failed because the destination does not exist anymore
        _executorExitCode = ExitCodeDataError;
        LOG_DEBUG(_logger, "Destination does not exist anymore, restarting sync.");
        return false;
    }

    if (jobExitCause == ExitCauseQuotaExceeded) {
        _syncPal->pause();
    } else {
        // The item should be temporarily blacklisted
        _syncPal->blacklistTemporarily(syncOp->affectedNode()->id() ? *syncOp->affectedNode()->id() : std::string(),
                                       syncOp->affectedNode()->getPath(), otherSide(syncOp->targetSide()));
    }

    affectedUpdateTree(syncOp)->deleteNode(syncOp->affectedNode());
    if (syncOp->correspondingNode()) {
        targetUpdateTree(syncOp)->deleteNode(syncOp->correspondingNode());
    }

    NodeId locaNodeId;
    NodeId remoteNodeId;
    if (syncOp->targetSide() == ReplicaSideLocal) {
        if (syncOp->correspondingNode() && syncOp->correspondingNode()->id()) locaNodeId = *syncOp->correspondingNode()->id();
        remoteNodeId = syncOp->affectedNode()->id() ? *syncOp->affectedNode()->id() : std::string();
    } else {
        locaNodeId = syncOp->affectedNode()->id() ? *syncOp->affectedNode()->id() : std::string();
        if (syncOp->correspondingNode() && syncOp->correspondingNode()->id()) remoteNodeId = *syncOp->correspondingNode()->id();
    }

    Error error;
    if (isInconsistencyIssue) {
        error = Error(_syncPal->syncDbId(), locaNodeId, remoteNodeId, syncOp->affectedNode()->type(),
                      syncOp->affectedNode()->getPath(), ConflictTypeNone, InconsistencyTypeForbiddenChar);
    } else {
        error = Error(_syncPal->syncDbId(), locaNodeId, remoteNodeId, syncOp->affectedNode()->type(),
                      syncOp->affectedNode()->getPath(), ConflictTypeNone, InconsistencyTypeNone, CancelTypeNone, "",
                      ExitCodeBackError, jobExitCause);
    }
    _syncPal->addError(error);

    return true;
}

namespace details {
bool isManagedBackError(ExitCause exitCause) {
    static const std::set<ExitCause> managedExitCauses = {ExitCauseInvalidName,   ExitCauseApiErr,
                                                          ExitCauseFileTooBig,    ExitCauseNotFound,
                                                          ExitCauseQuotaExceeded, ExitCauseUploadNotTerminated};

    return managedExitCauses.find(exitCause) != managedExitCauses.cend();
}
}  // namespace details

bool ExecutorWorker::handleFinishedJob(std::shared_ptr<AbstractJob> job, SyncOpPtr syncOp, const SyncPath &relativeLocalPath) {
    if (job->exitCode() == ExitCodeNeedRestart) {
        cancelAllOngoingJobs();
        _syncPal->_restart = true;
        _syncPal->setProgressComplete(
            relativeLocalPath,
            SyncFileStatusSuccess);  // Not really success but the file should not appear in error in Finder
        _executorExitCode = ExitCodeOk;

        return false;
    }

    NodeId locaNodeId;
    NodeId remoteNodeId;
    if (syncOp->targetSide() == ReplicaSideLocal) {
        if (syncOp->correspondingNode() && syncOp->correspondingNode()->id()) locaNodeId = *syncOp->correspondingNode()->id();
        if (syncOp->affectedNode()->id()) remoteNodeId = *syncOp->affectedNode()->id();
    } else {
        if (syncOp->affectedNode()->id()) locaNodeId = *syncOp->affectedNode()->id();
        if (syncOp->correspondingNode() && syncOp->correspondingNode()->id()) remoteNodeId = *syncOp->correspondingNode()->id();
    }

    auto networkJob(std::dynamic_pointer_cast<AbstractNetworkJob>(job));
    if (const bool isInconsistencyIssue = job->exitCause() == ExitCauseInvalidName;
        job->exitCode() == ExitCodeBackError && details::isManagedBackError(job->exitCause())) {
        return handleManagedBackError(job->exitCause(), syncOp, isInconsistencyIssue, networkJob->isDownloadImpossible());
    }

    if (job->exitCode() != ExitCodeOk) {
        if (networkJob && (networkJob->getStatusCode() == Poco::Net::HTTPResponse::HTTP_FORBIDDEN ||
                           networkJob->getStatusCode() == Poco::Net::HTTPResponse::HTTP_CONFLICT)) {
            handleForbiddenAction(syncOp, relativeLocalPath);
        } else if (job->exitCode() == ExitCodeSystemError &&
                   (job->exitCause() == ExitCauseFileAccessError || job->exitCause() == ExitCauseMoveToTrashFailed)) {
            LOGW_DEBUG(_logger, L"Item: " << Utility::formatSyncPath(relativeLocalPath).c_str()
                                          << L" doesn't have write permissions or is locked!");
            _syncPal->blacklistTemporarily(
                syncOp->affectedNode()->id().has_value() ? *syncOp->affectedNode()->id() : std::string(), relativeLocalPath,
                otherSide(syncOp->targetSide()));
            Error error(_syncPal->_syncDbId, "", "", NodeTypeDirectory, _syncPal->_localPath / relativeLocalPath,
                        ConflictTypeNone, InconsistencyTypeNone, CancelTypeNone, "", job->exitCode(), job->exitCause());
            _syncPal->addError(error);

            affectedUpdateTree(syncOp)->deleteNode(syncOp->affectedNode());
            if (syncOp->correspondingNode()) {
                targetUpdateTree(syncOp)->deleteNode(syncOp->correspondingNode());
            }
        } else {
            // Cancel all queued jobs
            _executorExitCode = job->exitCode();
            _executorExitCause = job->exitCause();
            LOGW_SYNCPAL_WARN(_logger,
                              L"Cancelling jobs. exit code: " << _executorExitCode << L" exit cause: " << _executorExitCause);
            cancelAllOngoingJobs();
            _syncPal->setProgressComplete(relativeLocalPath, SyncFileStatusError);
            return false;
        }
    } else {
        // Propagate changes to DB and update trees
        std::shared_ptr<Node> newNode;
        if (!propagateChangeToDbAndTree(syncOp, job, newNode)) {
            cancelAllOngoingJobs();
            _syncPal->setProgressComplete(relativeLocalPath, SyncFileStatusError);
            return false;
        }

        SyncFileStatus status = SyncFileStatusSuccess;
        // Check for conflict or inconsistency
        if (SyncFileItem syncItem; _syncPal->getSyncFileItem(relativeLocalPath, syncItem)) {
            if (syncOp->conflict().type() != ConflictTypeNone) {
                status = SyncFileStatusConflict;
            } else if (syncItem.inconsistency() != InconsistencyTypeNone) {
                status = SyncFileStatusInconsistency;
            }

            if (status != SyncFileStatusSuccess) {
                Error err(_syncPal->syncDbId(), syncItem.localNodeId() ? *syncItem.localNodeId() : "",
                          syncItem.remoteNodeId() ? *syncItem.remoteNodeId() : "", syncItem.type(),
                          syncItem.newPath() ? *syncItem.newPath() : syncItem.path(), syncItem.conflict(),
                          syncItem.inconsistency());
                _syncPal->addError(err);
            }
        }

        const bool bypassProgressComplete = syncOp->affectedNode()->hasChangeEvent(OperationTypeCreate) &&
                                            syncOp->affectedNode()->hasChangeEvent(
                                                OperationTypeDelete);  // TODO : If node has both create and delete events, bypass
        // progress complete. But this should be refactored
        // alongside UpdateTreeWorker::getOrCreateNodeFromPath

        if (!bypassProgressComplete) {
            _syncPal->setProgressComplete(relativeLocalPath, status);
        }
    }

    return true;
}

void ExecutorWorker::handleForbiddenAction(SyncOpPtr syncOp, const SyncPath &relativeLocalPath) {
    const SyncPath absoluteLocalFilePath = _syncPal->_localPath / relativeLocalPath;

    bool removeFromDb = true;
    SyncFileStatus status = SyncFileStatusSuccess;
    CancelType cancelType = CancelTypeNone;
    switch (syncOp->type()) {
        case OperationTypeCreate: {
            cancelType = CancelTypeCreate;
            status = SyncFileStatusIgnored;
            removeFromDb = false;
            PlatformInconsistencyCheckerUtility::renameLocalFile(absoluteLocalFilePath,
                                                                 PlatformInconsistencyCheckerUtility::SuffixTypeBlacklisted);
            break;
        }
        case OperationTypeMove: {
            // Delete the item from local replica
            NodeId remoteNodeId = syncOp->correspondingNode()->id().has_value() ? syncOp->correspondingNode()->id().value() : "";
            if (!remoteNodeId.empty()) {
                LocalDeleteJob deleteJob(_syncPal->driveDbId(), _syncPal->_localPath, relativeLocalPath, isLiteSyncActivated(),
                                         remoteNodeId);
                deleteJob.setBypassCheck(true);
                deleteJob.runSynchronously();
            }

            cancelType = CancelTypeMove;
            break;
        }
        case OperationTypeEdit: {
            // Rename the file so as not to lose any information
            SyncPath newSyncPath;
            PlatformInconsistencyCheckerUtility::renameLocalFile(
                absoluteLocalFilePath, PlatformInconsistencyCheckerUtility::SuffixTypeConflict, &newSyncPath);

            // Exclude file from sync
            if (!_syncPal->vfsFileStatusChanged(newSyncPath, SyncFileStatusIgnored)) {
                LOGW_SYNCPAL_WARN(_logger,
                                  L"Error in SyncPal::vfsFileStatusChanged: " << Utility::formatSyncPath(newSyncPath).c_str());
            }

            cancelType = CancelTypeEdit;
            break;
        }
        case OperationTypeDelete: {
            cancelType = CancelTypeDelete;
            break;
        }
        default: {
            break;
        }
    }

    SyncFileItem syncItem;
    if (_syncPal->getSyncFileItem(relativeLocalPath, syncItem)) {
        Error err(
            _syncPal->syncDbId(), syncItem.localNodeId().has_value() ? syncItem.localNodeId().value() : "",
            syncItem.remoteNodeId().has_value() ? syncItem.remoteNodeId().value() : "", syncItem.type(),
            syncOp->affectedNode()->moveOrigin().has_value() ? syncOp->affectedNode()->moveOrigin().value() : syncItem.path(),
            syncItem.conflict(), syncItem.inconsistency(), cancelType,
            syncOp->affectedNode()->moveOrigin().has_value() ? relativeLocalPath : "");
        _syncPal->addError(err);
    }
    _syncPal->setProgressComplete(relativeLocalPath, status);

    if (removeFromDb) {
        //  Remove the node from DB and tree so it will be re-created at its original location on next sync
        if (!propagateDeleteToDbAndTree(syncOp)) {
            LOGW_SYNCPAL_WARN(_logger, L"Failed to propagate changes in DB or update tree for: "
                                           << SyncName2WStr(syncOp->affectedNode()->name()).c_str());
        }
        _syncPal->_restart = true;
    }
}

void ExecutorWorker::sendProgress() {
    std::chrono::duration<double> elapsed_seconds = std::chrono::steady_clock::now() - _fileProgressTimer;
    if (elapsed_seconds.count() > SEND_PROGRESS_DELAY) {
        _fileProgressTimer = std::chrono::steady_clock::now();

        for (const auto &jobInfo : _ongoingJobs) {
            if (jobInfo.second->isProgressTracked() && jobInfo.second->progressChanged()) {
                _syncPal->setProgress(jobInfo.second->affectedFilePath(), jobInfo.second->getProgress());
            }
        }
    }
}

bool ExecutorWorker::propagateConflictToDbAndTree(SyncOpPtr syncOp, bool &propagateChange) {
    propagateChange = true;

    switch (syncOp->conflict().type()) {
        case ConflictTypeEditEdit:        // Edit conflict pattern
        case ConflictTypeCreateCreate:    // Name clash conflict pattern
        case ConflictTypeMoveCreate:      // Name clash conflict pattern
        case ConflictTypeMoveMoveDest:    // Name clash conflict pattern
        case ConflictTypeMoveMoveSource:  // Name clash conflict pattern
        {
            if (syncOp->conflict().type() != ConflictTypeMoveMoveSource) {
                DbNodeId dbId = -1;
                bool localNodeFoundInDb = false;
                // when it's an Edit-Edit we want to delete the node
                NodeId effectiveNodeId =
                    syncOp->conflict().localNode()->previousId().has_value() ? *syncOp->conflict().localNode()->previousId()
                    : syncOp->conflict().localNode()->id().has_value()       ? *syncOp->conflict().localNode()->id()
                                                                             : std::string();
                _syncPal->_syncDb->dbId(ReplicaSideLocal, effectiveNodeId, dbId, localNodeFoundInDb);
                if (localNodeFoundInDb) {
                    // Remove local node from DB
                    if (!deleteFromDb(syncOp->conflict().localNode())) {
                        propagateChange = false;
                        return true;
                    }
                }
            }

            // Remove node from update tree
            _syncPal->updateTree(ReplicaSideLocal)->deleteNode(syncOp->conflict().localNode());
            _syncPal->updateTree(ReplicaSideRemote)->deleteNode(syncOp->conflict().remoteNode());

            propagateChange = false;
            break;
        }
        case ConflictTypeEditDelete:  // Delete conflict pattern
        {
            if (syncOp->type() == OperationTypeDelete) {
                // Just apply normal behavior for delete operations
                break;
            }

            // Do nothing about the move operation since the nodes will be remove from DB anyway
            propagateChange = false;
            break;
        }
        case ConflictTypeCreateParentDelete:  // Indirect conflict pattern
        {
            // Remove node from update tree
            std::shared_ptr<UpdateTree> updateTree = affectedUpdateTree(syncOp);
            updateTree->deleteNode(syncOp->affectedNode());

            // Do not propagate changes to the DB
            // The created node has been moved and will be discovered as new on next sync
            propagateChange = false;
            break;
        }
        case ConflictTypeMoveDelete:        // Delete conflict pattern
        case ConflictTypeMoveParentDelete:  // Indirect conflict pattern
        case ConflictTypeMoveMoveCycle:     // Name clash conflict pattern
        case ConflictTypeNone:
        default:
            // Just apply normal behavior
            break;
    }
    return false;
}

bool ExecutorWorker::propagateChangeToDbAndTree(SyncOpPtr syncOp, std::shared_ptr<AbstractJob> job, std::shared_ptr<Node> &node) {
    if (syncOp->hasConflict()) {
        bool propagateChange = true;
        bool hasError = propagateConflictToDbAndTree(syncOp, propagateChange);
        if (!propagateChange || hasError) {
            return !hasError;
        }
    }

    switch (syncOp->type()) {
        case OperationTypeCreate:
        case OperationTypeEdit: {
            NodeId nodeId;
            SyncTime modtime = 0;
            if (syncOp->targetSide() == ReplicaSideLocal) {
                auto castJob(std::dynamic_pointer_cast<DownloadJob>(job));
                nodeId = castJob->localNodeId();
                modtime = castJob->modtime();
            } else {
                bool jobOk = false;
                auto uploadJob(std::dynamic_pointer_cast<UploadJob>(job));
                if (uploadJob) {
                    nodeId = uploadJob->nodeId();
                    modtime = uploadJob->modtime();
                    jobOk = true;
                } else {
                    auto uploadSessionJob(std::dynamic_pointer_cast<DriveUploadSession>(job));
                    if (uploadSessionJob) {
                        nodeId = uploadSessionJob->nodeId();
                        modtime = uploadSessionJob->modtime();
                        jobOk = true;
                    }
                }

                if (!jobOk) {
                    LOGW_SYNCPAL_WARN(_logger, L"Failed to cast upload job " << job->jobId());
                    _executorExitCode = ExitCodeSystemError;
                    _executorExitCause = ExitCauseUnknown;
                    return false;
                }
            }

            if (syncOp->type() == OperationTypeCreate) {
                return propagateCreateToDbAndTree(syncOp, nodeId, modtime, node);
            } else {
                return propagateEditToDbAndTree(syncOp, nodeId, modtime, node);
            }
        }
        case OperationTypeMove: {
            return propagateMoveToDbAndTree(syncOp);
        }
        case OperationTypeDelete: {
            return propagateDeleteToDbAndTree(syncOp);
        }
        default: {
            LOGW_SYNCPAL_WARN(_logger, L"Unknown operation type " << Utility::s2ws(Utility::opType2Str(syncOp->type())).c_str()
                                                                  << L" on file "
                                                                  << SyncName2WStr(syncOp->affectedNode()->name()).c_str());
        }
    }

    return false;
}

bool ExecutorWorker::propagateCreateToDbAndTree(SyncOpPtr syncOp, const NodeId &newNodeId, std::optional<SyncTime> newLastModTime,
                                                std::shared_ptr<Node> &node) {
    std::shared_ptr<Node> newCorrespondingParentNode = nullptr;
    if (affectedUpdateTree(syncOp)->rootNode() == syncOp->affectedNode()->parentNode()) {
        newCorrespondingParentNode = targetUpdateTree(syncOp)->rootNode();
    } else {
        newCorrespondingParentNode = correspondingNodeInOtherTree(syncOp->affectedNode()->parentNode());
    }

    if (!newCorrespondingParentNode) {
        LOG_SYNCPAL_WARN(_logger, "Corresponding parent node not found");
        _executorExitCode = ExitCodeDataError;
        _executorExitCause = ExitCauseUnknown;
        return false;
    }

    // 2. Insert a new entry into the database, to avoid that the object is detected again by compute_ops() on the next sync
    // iteration.
    std::string localId = syncOp->targetSide() == ReplicaSideLocal
                              ? newNodeId
                              : (syncOp->affectedNode()->id().has_value() ? *syncOp->affectedNode()->id() : "");
    std::string remoteId = syncOp->targetSide() == ReplicaSideLocal
                               ? (syncOp->affectedNode()->id().has_value() ? *syncOp->affectedNode()->id() : "")
                               : newNodeId;
    SyncName localName = syncOp->targetSide() == ReplicaSideLocal ? syncOp->newName() : syncOp->affectedNode()->name();
    SyncName remoteName = localName;

    if (localId.empty() || remoteId.empty()) {
        LOGW_SYNCPAL_WARN(_logger, L"Empty " << (localId.empty() ? L"local" : L"remote") << L" id for item "
                                             << SyncName2WStr(syncOp->affectedNode()->name()).c_str());
        _executorExitCode = ExitCodeDataError;
        _executorExitCause = ExitCauseUnknown;
        return false;
    }

    DbNode dbNode(0, newCorrespondingParentNode->idb(), localName, remoteName, localId, remoteId,
                  syncOp->affectedNode()->createdAt(), newLastModTime, newLastModTime, syncOp->affectedNode()->type(),
                  syncOp->affectedNode()->size(), ""  // TODO : change it once we start using content checksum
                  ,
                  syncOp->omit() ? SyncFileStatusSuccess : SyncFileStatusUnknown);

    if (ParametersCache::isExtendedLogEnabled()) {
        LOGW_SYNCPAL_DEBUG(_logger,
                           L"Inserting in DB: "
                               << L" localName=" << SyncName2WStr(localName).c_str() << L" / remoteName="
                               << SyncName2WStr(remoteName).c_str() << L" / localId=" << Utility::s2ws(localId).c_str()
                               << L" / remoteId=" << Utility::s2ws(remoteId).c_str() << L" / parent DB ID="
                               << (newCorrespondingParentNode->idb().has_value() ? newCorrespondingParentNode->idb().value() : -1)
                               << L" / createdAt="
                               << (syncOp->affectedNode()->createdAt().has_value() ? *syncOp->affectedNode()->createdAt() : -1)
                               << L" / lastModTime=" << (newLastModTime.has_value() ? *newLastModTime : -1) << L" / type="
                               << syncOp->affectedNode()->type());
    }

    if (dbNode.nameLocal().empty() || dbNode.nameRemote().empty() || !dbNode.nodeIdLocal().has_value() ||
        !dbNode.nodeIdRemote().has_value() || !dbNode.created().has_value() || !dbNode.lastModifiedLocal().has_value() ||
        !dbNode.lastModifiedRemote().has_value() || dbNode.type() == NodeTypeUnknown) {
        LOG_SYNCPAL_ERROR(_logger, "Error inserting new node in DB!!!");
        assert(false);
    }

    DbNodeId newDbNodeId;
    bool constraintError = false;
    if (!_syncPal->_syncDb->insertNode(dbNode, newDbNodeId, constraintError)) {
        LOGW_SYNCPAL_WARN(
            _logger, L"Failed to insert node into DB:"
                         << L" local ID: " << Utility::s2ws(localId).c_str() << L", remote ID: "
                         << Utility::s2ws(remoteId).c_str() << L", local name: " << SyncName2WStr(localName).c_str()
                         << L", remote name: " << SyncName2WStr(remoteName).c_str() << L", parent DB ID: "
                         << (newCorrespondingParentNode->idb().has_value() ? newCorrespondingParentNode->idb().value() : -1));

        if (!constraintError) {
            _executorExitCode = ExitCodeDbError;
            _executorExitCause = ExitCauseDbAccessError;
            return false;
        }

        // Manage DELETE events not reported by the folder watcher
        // Some apps save files with DELETE + CREATE operations, but sometimes, the DELETE operation is not reported
        // => The local snapshot will contain 2 nodes with the same remote id
        // => A unique constraint error on the remote node id will occur when inserting the new node in DB
        DbNodeId dbNodeId;
        bool found = false;
        if (!_syncPal->_syncDb->dbId(ReplicaSideRemote, remoteId, dbNodeId, found)) {
            LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbId");
            _executorExitCode = ExitCodeDbError;
            _executorExitCause = ExitCauseDbAccessError;
            return false;
        }
        if (found) {
            // Delete old node
            if (!_syncPal->_syncDb->deleteNode(dbNodeId, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::deleteNode");
                _executorExitCode = ExitCodeDbError;
                _executorExitCause = ExitCauseDbAccessError;
                return false;
            }
        }

        // Create new node
        if (!_syncPal->_syncDb->insertNode(dbNode, newDbNodeId, constraintError)) {
            LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::insertNode");
            _executorExitCode = constraintError ? ExitCodeDataError : ExitCodeDbError;
            _executorExitCause = ExitCauseDbAccessError;
            return false;
        }

        // The snapshot must be invalidated before the next sync
        _snapshotToInvalidate = true;
    }

    // 3. Update the update tree structures to ensure that follow-up operations can execute correctly, as they are based on
    // the information in these structures.
    if (syncOp->omit()) {
        // Update existing nodes
        syncOp->affectedNode()->setIdb(newDbNodeId);
        syncOp->correspondingNode()->setIdb(newDbNodeId);
        node = syncOp->correspondingNode();
    } else {
        // insert new node
        node = std::shared_ptr<Node>(
            new Node(newDbNodeId, syncOp->targetSide() == ReplicaSideLocal ? ReplicaSideLocal : ReplicaSideRemote, remoteName,
                     syncOp->affectedNode()->type(), OperationTypeNone, newNodeId, newLastModTime, newLastModTime,
                     syncOp->affectedNode()->size(), newCorrespondingParentNode));
        if (node == nullptr) {
            _executorExitCode = ExitCodeSystemError;
            _executorExitCause = ExitCauseNotEnoughtMemory;
            std::cout << "Failed to allocate memory" << std::endl;
            LOG_SYNCPAL_ERROR(_logger, "Failed to allocate memory");
            return false;
        }

        std::shared_ptr<UpdateTree> updateTree = targetUpdateTree(syncOp);
        updateTree->insertNode(node);

        newCorrespondingParentNode->insertChildren(node);

        // Affected node does not have a valid DB ID yet, update it
        syncOp->affectedNode()->setIdb(newDbNodeId);
    }

    return true;
}

bool ExecutorWorker::propagateEditToDbAndTree(SyncOpPtr syncOp, const NodeId &newNodeId, std::optional<SyncTime> newLastModTime,
                                              std::shared_ptr<Node> &node) {
    DbNode dbNode;
    bool found = false;
    if (!_syncPal->_syncDb->node(*syncOp->correspondingNode()->idb(), dbNode, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::node");
        _executorExitCode = ExitCodeDbError;
        _executorExitCause = ExitCauseDbAccessError;
        return false;
    }
    if (!found) {
        LOG_SYNCPAL_DEBUG(_logger, "Failed to retrieve node for dbId=" << *syncOp->correspondingNode()->idb());
        _executorExitCode = ExitCodeDataError;
        _executorExitCause = ExitCauseDbEntryNotFound;
        return false;
    }

    // 2. Update the database entry, to avoid detecting the edit operation again.
    std::string localId = syncOp->targetSide() == ReplicaSideLocal   ? newNodeId
                          : syncOp->affectedNode()->id().has_value() ? *syncOp->affectedNode()->id()
                                                                     : std::string();
    std::string remoteId = syncOp->targetSide() == ReplicaSideLocal
                               ? syncOp->affectedNode()->id().has_value() ? *syncOp->affectedNode()->id() : std::string()
                               : newNodeId;
    SyncName localName = syncOp->affectedNode()->name();
    SyncName remoteName = syncOp->affectedNode()->name();

    if (localId.empty() || remoteId.empty()) {
        LOGW_SYNCPAL_WARN(_logger, L"Empty " << (localId.empty() ? L"local" : L"remote") << L" id for item "
                                             << SyncName2WStr(syncOp->affectedNode()->name()).c_str());
        _executorExitCode = ExitCodeDataError;
        _executorExitCause = ExitCauseUnknown;
        return false;
    }

    dbNode.setNodeIdLocal(localId);
    dbNode.setNodeIdRemote(remoteId);
    dbNode.setLastModifiedLocal(newLastModTime);
    dbNode.setLastModifiedRemote(newLastModTime);
    dbNode.setSize(syncOp->affectedNode()->size());
    dbNode.setChecksum("");  // TODO : change it once we start using content checksum
    if (syncOp->omit()) {
        dbNode.setStatus(SyncFileStatusSuccess);
    }

    if (ParametersCache::isExtendedLogEnabled()) {
        LOGW_SYNCPAL_DEBUG(
            _logger,
            L"Updating DB: " << L" / localName=" << SyncName2WStr(localName).c_str() << L" / remoteName="
                             << SyncName2WStr(remoteName).c_str() << L" / localId=" << Utility::s2ws(localId).c_str()
                             << L" / remoteId=" << Utility::s2ws(remoteId).c_str() << L" / parent DB ID="
                             << (dbNode.parentNodeId().has_value() ? dbNode.parentNodeId().value() : -1) << L" / createdAt="
                             << (syncOp->affectedNode()->createdAt().has_value() ? *syncOp->affectedNode()->createdAt() : -1)
                             << L" / lastModTime=" << (newLastModTime.has_value() ? *newLastModTime : -1) << L" / type="
                             << syncOp->affectedNode()->type());
    }

    if (!_syncPal->_syncDb->updateNode(dbNode, found)) {
        LOGW_SYNCPAL_WARN(_logger, L"Failed to update node into DB: "
                                       << L"local ID: " << Utility::s2ws(localId).c_str() << L", remote ID: "
                                       << Utility::s2ws(remoteId).c_str() << L", local name: " << SyncName2WStr(localName).c_str()
                                       << L", remote name: " << SyncName2WStr(remoteName).c_str() << L", parent DB ID: "
                                       << (dbNode.parentNodeId().has_value() ? dbNode.parentNodeId().value() : -1));
        _executorExitCode = ExitCodeDbError;
        _executorExitCause = ExitCauseDbAccessError;
        return false;
    }
    if (!found) {
        _executorExitCode = ExitCodeDataError;
        _executorExitCause = ExitCauseDbEntryNotFound;
        return false;
    }

    // 3. If the omit flag is False, update the updatetreeY structure to ensure that follow-up operations can execute
    // correctly, as they are based on the information in this structure
    if (!syncOp->omit()) {
        syncOp->correspondingNode()->setId(syncOp->targetSide() == ReplicaSideLocal
                                               ? localId
                                               : remoteId);  // ID might have changed in the case of a delete+create
        syncOp->correspondingNode()->setLastModified(newLastModTime);
    }
    node = syncOp->correspondingNode();

    return true;
}

bool ExecutorWorker::propagateMoveToDbAndTree(SyncOpPtr syncOp) {
    std::shared_ptr<Node> correspondingNode =
        syncOp->correspondingNode() ? syncOp->correspondingNode() : syncOp->affectedNode();  // No corresponding node => rename

    if (!correspondingNode || !correspondingNode->idb().has_value()) {
        LOG_SYNCPAL_WARN(_logger, "Invalid corresponding node");
        _executorExitCode = ExitCodeDataError;
        return false;
    }

    DbNode dbNode;
    bool found = false;
    if (!_syncPal->_syncDb->node(*correspondingNode->idb(), dbNode, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::node");
        _executorExitCode = ExitCodeDbError;
        _executorExitCause = ExitCauseDbAccessError;
        return false;
    }
    if (!found) {
        LOG_SYNCPAL_DEBUG(_logger, "Failed to retrieve node for dbId=" << *correspondingNode->idb());
        _executorExitCode = ExitCodeDataError;
        _executorExitCause = ExitCauseDbEntryNotFound;
        return false;
    }

    // 2. Update the database entry, to avoid detecting the move operation again.
    std::shared_ptr<Node> parentNode =
        syncOp->newParentNode()
            ? syncOp->newParentNode()
            : (syncOp->correspondingNode() ? correspondingNodeInOtherTree(syncOp->affectedNode()->parentNode())
                                           : correspondingNode->parentNode());
    if (!parentNode) {
        LOGW_SYNCPAL_DEBUG(_logger,
                           L"Failed to get corresponding parent node: " << SyncName2WStr(syncOp->affectedNode()->name()).c_str());
        _executorExitCode = ExitCodeDataError;
        _executorExitCause = ExitCauseUnknown;
        return false;
    }

    std::string localId = syncOp->targetSide() == ReplicaSideLocal
                              ? correspondingNode->id().has_value() ? *correspondingNode->id() : std::string()
                          : syncOp->affectedNode()->id().has_value() ? *syncOp->affectedNode()->id()
                                                                     : std::string();
    std::string remoteId = syncOp->targetSide() == ReplicaSideLocal
                               ? syncOp->affectedNode()->id().has_value() ? *syncOp->affectedNode()->id() : std::string()
                           : correspondingNode->id().has_value() ? *correspondingNode->id()
                                                                 : std::string();
    SyncName localName = syncOp->newName();
    SyncName remoteName = localName;

    if (localId.empty() || remoteId.empty()) {
        LOGW_SYNCPAL_WARN(_logger, L"Empty " << (localId.empty() ? L"local" : L"remote") << L" id for item "
                                             << SyncName2WStr(syncOp->affectedNode()->name()).c_str());
        _executorExitCode = ExitCodeDataError;
        _executorExitCause = ExitCauseUnknown;
        return false;
    }

    dbNode.setParentNodeId(parentNode->idb());
    dbNode.setNameLocal(localName);
    dbNode.setNameRemote(remoteName);
    if (syncOp->omit()) {
        dbNode.setStatus(SyncFileStatusSuccess);
    }

    if (ParametersCache::isExtendedLogEnabled()) {
        LOGW_SYNCPAL_DEBUG(
            _logger,
            L"Updating DB: " << L" localName=" << SyncName2WStr(localName).c_str() << L" / remoteName="
                             << SyncName2WStr(remoteName).c_str() << L" / localId=" << Utility::s2ws(localId).c_str()
                             << L" / remoteId=" << Utility::s2ws(remoteId).c_str() << L" / parent DB ID="
                             << (dbNode.parentNodeId().has_value() ? dbNode.parentNodeId().value() : -1) << L" / createdAt="
                             << (syncOp->affectedNode()->createdAt().has_value() ? *syncOp->affectedNode()->createdAt() : -1)
                             << L" / lastModTime="
                             << (syncOp->affectedNode()->lastmodified().has_value() ? *syncOp->affectedNode()->lastmodified()
                                                                                    : -1)
                             << L" / type=" << syncOp->affectedNode()->type());
    }

    if (!_syncPal->_syncDb->updateNode(dbNode, found)) {
        LOGW_SYNCPAL_WARN(_logger, L"Failed to update node into DB: "
                                       << L"local ID: " << Utility::s2ws(localId).c_str() << L", remote ID: "
                                       << Utility::s2ws(remoteId).c_str() << L", local name: " << SyncName2WStr(localName).c_str()
                                       << L", remote name: " << SyncName2WStr(remoteName).c_str() << L", parent DB ID: "
                                       << (dbNode.parentNodeId().has_value() ? dbNode.parentNodeId().value() : -1));
        _executorExitCode = ExitCodeDbError;
        _executorExitCause = ExitCauseDbAccessError;
        return false;
    }
    if (!found) {
        _executorExitCode = ExitCodeDataError;
        _executorExitCause = ExitCauseDbEntryNotFound;
        return false;
    }

    // 3. If the omit flag is False, update the updatetreeY structure to ensure that follow-up operations can execute
    // correctly, as they are based on the information in this structure.
    if (!syncOp->omit()) {
        auto prevParent = correspondingNode->parentNode();
        prevParent->deleteChildren(correspondingNode);

        correspondingNode->setName(remoteName);
        correspondingNode->setParentNode(parentNode);
        correspondingNode->parentNode()->insertChildren(correspondingNode);
    }

    return true;
}

bool ExecutorWorker::propagateDeleteToDbAndTree(SyncOpPtr syncOp) {
    // 2. Remove the entry from the database. If nX is a directory node, also remove all entries for each node n ∈ S. This
    // avoids that the object(s) are detected again by compute_ops() on the next sync iteration
    if (!deleteFromDb(syncOp->affectedNode())) {
        return false;
    }

    // 3. Remove nX and nY from the update tree structures.
    affectedUpdateTree(syncOp)->deleteNode(syncOp->affectedNode());
    targetUpdateTree(syncOp)->deleteNode(syncOp->correspondingNode());

    return true;
}

bool ExecutorWorker::deleteFromDb(std::shared_ptr<Node> node) {
    if (!node->idb().has_value()) {
        LOGW_SYNCPAL_WARN(_logger, L"Node " << SyncName2WStr(node->name()).c_str() << L" does not have a DB ID");

        _executorExitCode = ExitCodeDataError;
        _executorExitCause = ExitCauseDbEntryNotFound;
        return false;
    }

    // Remove item (and children by cascade) from DB
    bool found = false;
    if (!_syncPal->_syncDb->deleteNode(*node->idb(), found)) {
        LOG_SYNCPAL_WARN(_logger, "Failed to remove node " << *node->idb() << " from DB");
        _executorExitCode = ExitCodeDbError;
        _executorExitCause = ExitCauseDbAccessError;
        return false;
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "Node DB ID " << *node->idb() << " not found");
        _executorExitCode = ExitCodeDataError;
        _executorExitCause = ExitCauseDbEntryNotFound;
        return false;
    }

    if (ParametersCache::isExtendedLogEnabled()) {
        LOGW_SYNCPAL_DEBUG(_logger, L"Item \"" << SyncName2WStr(node->name()).c_str() << L"\" removed from DB");
    }

    return true;
}

bool ExecutorWorker::runCreateDirJob(SyncOpPtr syncOp, std::shared_ptr<AbstractJob> job) {
    job->runSynchronously();

    std::string errorCode;
    auto tokenJob(std::dynamic_pointer_cast<AbstractTokenNetworkJob>(job));
    if (tokenJob && tokenJob->hasErrorApi(&errorCode)) {
        const auto code = getNetworkErrorCode(errorCode);
        if (code == NetworkErrorCode::destinationAlreadyExists) {
            // Folder is already there, ignore this error
        } else if (code == NetworkErrorCode::forbiddenError) {
            // The item should be blacklisted
            _executorExitCode = ExitCodeOk;
            _syncPal->blacklistTemporarily(
                syncOp->affectedNode()->id().has_value() ? syncOp->affectedNode()->id().value() : std::string(),
                syncOp->affectedNode()->getPath(), ReplicaSideLocal);
            Error error(_syncPal->_syncDbId,
                        syncOp->affectedNode()->id().has_value() ? syncOp->affectedNode()->id().value() : std::string(), "",
                        syncOp->affectedNode()->type(), syncOp->affectedNode()->getPath(), ConflictTypeNone,
                        InconsistencyTypeNone, CancelTypeNone, "", ExitCodeBackError, ExitCauseHttpErrForbidden);
            _syncPal->addError(error);

            affectedUpdateTree(syncOp)->deleteNode(syncOp->affectedNode());
            if (syncOp->correspondingNode()) {
                targetUpdateTree(syncOp)->deleteNode(syncOp->correspondingNode());
            }
            return true;
        }
    }

    if (job->exitCode() == ExitCodeNeedRestart) {
        // Special case: not an error but sync needs to be restarted
        _executorExitCode = ExitCodeOk;
        _syncPal->_restart = true;
        return false;
    } else if ((syncOp->targetSide() == ReplicaSideLocal && job->exitCode() == ExitCodeDataError &&
                job->exitCause() == ExitCauseFileAlreadyExist) ||
               (syncOp->targetSide() == ReplicaSideRemote && job->exitCode() == ExitCodeBackError &&
                job->exitCause() == ExitCauseFileAlreadyExist)) {
        auto localCreateDirJob(std::dynamic_pointer_cast<LocalCreateDirJob>(job));
        if (localCreateDirJob) {
            LOGW_SYNCPAL_WARN(_logger, L"Item: " << Utility::formatSyncPath(localCreateDirJob->destFilePath()).c_str()
                                                 << L" already exist. Blacklisting it on local replica.");
            PlatformInconsistencyCheckerUtility::renameLocalFile(_syncPal->localPath() / localCreateDirJob->destFilePath(),
                                                                 PlatformInconsistencyCheckerUtility::SuffixTypeBlacklisted);
        }
        return false;
    } else if (job->exitCode() != ExitCodeOk) {
        LOGW_SYNCPAL_WARN(_logger, L"Failed to create directory: " << SyncName2WStr(syncOp->affectedNode()->name()).c_str());
        _executorExitCode = job->exitCode();
        _executorExitCause = job->exitCause();
        return false;
    }

    NodeId newNodeId;
    SyncTime newModTime = 0;

    if (syncOp->targetSide() == ReplicaSideLocal) {
        auto castJob(std::dynamic_pointer_cast<LocalCreateDirJob>(job));
        newNodeId = castJob->nodeId();
        newModTime = castJob->modtime();
    } else {
        auto castJob(std::dynamic_pointer_cast<CreateDirJob>(job));
        newNodeId = castJob->nodeId();
        newModTime = castJob->modtime();
    }

    if (newNodeId.empty()) {
        LOGW_SYNCPAL_WARN(_logger,
                          L"Failed to retreive ID for directory: " << SyncName2WStr(syncOp->affectedNode()->name()).c_str());
        _executorExitCode = ExitCodeDataError;
        _executorExitCause = ExitCauseApiErr;
        return false;
    }

    std::shared_ptr<Node> newNode = nullptr;
    if (!propagateCreateToDbAndTree(syncOp, newNodeId, newModTime, newNode)) {
        LOGW_SYNCPAL_WARN(_logger, L"Failed to propagate changes in DB or update tree for: "
                                       << SyncName2WStr(syncOp->affectedNode()->name()).c_str());
        return false;
    }

    return true;
}

void ExecutorWorker::cancelAllOngoingJobs(bool reschedule /*= false*/) {
    LOG_SYNCPAL_DEBUG(_logger, "Cancelling all queued executor jobs");

    // First, abort all jobs that are not running yet to avoid starting them for nothing
    std::list<std::shared_ptr<AbstractJob>> remainingJobs;
    for (const auto &job : _ongoingJobs) {
        if (!job.second->isRunning()) {
            LOG_SYNCPAL_DEBUG(_logger, "Cancelling job: " << job.second->jobId());
            job.second->abort();

            if (reschedule) {
                _opList.push_front(_jobToSyncOpMap[job.first]->id());
            }
        } else {
            remainingJobs.push_back(job.second);
        }
    }

    // Then cancel jobs that are currently running
    for (const auto &job : remainingJobs) {
        LOG_SYNCPAL_DEBUG(_logger, "Cancelling job: " << job->jobId());
        job->abort();

        if (reschedule) {
            _opList.push_front(_jobToSyncOpMap[job->jobId()]->id());
        }
    }
    _ongoingJobs.clear();
    if (!reschedule) {
        _opList.clear();
    }

    LOG_SYNCPAL_DEBUG(_logger, "All queued executor jobs cancelled");
}

void ExecutorWorker::manageJobDependencies(SyncOpPtr syncOp, std::shared_ptr<AbstractJob> job) {
    _syncOpToJobMap.insert({syncOp->id(), job->jobId()});
    // Check for job dependencies on other job
    if (syncOp->hasParentOp()) {
        auto parentOpId = syncOp->parentId();
        if (_syncOpToJobMap.find(parentOpId) != _syncOpToJobMap.end()) {
            // The job ID associated with the parent sync operation
            auto parentJobId = _syncOpToJobMap.find(parentOpId)->second;
            job->setParentJobId(parentJobId);
        }
    }
}

void ExecutorWorker::increaseErrorCount(SyncOpPtr syncOp) {
    if (syncOp->affectedNode() && syncOp->affectedNode()->id().has_value()) {
        _syncPal->increaseErrorCount(*syncOp->affectedNode()->id(), syncOp->affectedNode()->type(),
                                     syncOp->affectedNode()->getPath(), otherSide(syncOp->targetSide()));

        affectedUpdateTree(syncOp)->deleteNode(syncOp->affectedNode());
        if (syncOp->correspondingNode() && syncOp->correspondingNode()->id().has_value()) {
            targetUpdateTree(syncOp)->deleteNode(syncOp->correspondingNode());
        }
    }
}

bool ExecutorWorker::getFileSize(const SyncPath &path, uint64_t &size) {
    IoError ioError = IoErrorUnknown;
    if (!IoHelper::getFileSize(path, size, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::getFileSize for " << Utility::formatIoError(path, ioError).c_str());
        _executorExitCode = ExitCodeSystemError;
        _executorExitCause = ExitCauseUnknown;
        return false;
    }

    if (ioError == IoErrorNoSuchFileOrDirectory) {  // The synchronization will be re-started.
        LOGW_WARN(_logger, L"File doesn't exist: " << Utility::formatSyncPath(path).c_str());
        _executorExitCode = ExitCodeDataError;
        _executorExitCause = ExitCauseUnknown;
        return false;
    }

    if (ioError == IoErrorAccessDenied) {  // An action from the user is requested.
        LOGW_WARN(_logger, L"File search permission missing: " << Utility::formatSyncPath(path).c_str());
        _executorExitCode = ExitCodeSystemError;
        _executorExitCause = ExitCauseNoSearchPermission;
        return false;
    }

    assert(ioError == IoErrorSuccess);
    if (ioError != IoErrorSuccess) {
        LOGW_WARN(_logger, L"Unable to read file size for " << Utility::formatSyncPath(path).c_str());
        _executorExitCode = ExitCodeSystemError;
        _executorExitCause = ExitCauseUnknown;
        return false;
    }

    return true;
}

}  // namespace KDC
