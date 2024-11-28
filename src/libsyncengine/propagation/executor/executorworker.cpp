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
#include "jobs/network/API_v2/createdirjob.h"
#include "jobs/network/API_v2/deletejob.h"
#include "jobs/network/API_v2/downloadjob.h"
#include "jobs/network/API_v2/movejob.h"
#include "jobs/network/API_v2/renamejob.h"
#include "jobs/network/API_v2/uploadjob.h"
#include "jobs/network/API_v2/getfilelistjob.h"
#include "jobs/network/API_v2/upload_session/driveuploadsession.h"
#include "reconciliation/platform_inconsistency_checker/platforminconsistencycheckerutility.h"
#include "update_detection/file_system_observer/filesystemobserverworker.h"
#include "update_detection/update_detector/updatetree.h"
#include "jobs/jobmanager.h"
#include "libcommon/log/sentry/scopedptrace.h"
#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"
#include "requests/parameterscache.h"
#include "requests/syncnodecache.h"
#include "utility/jsonparserutility.h"

#include <iostream>
#include <log4cplus/loggingmacros.h>

namespace KDC {

#define SEND_PROGRESS_DELAY 1 // 1 sec
#define SNAPSHOT_INVALIDATION_THRESHOLD 100 // Changes

ExecutorWorker::ExecutorWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName) :
    OperationProcessor(syncPal, name, shortName) {}

void ExecutorWorker::executorCallback(UniqueId jobId) {
    _terminatedJobs.push(jobId);
}

void ExecutorWorker::execute() {
    ExitInfo executorExitInfo = ExitCode::Ok;
    _snapshotToInvalidate = false;

    _jobToSyncOpMap.clear();
    _syncOpToJobMap.clear();

    LOG_SYNCPAL_DEBUG(_logger, "Worker started: name=" << name().c_str());

    LOG_SYNCPAL_DEBUG(_logger, "JobManager::instance()->countManagedJobs(): " << JobManager::instance()->countManagedJobs());
    LOG_SYNCPAL_DEBUG(_logger, "JobManager::instance()->maxNbThreads() * 2: " << JobManager::instance()->maxNbThreads() * 2);

    // Keep a copy of the sorted list
    _opList = _syncPal->_syncOps->opSortedList();
    auto perfMonitor = Sentry::ScopedPTrace(Sentry::PTraceName::InitProgress, syncDbId());
    initProgressManager();
    uint64_t changesCounter = 0;
    while (!_opList.empty()) { // Same loop twice because we might reschedule the jobs after a pause TODO : refactor double loop
        // Create all the jobs
        perfMonitor.stopAndStart(Sentry::PTraceName::JobGeneration, syncDbId());
        while (!_opList.empty()) {
            if (ExitInfo exitInfo = deleteFinishedAsyncJobs(); !exitInfo) {
                executorExitInfo = exitInfo;
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

            if (JobManager::instance()->countManagedJobs() > static_cast<size_t>(JobManager::instance()->maxNbThreads()) * 2) {
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
            bool ignored = false;
            bool bypassProgressComplete = false;
            switch (syncOp->type()) {
                case OperationType::Create: {
                    executorExitInfo = handleCreateOp(syncOp, job, ignored);
                    break;
                }
                case OperationType::Edit: {
                    executorExitInfo = handleEditOp(syncOp, job, ignored);
                    break;
                }
                case OperationType::Move: {
                    executorExitInfo = handleMoveOp(syncOp, ignored, bypassProgressComplete);
                    break;
                }
                case OperationType::Delete: {
                    executorExitInfo = handleDeleteOp(syncOp, ignored, bypassProgressComplete);
                    break;
                }
                default: {
                    LOGW_SYNCPAL_WARN(_logger, L"Unknown operation type: " << syncOp->type() << L" on file "
                                                                           << SyncName2WStr(syncOp->affectedNode()->name()));
                    executorExitInfo = ExitCode::DataError;
                }
            }

            // If an operation fails but is correctly handled by handleExecutorError, execution can proceed.
            if (executorExitInfo.cause() == ExitCause::OperationCanceled) {
                if (!bypassProgressComplete) setProgressComplete(syncOp, SyncFileStatus::Error);
                continue;
            }

            if (!executorExitInfo) {
                executorExitInfo = handleExecutorError(syncOp, executorExitInfo);
                if (!executorExitInfo) { // If the error is not handled, stop the execution
                    increaseErrorCount(syncOp);
                    cancelAllOngoingJobs();
                    break;
                } else { // If the error is handled, continue the execution
                    if (!bypassProgressComplete) setProgressComplete(syncOp, SyncFileStatus::Error);
                    continue;
                }
                if (!bypassProgressComplete) setProgressComplete(syncOp, SyncFileStatus::Error);
            }

            if (job) {
                manageJobDependencies(syncOp, job);
                std::function<void(UniqueId)> callback =
                        std::bind(&ExecutorWorker::executorCallback, this, std::placeholders::_1);
                JobManager::instance()->queueAsyncJob(job, Poco::Thread::PRIO_NORMAL, callback);
                _ongoingJobs.insert({job->jobId(), job});
                _jobToSyncOpMap.insert({job->jobId(), syncOp});
            } else {
                if (!bypassProgressComplete) {
                    if (ignored) {
                        setProgressComplete(syncOp, SyncFileStatus::Ignored);
                    } else if (syncOp->affectedNode() && syncOp->affectedNode()->inconsistencyType() != InconsistencyType::None) {
                        setProgressComplete(syncOp, SyncFileStatus::Inconsistency);
                    } else {
                        setProgressComplete(syncOp, SyncFileStatus::Success);
                    }
                }

                if (syncOp->affectedNode()->id().has_value()) {
                    std::unordered_set<NodeId> whiteList;
                    SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), SyncNodeType::WhiteList, whiteList);
                    if (whiteList.find(syncOp->affectedNode()->id().value()) != whiteList.end()) {
                        // This item has been synchronized, it can now be removed from white list
                        whiteList.erase(syncOp->affectedNode()->id().value());
                        SyncNodeCache::instance()->update(_syncPal->syncDbId(), SyncNodeType::WhiteList, whiteList);
                    }
                }
            }
        }
        perfMonitor.stopAndStart(Sentry::PTraceName::waitForAllJobsToFinish, syncDbId());
        if (ExitInfo exitInfo = waitForAllJobsToFinish(); !exitInfo) {
            executorExitInfo = exitInfo;
            break;
        }
        perfMonitor.stop();
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

    setExitCause(executorExitInfo.cause());
    setDone(executorExitInfo.code());
    LOG_SYNCPAL_DEBUG(_logger, "Worker stopped: name=" << name().c_str());
}

void ExecutorWorker::initProgressManager() {
    for (const auto syncOpId: _opList) {
        SyncFileItem syncItem;
        SyncOpPtr syncOp = _syncPal->_syncOps->getOp(syncOpId);
        if (!syncOp) {
            LOG_SYNCPAL_WARN(_logger, "Operation doesn't exist anymore: id=" << syncOpId);
            continue;
        }

        if (syncOp->omit()) {
            continue; // Do not notify UI of progress in case of pseudo conflicts
        }

        initSyncFileItem(syncOp, syncItem);
        if (!_syncPal->initProgress(syncItem)) {
            LOG_SYNCPAL_WARN(_logger, "Error in SyncPal::initProgress: id=" << syncOpId);
        }
    }
}

void ExecutorWorker::initSyncFileItem(SyncOpPtr syncOp, SyncFileItem &syncItem) {
    syncItem.setType(syncOp->affectedNode()->type());
    syncItem.setConflict(syncOp->conflict().type());
    syncItem.setInconsistency(syncOp->affectedNode()->inconsistencyType());
    syncItem.setSize(syncOp->affectedNode()->size());
    syncItem.setModTime(syncOp->affectedNode()->lastmodified().has_value() ? syncOp->affectedNode()->lastmodified().value() : 0);
    syncItem.setCreationTime(syncOp->affectedNode()->createdAt().has_value() ? syncOp->affectedNode()->createdAt().value() : 0);

    if (bitWiseEnumToBool(syncOp->type() & OperationType::Move)) {
        syncItem.setInstruction(SyncFileInstruction::Move);
        syncItem.setPath(syncOp->affectedNode()->moveOrigin().has_value() ? *syncOp->affectedNode()->moveOrigin() : SyncPath());
        syncItem.setNewPath(syncOp->affectedNode()->getPath());
    } else {
        syncItem.setPath(syncOp->affectedNode()->getPath());

        if (bitWiseEnumToBool(syncOp->type() & OperationType::Edit)) {
            syncItem.setInstruction(SyncFileInstruction::Update);
        } else if (bitWiseEnumToBool(syncOp->type() & OperationType::Delete)) {
            syncItem.setInstruction(SyncFileInstruction::Remove);
        }
    }

    if (syncOp->targetSide() == ReplicaSide::Local) {
        syncItem.setLocalNodeId(syncOp->correspondingNode() ? syncOp->correspondingNode()->id() : std::nullopt);
        syncItem.setRemoteNodeId(syncOp->affectedNode()->id());
        syncItem.setDirection(SyncDirection::Down);
        if (bitWiseEnumToBool(syncOp->type() & OperationType::Create)) {
            syncItem.setInstruction(SyncFileInstruction::Get);
        }
    } else {
        syncItem.setLocalNodeId(syncOp->affectedNode()->id());
        syncItem.setRemoteNodeId(syncOp->correspondingNode() ? syncOp->correspondingNode()->id() : std::nullopt);
        syncItem.setDirection(SyncDirection::Up);
        if (bitWiseEnumToBool(syncOp->type() & OperationType::Create)) {
            syncItem.setInstruction(SyncFileInstruction::Put);
        }
    }
}

void ExecutorWorker::logCorrespondingNodeErrorMsg(const SyncOpPtr syncOp) {
    const std::wstring mainMsg = L"Error in UpdateTree::deleteNode: ";
    if (syncOp->correspondingNode()) {
        const auto nodeName = SyncName2WStr(syncOp->correspondingNode()->name());
        LOGW_SYNCPAL_WARN(_logger, mainMsg << L"correspondingNode name=" << L"'" << nodeName << L"'.");
    } else {
        const auto nodeName = SyncName2WStr(syncOp->affectedNode()->name());
        LOGW_SYNCPAL_WARN(_logger,
                          mainMsg << L"correspondingNode is nullptr, former affectedNode name=" << L"'" << nodeName << L"'.");
    }
}

void ExecutorWorker::setProgressComplete(const SyncOpPtr syncOp, SyncFileStatus status) {
    SyncPath relativeLocalFilePath;
    if (syncOp->type() == OperationType::Create || syncOp->type() == OperationType::Edit) {
        relativeLocalFilePath = syncOp->nodePath(ReplicaSide::Local);
    } else {
        relativeLocalFilePath = syncOp->affectedNode()->getPath();
    }

    if (!_syncPal->setProgressComplete(relativeLocalFilePath, status)) {
        LOGW_SYNCPAL_WARN(_logger, L"Error in SyncPal::setProgressComplete: " << Utility::formatSyncPath(relativeLocalFilePath));
    }
}

ExitInfo ExecutorWorker::handleCreateOp(SyncOpPtr syncOp, std::shared_ptr<AbstractJob> &job, bool &ignored) {
    // The execution of the create operation consists of three steps:
    // 1. If omit-flag is False, propagate the file or directory to target replica, because the object is missing there.
    // 2. Insert a new entry into the database, to avoid that the object is detected again by compute_ops() on the next sync
    // iteration.
    // 3. Update the update tree structures to ensure that follow-up operations can execute correctly, as they are based on
    // the information in these structures.
    ignored = false;

    SyncPath relativeLocalFilePath = syncOp->nodePath(ReplicaSide::Local);
    assert(!relativeLocalFilePath.empty());
    SyncPath absoluteLocalFilePath = _syncPal->localPath() / relativeLocalFilePath;
    if (isLiteSyncActivated() && !syncOp->omit()) {
        bool isDehydratedPlaceholder = false;
        if (ExitInfo exitInfo = checkLiteSyncInfoForCreate(syncOp, absoluteLocalFilePath, isDehydratedPlaceholder); !exitInfo) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in checkLiteSyncInfoForCreate");
            return exitInfo;
        }

        if (isDehydratedPlaceholder) {
            // Blacklist dehydrated placeholder
            PlatformInconsistencyCheckerUtility::renameLocalFile(absoluteLocalFilePath,
                                                                 PlatformInconsistencyCheckerUtility::SuffixType::Blacklisted);

            // Remove from update tree
            if (!affectedUpdateTree(syncOp)->deleteNode(syncOp->affectedNode())) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTree::deleteNode: affectedNode name="
                                                   << SyncName2WStr(syncOp->affectedNode()->name()));
                return ExitCode::DataError;
            }

            if (!targetUpdateTree(syncOp)->deleteNode(syncOp->correspondingNode())) {
                logCorrespondingNodeErrorMsg(syncOp);
                return ExitCode::DataError;
            }

            return ExitCode::Ok;
        }
    }

    if (syncOp->omit()) {
        // Do not generate job, only push changes in DB and update tree
        std::shared_ptr<Node> node;
        if (ExitInfo exitInfo = propagateCreateToDbAndTree(
                    syncOp, syncOp->correspondingNode()->id().has_value() ? *syncOp->correspondingNode()->id() : std::string(),
                    syncOp->affectedNode()->lastmodified(), node);
            !exitInfo) {
            LOGW_SYNCPAL_WARN(_logger, L"Failed to propagate changes in DB or update tree for: "
                                               << SyncName2WStr(syncOp->affectedNode()->name()));
            return exitInfo;
        }
    } else {
        if (!isLiteSyncActivated() && !enoughLocalSpace(syncOp)) {
            _syncPal->addError(Error(_syncPal->syncDbId(), name(), ExitCode::SystemError, ExitCause::NotEnoughDiskSpace));
            return {ExitCode::SystemError, ExitCause::NotEnoughDiskSpace};
        }

        if (!isValidDestination(syncOp)) {
            if (syncOp->targetSide() == ReplicaSide::Remote) {
                bool exists = false;
                if (auto ioError = IoError::Success; !IoHelper::checkIfPathExists(absoluteLocalFilePath, exists, ioError)) {
                    LOGW_WARN(_logger,
                              L"Error in Utility::checkIfPathExists: " << Utility::formatSyncPath(absoluteLocalFilePath));
                    return ExitCode::SystemError;
                }
                if (!exists) return {ExitCode::DataError, ExitCause::UnexpectedFileSystemEvent};

                // Ignore operation
                if (SyncFileItem syncItem; _syncPal->getSyncFileItem(relativeLocalFilePath, syncItem)) {
                    const Error err(
                            _syncPal->syncDbId(), syncItem.localNodeId().has_value() ? syncItem.localNodeId().value() : "",
                            syncItem.remoteNodeId().has_value() ? syncItem.remoteNodeId().value() : "", syncItem.type(),
                            syncOp->affectedNode()->moveOrigin().has_value() ? syncOp->affectedNode()->moveOrigin().value()
                                                                             : syncItem.path(),
                            syncItem.conflict(), syncItem.inconsistency(), CancelType::Create);
                    _syncPal->addError(err);
                }

                if (const std::shared_ptr<UpdateTree> sourceUpdateTree = affectedUpdateTree(syncOp);
                    !sourceUpdateTree->deleteNode(syncOp->affectedNode())) {
                    LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTree::deleteNode: node name="
                                                       << SyncName2WStr(syncOp->affectedNode()->name()));
                    return ExitCode::DataError;
                }
            }

            ignored = true;
            LOGW_SYNCPAL_INFO(_logger,
                              L"Forbidden destination, operation ignored: " << Utility::formatSyncPath(absoluteLocalFilePath));
            return ExitCode::Ok;
        }

        if (ExitInfo exitInfo = generateCreateJob(syncOp, job); !exitInfo) {
            return exitInfo;
        }

        if (job && syncOp->affectedNode()->type() == NodeType::Directory) {
            // Propagate the directory creation immediately in order to avoid blocking other dependant job creation
            if (const ExitInfo exitInfoRunCreateDirJob = runCreateDirJob(syncOp, job); !exitInfoRunCreateDirJob) {
                std::shared_ptr<CreateDirJob> createDirJob = std::dynamic_pointer_cast<CreateDirJob>(job);
                if (createDirJob && (createDirJob->getStatusCode() == Poco::Net::HTTPResponse::HTTP_BAD_REQUEST ||
                                     createDirJob->getStatusCode() == Poco::Net::HTTPResponse::HTTP_FORBIDDEN)) {
                    if (const ExitInfo exitInfoCheckAlreadyExcluded =
                                checkAlreadyExcluded(absoluteLocalFilePath, createDirJob->parentDirId());
                        !exitInfoCheckAlreadyExcluded) {
                        LOG_SYNCPAL_WARN(_logger, "Error in ExecutorWorker::checkAlreadyExcluded");
                        return exitInfoCheckAlreadyExcluded;
                    }

                    if (const ExitInfo exitInfo = handleForbiddenAction(syncOp, relativeLocalFilePath, ignored); !exitInfo) {
                        LOGW_SYNCPAL_WARN(_logger, L"Error in handleForbiddenAction for item: "
                                                           << Utility::formatSyncPath(relativeLocalFilePath));
                        return exitInfo;
                    }
                    return {ExitCode::BackError, ExitCause::FileAccessError};
                }
                return exitInfoRunCreateDirJob;
            }

            if (const ExitInfo exitInfo =
                        convertToPlaceholder(relativeLocalFilePath, syncOp->targetSide() == ReplicaSide::Remote);
                !exitInfo) {
                LOGW_SYNCPAL_WARN(_logger,
                                  L"Failed to convert to placeholder for: " << SyncName2WStr(syncOp->affectedNode()->name()));
                return exitInfo;
            }

            job.reset();
        }
    }
    return ExitCode::Ok;
}

ExitInfo ExecutorWorker::checkAlreadyExcluded(const SyncPath &absolutePath, const NodeId &parentId) {
    bool alreadyExist = false;

    // List all items in parent dir
    try {
        GetFileListJob job(_syncPal->driveDbId(), parentId);
        ExitCode exitCode = job.runSynchronously();
        if (exitCode != ExitCode::Ok) {
            LOG_SYNCPAL_WARN(_logger, "Error in GetFileListJob::runSynchronously for driveDbId="
                                              << _syncPal->driveDbId() << " nodeId=" << parentId.c_str() << " : " << exitCode
                                              << " " << job.exitCause());
            return {exitCode, job.exitCause()};
        }

        Poco::JSON::Object::Ptr resObj = job.jsonRes();
        if (!resObj) {
            LOG_SYNCPAL_WARN(Log::instance()->getLogger(),
                             "GetFileListJob failed for driveDbId=" << _syncPal->driveDbId() << " nodeId=" << parentId.c_str());
            return {ExitCode::BackError, ExitCause::ApiErr};
        }

        Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
        if (!dataArray) {
            LOG_SYNCPAL_WARN(Log::instance()->getLogger(),
                             "GetFileListJob failed for driveDbId=" << _syncPal->driveDbId() << " nodeId=" << parentId.c_str());
            return {ExitCode::BackError, ExitCause::ApiErr};
        }

        for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
            Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
            std::string name;
            if (!JsonParserUtility::extractValue(obj, nameKey, name)) {
                LOG_SYNCPAL_WARN(
                        Log::instance()->getLogger(),
                        "GetFileListJob failed for driveDbId=" << _syncPal->driveDbId() << " nodeId=" << parentId.c_str());
                return {ExitCode::BackError, ExitCause::ApiErr};
            }

            if (name == absolutePath.filename().string()) {
                alreadyExist = true;
                break;
            }
        }
    } catch (const std::exception &e) {
        LOG_WARN(Log::instance()->getLogger(), "Error in GetFileListJob::GetFileListJob for driveDbId="
                                                       << _syncPal->driveDbId() << " nodeId=" << parentId.c_str()
                                                       << " error=" << e.what());
        return ExitCode::DataError;
    }

    if (!alreadyExist) {
        return ExitCode::Ok;
    }
    return {ExitCode::DataError, ExitCause::FileAlreadyExist};
}

ExitInfo ExecutorWorker::generateCreateJob(SyncOpPtr syncOp, std::shared_ptr<AbstractJob> &job) noexcept {
    // 1. If omit-flag is False, propagate the file or directory to replica Y, because the object is missing there.
    std::shared_ptr<Node> newCorrespondingParentNode = nullptr;
    if (affectedUpdateTree(syncOp)->rootNode() == syncOp->affectedNode()->parentNode()) {
        newCorrespondingParentNode = targetUpdateTree(syncOp)->rootNode();

        if (!newCorrespondingParentNode) {
            LOGW_SYNCPAL_WARN(_logger, L"Failed to get target root node");
            return ExitCode::DataError;
        }
    } else {
        newCorrespondingParentNode = correspondingNodeInOtherTree(syncOp->affectedNode()->parentNode());

        if (!newCorrespondingParentNode) {
            LOGW_SYNCPAL_WARN(_logger,
                              L"Failed to get corresponding parent node: " << SyncName2WStr(syncOp->affectedNode()->name()));
            return ExitCode::DataError;
        }
    }

    if (syncOp->targetSide() == ReplicaSide::Local) {
        SyncPath relativeLocalFilePath = newCorrespondingParentNode->getPath() / syncOp->newName();
        SyncPath absoluteLocalFilePath = _syncPal->localPath() / relativeLocalFilePath;

        bool placeholderCreation = isLiteSyncActivated() && syncOp->affectedNode()->type() == NodeType::File;
        if (placeholderCreation && syncOp->affectedNode()->id().has_value()) {
            const bool isLink = _syncPal->_remoteSnapshot->isLink(*syncOp->affectedNode()->id());
            placeholderCreation = !isLink;
        }

        if (placeholderCreation) {
            if (ExitInfo exitInfo = createPlaceholder(relativeLocalFilePath); !exitInfo) {
                LOGW_SYNCPAL_WARN(_logger,
                                  L"Failed to create placeholder for: " << SyncName2WStr(syncOp->affectedNode()->name()));
                return exitInfo;
            }

            FileStat fileStat;
            IoError ioError = IoError::Success;
            if (!IoHelper::getFileStat(absoluteLocalFilePath, &fileStat, ioError)) {
                LOGW_SYNCPAL_WARN(_logger,
                                  L"Error in IoHelper::getFileStat: " << Utility::formatIoError(absoluteLocalFilePath, ioError));
                return ExitCode::SystemError;
            }

            if (ioError == IoError::NoSuchFileOrDirectory) {
                LOGW_WARN(_logger, L"Item does not exist anymore: " << Utility::formatSyncPath(absoluteLocalFilePath));
                return {ExitCode::DataError, ExitCause::InvalidSnapshot};
            } else if (ioError == IoError::AccessDenied) {
                LOGW_WARN(_logger, L"Item misses search permission: " << Utility::formatSyncPath(absoluteLocalFilePath));
                return {ExitCode::SystemError, ExitCause::FileAccessError};
            }

            std::shared_ptr<Node> newNode = nullptr;
            if (ExitInfo exitInfo = propagateCreateToDbAndTree(syncOp, std::to_string(fileStat.inode),
                                                               syncOp->affectedNode()->lastmodified(), newNode);
                !exitInfo) {
                LOGW_SYNCPAL_WARN(_logger, L"Failed to propagate changes in DB or update tree for: "
                                                   << SyncName2WStr(syncOp->affectedNode()->name()));
                return exitInfo;
            }

            // Check for inconsistency
            if (syncOp->affectedNode()->inconsistencyType() != InconsistencyType::None) {
                // Notify user that the file has been renamed
                SyncFileItem syncItem;
                if (_syncPal->getSyncFileItem(relativeLocalFilePath, syncItem)) {
                    Error err(_syncPal->syncDbId(), syncItem.localNodeId().has_value() ? syncItem.localNodeId().value() : "",
                              syncItem.remoteNodeId().has_value() ? syncItem.remoteNodeId().value() : "", syncItem.type(),
                              syncItem.newPath().has_value() ? syncItem.newPath().value() : syncItem.path(), syncItem.conflict(),
                              syncItem.inconsistency());
                    _syncPal->addError(err);
                }
            }
        } else {
            if (syncOp->affectedNode()->type() == NodeType::Directory) {
                job = std::make_shared<LocalCreateDirJob>(absoluteLocalFilePath);
            } else {
                bool exists = false;
                IoError ioError = IoError::Success;
                if (!IoHelper::checkIfPathExists(absoluteLocalFilePath, exists, ioError)) {
                    LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: "
                                               << Utility::formatIoError(absoluteLocalFilePath, ioError));
                    return ExitCode::SystemError;
                }
                if (ioError == IoError::AccessDenied) {
                    LOGW_WARN(_logger, L"Item misses search permission: " << Utility::formatSyncPath(absoluteLocalFilePath));
                    return {ExitCode::SystemError, ExitCause::FileAccessError};
                }

                if (exists) {
                    // Normal in lite sync mode
                    // or in case where a remote item and a local item have same name with different cases
                    // (ex: 'test.txt' on local replica needs to be uploaded, 'Text.txt' on remote replica needs to be
                    // downloaded)
                    if (ParametersCache::isExtendedLogEnabled()) {
                        LOGW_SYNCPAL_DEBUG(_logger, L"File: " << Utility::formatSyncPath(absoluteLocalFilePath)
                                                              << L" already exists on local replica");
                    }
                    // Do not propagate this file
                    return ExitCode::Ok;
                } else {
                    try {
                        job = std::make_shared<DownloadJob>(
                                _syncPal->driveDbId(),
                                syncOp->affectedNode()->id().has_value() ? *syncOp->affectedNode()->id() : std::string(),
                                absoluteLocalFilePath, syncOp->affectedNode()->size(),
                                syncOp->affectedNode()->createdAt().has_value() ? *syncOp->affectedNode()->createdAt() : 0,
                                syncOp->affectedNode()->lastmodified().has_value() ? *syncOp->affectedNode()->lastmodified() : 0,
                                true);
                    } catch (std::exception const &e) {
                        LOGW_SYNCPAL_WARN(_logger, L"Error in DownloadJob::DownloadJob for driveDbId="
                                                           << _syncPal->driveDbId() << L" : " << Utility::s2ws(e.what()));
                        return ExitCode::DataError;
                    }

                    job->setAffectedFilePath(relativeLocalFilePath);
                }
            }
        }
    } else {
        SyncPath relativeLocalFilePath = syncOp->nodePath(ReplicaSide::Local);
        SyncPath absoluteLocalFilePath = _syncPal->localPath() / relativeLocalFilePath;
        if (syncOp->affectedNode()->type() == NodeType::Directory) {
            if (ExitInfo exitInfo = convertToPlaceholder(relativeLocalFilePath, syncOp->targetSide() == ReplicaSide::Remote);
                !exitInfo) {
                LOGW_SYNCPAL_WARN(_logger,
                                  L"Failed to convert to placeholder for: " << SyncName2WStr(syncOp->affectedNode()->name()));
                _syncPal->setRestart(true);

                if (!_syncPal->updateTree(ReplicaSide::Local)->deleteNode(syncOp->affectedNode())) {
                    LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTree::deleteNode: node name="
                                                       << SyncName2WStr(syncOp->affectedNode()->name()));
                }

                return exitInfo;
            }

            try {
                job = std::make_shared<CreateDirJob>(
                        _syncPal->driveDbId(), absoluteLocalFilePath,
                        newCorrespondingParentNode->id().has_value() ? *newCorrespondingParentNode->id() : std::string(),
                        syncOp->newName());
            } catch (std::exception const &e) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in CreateDirJob::CreateDirJob for driveDbId="
                                                   << _syncPal->driveDbId() << L" : " << Utility::s2ws(e.what()));
                return ExitCode::DataError;
            }
        } else {
            if (ExitInfo exitInfo = convertToPlaceholder(relativeLocalFilePath, true); !exitInfo) {
                LOGW_SYNCPAL_WARN(_logger,
                                  L"Failed to convert to placeholder for: " << SyncName2WStr(syncOp->affectedNode()->name()));
                return exitInfo;
            }

            uint64_t filesize = 0;
            if (ExitInfo exitInfo = getFileSize(absoluteLocalFilePath, filesize); !exitInfo) {
                LOGW_SYNCPAL_WARN(_logger,
                                  L"Error in ExecutorWorker::getFileSize for " << Utility::formatSyncPath(absoluteLocalFilePath));
                return exitInfo;
            }

            if (filesize > useUploadSessionThreshold) {
                try {
                    int uploadSessionParallelJobs = ParametersCache::instance()->parameters().uploadSessionParallelJobs();
                    job = std::make_shared<DriveUploadSession>(
                            _syncPal->driveDbId(), _syncPal->_syncDb, absoluteLocalFilePath, syncOp->affectedNode()->name(),
                            newCorrespondingParentNode->id().has_value() ? *newCorrespondingParentNode->id() : std::string(),
                            syncOp->affectedNode()->lastmodified() ? *syncOp->affectedNode()->lastmodified() : 0,
                            isLiteSyncActivated(), uploadSessionParallelJobs);
                } catch (std::exception const &e) {
                    LOGW_SYNCPAL_WARN(_logger, L"Error in DriveUploadSession::DriveUploadSession: " << Utility::s2ws(e.what()));
                    return ExitCode::DataError;
                }
            } else {
                try {
                    job = std::make_shared<UploadJob>(
                            _syncPal->driveDbId(), absoluteLocalFilePath, syncOp->affectedNode()->name(),
                            newCorrespondingParentNode->id().has_value() ? *newCorrespondingParentNode->id() : std::string(),
                            syncOp->affectedNode()->lastmodified() ? *syncOp->affectedNode()->lastmodified() : 0);
                } catch (std::exception const &e) {
                    LOGW_SYNCPAL_WARN(_logger, L"Error in UploadJob::UploadJob for driveDbId=" << _syncPal->driveDbId() << L" : "
                                                                                               << Utility::s2ws(e.what()));
                    return ExitCode::DataError;
                }
            }

            job->setAffectedFilePath(relativeLocalFilePath);
        }
    }

    if (job) {
        if (_syncPal->vfsMode() == VirtualFileMode::Mac || _syncPal->vfsMode() == VirtualFileMode::Win) {
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

    return ExitCode::Ok;
}

ExitInfo ExecutorWorker::checkLiteSyncInfoForCreate(SyncOpPtr syncOp, const SyncPath &path, bool &isDehydratedPlaceholder) {
    isDehydratedPlaceholder = false;

    if (syncOp->targetSide() == ReplicaSide::Remote) {
        if (syncOp->affectedNode()->type() == NodeType::Directory) {
            return ExitCode::Ok;
        }

        bool isPlaceholder = false;
        bool isHydrated = false;
        bool isSyncing = false;
        int progress = 0;
        if (!_syncPal->vfsStatus(path, isPlaceholder, isHydrated, isSyncing, progress)) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in vfsStatus: " << Utility::formatSyncPath(path));
            return {ExitCode::SystemError, ExitCause::FileAccessError};
        }

        if (isPlaceholder && !isHydrated && !isSyncing) {
            LOGW_SYNCPAL_INFO(_logger, L"Do not upload dehydrated placeholders: " << Utility::formatSyncPath(path));
            isDehydratedPlaceholder = true;
        }
    }

    return ExitCode::Ok;
}

ExitInfo ExecutorWorker::createPlaceholder(const SyncPath &relativeLocalPath) {
    SyncFileItem syncItem;
    if (!_syncPal->getSyncFileItem(relativeLocalPath, syncItem)) {
        LOGW_SYNCPAL_WARN(_logger,
                          L"Failed to retrieve SyncFileItem associated to item: " << Utility::formatSyncPath(relativeLocalPath));
        return {ExitCode::DataError, ExitCause::InvalidSnapshot};
    }

    bool exists = false;
    IoError ioError = IoError::Success;
    SyncPath absoluteLocalPath = _syncPal->localPath() / relativeLocalPath;
    if (!IoHelper::checkIfPathExists(absoluteLocalPath, exists, ioError)) {
        LOGW_SYNCPAL_WARN(_logger,
                          L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(absoluteLocalPath, ioError));
        return ExitCode::SystemError;
    }

    if (ioError == IoError::AccessDenied) {
        LOGW_WARN(_logger, L"Access denied to " << Path2WStr(absoluteLocalPath).c_str());
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    if (exists) {
        LOGW_WARN(_logger, L"Item already exists: " << Utility::formatSyncPath(absoluteLocalPath));
        return {ExitCode::DataError, ExitCause::InvalidSnapshot};
    }

    if (!_syncPal->vfsCreatePlaceholder(relativeLocalPath, syncItem)) {
        // TODO: vfs functions should output an ioError parameter
        // Check if the item already exists on local replica
        return processCreateOrConvertToPlaceholderError(relativeLocalPath, true);
    }

    return ExitCode::Ok;
}

ExitInfo ExecutorWorker::convertToPlaceholder(const SyncPath &relativeLocalPath, bool hydrated) {
    SyncFileItem syncItem;
    if (!_syncPal->getSyncFileItem(relativeLocalPath, syncItem)) {
        LOGW_SYNCPAL_WARN(_logger,
                          L"Failed to retrieve SyncFileItem associated to item: " << Utility::formatSyncPath(relativeLocalPath));
        return {ExitCode::DataError, ExitCause::InvalidSnapshot};
    }

    SyncPath absoluteLocalFilePath = _syncPal->localPath() / relativeLocalPath;

#ifdef __APPLE__
    // VfsMac::convertToPlaceholder needs only SyncFileItem::_dehydrated
    syncItem.setDehydrated(!hydrated);
#elif _WIN32
    // VfsWin::convertToPlaceholder needs only SyncFileItem::_localNodeId
    FileStat fileStat;
    IoError ioError = IoError::Success;
    if (!IoHelper::getFileStat(absoluteLocalFilePath, &fileStat, ioError)) {
        LOGW_SYNCPAL_WARN(_logger, L"Error in IoHelper::getFileStat: " << Utility::formatIoError(absoluteLocalFilePath, ioError));
        return ExitCode::SystemError;
    }

    if (ioError == IoError::NoSuchFileOrDirectory) {
        LOGW_SYNCPAL_WARN(_logger, L"Item does not exist anymore: " << Utility::formatSyncPath(absoluteLocalFilePath));
        return {ExitCode::DataError, ExitCause::InvalidSnapshot};
    } else if (ioError == IoError::AccessDenied) {
        LOGW_SYNCPAL_WARN(_logger, L"Item misses search permission: " << Utility::formatSyncPath(absoluteLocalFilePath));
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    syncItem.setLocalNodeId(std::to_string(fileStat.inode));
#endif

    if (!_syncPal->vfsConvertToPlaceholder(absoluteLocalFilePath, syncItem)) {
        // TODO: vfs functions should output an ioError parameter
        // Check that the item exists on local replica
        return processCreateOrConvertToPlaceholderError(relativeLocalPath, false);
    }

    if (!_syncPal->vfsSetPinState(absoluteLocalFilePath, hydrated ? PinState::AlwaysLocal : PinState::OnlineOnly)) {
        LOGW_SYNCPAL_WARN(_logger, L"Error in vfsSetPinState: " << Utility::formatSyncPath(absoluteLocalFilePath));
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    return ExitCode::Ok;
}

ExitInfo ExecutorWorker::processCreateOrConvertToPlaceholderError(const SyncPath &relativeLocalPath, bool create) {
    // TODO: Simplify/remove this function when vfs functions will output an ioError parameter
    SyncPath absoluteLocalFilePath = _syncPal->localPath() / relativeLocalPath;
    bool exists = false;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(absoluteLocalFilePath, exists, ioError)) {
        LOGW_SYNCPAL_WARN(_logger,
                          L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(absoluteLocalFilePath, ioError));
        return ExitCode::SystemError;
    }

    if (ioError == IoError::AccessDenied) {
        LOGW_SYNCPAL_WARN(_logger, L"Item misses search permission: " << Utility::formatSyncPath(absoluteLocalFilePath));
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    if (create && exists) {
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    } else if (!create && !exists) {
        return {ExitCode::DataError, ExitCause::InvalidSnapshot};
    }

    if (create) {
        // Check if the parent folder exists on local replica
        bool parentExists = false;
        if (!IoHelper::checkIfPathExists(absoluteLocalFilePath.parent_path(), parentExists, ioError)) {
            LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: "
                                       << Utility::formatIoError(absoluteLocalFilePath.parent_path(), ioError));
            return ExitCode::SystemError;
        }

        if (ioError == IoError::AccessDenied) {
            LOGW_WARN(_logger,
                      L"Item misses search permission: " << Utility::formatSyncPath(absoluteLocalFilePath.parent_path()));
            return {ExitCode::SystemError, ExitCause::FileAccessError};
        }

        if (!parentExists) {
            return {ExitCode::DataError, ExitCause::InvalidSnapshot};
        }
    }

    return {ExitCode::SystemError, ExitCause::FileAccessError};
}

// !!! When returning with hasError == true, _executorExitCode and _executorExitCause must be set !!!
ExitInfo ExecutorWorker::handleEditOp(SyncOpPtr syncOp, std::shared_ptr<AbstractJob> &job, bool &ignored) {
    // The execution of the edit operation consists of three steps:
    // 1. If omit-flag is False, propagate the file to replicaY, replacing the existing one.
    // 2. Insert a new entry into the database, to avoid that the object is detected again by compute_ops() on the next sync
    // iteration.
    // 3. If the omit flag is False, update the updatetreeY structure to ensure that follow-up operations can execute correctly,
    // as they are based on the information in this structure.
    ignored = false;

    SyncPath relativeLocalFilePath = syncOp->nodePath(ReplicaSide::Local);

    if (relativeLocalFilePath.empty()) {
        return ExitCode::DataError;
    }

    if (isLiteSyncActivated()) {
        SyncPath absoluteLocalFilePath = _syncPal->localPath() / relativeLocalFilePath;
        bool ignoreItem = false;
        bool isSyncing = false;
        if (ExitInfo exitInfo = checkLiteSyncInfoForEdit(syncOp, absoluteLocalFilePath, ignoreItem, isSyncing); !exitInfo) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in checkLiteSyncInfoForEdit");
            return exitInfo;
        }

        if (ignoreItem) {
            ignored = true;
            return ExitCode::Ok;
        }

        if (isSyncing) {
            return ExitCode::Ok;
        }
    }

    if (syncOp->omit()) {
        // Do not generate job, only push changes in DB and update tree
        std::shared_ptr<Node> node;
        if (ExitInfo exitInfo = propagateEditToDbAndTree(
                    syncOp, syncOp->correspondingNode()->id().has_value() ? *syncOp->correspondingNode()->id() : std::string(),
                    syncOp->affectedNode()->lastmodified(), node);
            !exitInfo) {
            LOGW_SYNCPAL_WARN(_logger, L"Failed to propagate changes in DB or update tree for: "
                                               << SyncName2WStr(syncOp->affectedNode()->name()));
            return exitInfo;
        }
    } else {
        if (!enoughLocalSpace(syncOp)) {
            _syncPal->addError(Error(_syncPal->syncDbId(), name(), ExitCode::SystemError, ExitCause::NotEnoughDiskSpace));
            return {ExitCode::SystemError, ExitCause::NotEnoughDiskSpace};
        }

        if (ExitInfo exitInfo = generateEditJob(syncOp, job); !exitInfo) {
            return exitInfo;
        }
    }
    return ExitCode::Ok;
}

ExitInfo ExecutorWorker::generateEditJob(SyncOpPtr syncOp, std::shared_ptr<AbstractJob> &job) {
    // 1. If omit-flag is False, propagate the file to replicaY, replacing the existing one.
    if (syncOp->targetSide() == ReplicaSide::Local) {
        SyncPath relativeLocalFilePath = syncOp->nodePath(ReplicaSide::Local);
        SyncPath absoluteLocalFilePath = _syncPal->localPath() / relativeLocalFilePath;

        try {
            job = std::make_shared<DownloadJob>(
                    _syncPal->driveDbId(), syncOp->affectedNode()->id() ? *syncOp->affectedNode()->id() : std::string(),
                    absoluteLocalFilePath, syncOp->affectedNode()->size(),
                    syncOp->affectedNode()->createdAt().has_value() ? *syncOp->affectedNode()->createdAt() : 0,
                    syncOp->affectedNode()->lastmodified().has_value() ? *syncOp->affectedNode()->lastmodified() : 0, false);
        } catch (std::exception const &e) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in DownloadJob::DownloadJob for driveDbId=" << _syncPal->driveDbId() << L" : "
                                                                                           << Utility::s2ws(e.what()));
            return ExitCode::DataError;
        }

        job->setAffectedFilePath(relativeLocalFilePath);

        // Set callbacks
        std::shared_ptr<DownloadJob> downloadJob = std::dynamic_pointer_cast<DownloadJob>(job);

        if (_syncPal->vfsMode() == VirtualFileMode::Mac || _syncPal->vfsMode() == VirtualFileMode::Win) {
            std::function<bool(const SyncPath &, PinState)> vfsSetPinStateCallback =
                    std::bind(&SyncPal::vfsSetPinState, _syncPal, std::placeholders::_1, std::placeholders::_2);
            downloadJob->setVfsSetPinStateCallback(vfsSetPinStateCallback);

            std::function<bool(const SyncPath &, bool, int, bool)> vfsForceStatusCallback =
                    std::bind(&SyncPal::vfsForceStatus, _syncPal, std::placeholders::_1, std::placeholders::_2,
                              std::placeholders::_3, std::placeholders::_4);
            downloadJob->setVfsForceStatusCallback(vfsForceStatusCallback);

            std::function<bool(const SyncPath &, const SyncTime &, const SyncTime &, const int64_t, const NodeId &,
                               std::string &)>
                    vfsUpdateMetadataCallback =
                            std::bind(&SyncPal::vfsUpdateMetadata, _syncPal, std::placeholders::_1, std::placeholders::_2,
                                      std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6);
            downloadJob->setVfsUpdateMetadataCallback(vfsUpdateMetadataCallback);
        }
    } else {
        SyncPath relativeLocalFilePath = syncOp->nodePath(ReplicaSide::Local);
        SyncPath absoluteLocalFilePath = _syncPal->localPath() / relativeLocalFilePath;

        uint64_t filesize;
        if (ExitInfo exitInfo = getFileSize(absoluteLocalFilePath, filesize); !exitInfo) {
            LOGW_WARN(_logger, L"Error in ExecutorWorker::getFileSize for " << Utility::formatSyncPath(absoluteLocalFilePath));
            return exitInfo;
        }

        if (!syncOp->correspondingNode()->id()) {
            // Should not happen
            LOGW_SYNCPAL_WARN(_logger, L"Edit operation with empty corresponding node id for "
                                               << Utility::formatSyncPath(absoluteLocalFilePath));
            Sentry::Handler::captureMessage(Sentry::Level::Warning, "ExecutorWorker::generateEditJob",
                                                      "Edit operation with empty corresponding node id");
            return ExitCode::DataError;
        }

        if (filesize > useUploadSessionThreshold) {
            try {
                int uploadSessionParallelJobs = ParametersCache::instance()->parameters().uploadSessionParallelJobs();
                job = std::make_shared<DriveUploadSession>(
                        _syncPal->driveDbId(), _syncPal->_syncDb, absoluteLocalFilePath,
                        syncOp->correspondingNode()->id() ? *syncOp->correspondingNode()->id() : std::string(),
                        syncOp->affectedNode()->lastmodified() ? *syncOp->affectedNode()->lastmodified() : 0,
                        isLiteSyncActivated(), uploadSessionParallelJobs);
            } catch (std::exception const &e) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in DriveUploadSession::DriveUploadSession: " << Utility::s2ws(e.what()));
                return ExitCode::DataError;
            };
        } else {
            try {
                job = std::make_shared<UploadJob>(
                        _syncPal->driveDbId(), absoluteLocalFilePath,
                        syncOp->correspondingNode()->id() ? *syncOp->correspondingNode()->id() : std::string(),
                        syncOp->affectedNode()->lastmodified() ? *syncOp->affectedNode()->lastmodified() : 0);
            } catch (std::exception const &e) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in UploadJob::UploadJob for driveDbId=" << _syncPal->driveDbId() << L" : "
                                                                                           << Utility::s2ws(e.what()));
                return ExitCode::DataError;
            }

            // Set callbacks
            std::shared_ptr<UploadJob> uploadJob = std::dynamic_pointer_cast<UploadJob>(job);
            if (_syncPal->vfsMode() == VirtualFileMode::Mac || _syncPal->vfsMode() == VirtualFileMode::Win) {
                std::function<bool(const SyncPath &, bool, int, bool)> vfsForceStatusCallback =
                        std::bind(&SyncPal::vfsForceStatus, _syncPal, std::placeholders::_1, std::placeholders::_2,
                                  std::placeholders::_3, std::placeholders::_4);
                uploadJob->setVfsForceStatusCallback(vfsForceStatusCallback);
            }
        }

        job->setAffectedFilePath(relativeLocalFilePath);
    }
    return ExitCode::Ok;
}

ExitInfo ExecutorWorker::fixModificationDate(SyncOpPtr syncOp, const SyncPath &absolutePath) {
    const auto id = syncOp->affectedNode()->id().has_value() ? syncOp->affectedNode()->id().value() : "";
    LOGW_SYNCPAL_DEBUG(_logger, L"Do not upload dehydrated placeholders: " << Utility::formatSyncPath(absolutePath) << L" ("
                                                                           << Utility::s2ws(id) << L")");

    // Update last modification date in order to avoid generating more EDIT operations.
    bool found = false;
    DbNode dbNode;
    if (!_syncPal->_syncDb->node(*syncOp->correspondingNode()->idb(), dbNode, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::node");
        return {ExitCode::DbError, ExitCause::DbAccessError};
    }
    if (!found) {
        LOG_SYNCPAL_DEBUG(_logger, "Failed to retrieve node for dbId=" << *syncOp->correspondingNode()->idb());
        return {ExitCode::DataError, ExitCause::DbEntryNotFound};
    }

    bool exists = false;
    if (!Utility::setFileDates(absolutePath, dbNode.created(), dbNode.lastModifiedRemote(), false, exists)) {
        LOGW_SYNCPAL_WARN(_logger, L"Error in Utility::setFileDates: " << Utility::formatSyncPath(absolutePath));
    }
    if (exists) {
        LOGW_SYNCPAL_INFO(_logger,
                          L"Last modification date updated locally to avoid further wrongly generated EDIT operations for file: "
                                  << Utility::formatSyncPath(absolutePath));
    }
    // If file does not exist anymore, do nothing special. This is fine, it will not generate EDIT operations anymore.
    return ExitCode::Ok;
}

ExitInfo ExecutorWorker::checkLiteSyncInfoForEdit(SyncOpPtr syncOp, const SyncPath &absolutePath, bool &ignoreItem,
                                                  bool &isSyncing) {
    ignoreItem = false;

    bool isPlaceholder = false;
    bool isHydrated = false;
    bool isSyncingTmp = false;
    int progress = 0;
    if (!_syncPal->vfsStatus(absolutePath, isPlaceholder, isHydrated, isSyncingTmp, progress)) {
        LOGW_SYNCPAL_WARN(_logger, L"Error in vfsStatus: " << Utility::formatSyncPath(absolutePath));
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    if (syncOp->targetSide() == ReplicaSide::Remote) {
        if (isPlaceholder && !isHydrated) {
            ignoreItem = true;
            return fixModificationDate(syncOp, absolutePath);
        }
    } else {
        if (isPlaceholder) {
            PinState pinState = PinState::Unspecified;
            if (!_syncPal->vfsPinState(absolutePath, pinState)) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in vfsPinState for file: " << Utility::formatSyncPath(absolutePath));
                return {ExitCode::SystemError, ExitCause::InconsistentPinState};
            }

            switch (pinState) {
                case PinState::Inherited: {
                    // TODO : what do we do in that case??
                    LOG_SYNCPAL_WARN(_logger, "Inherited pin state not implemented yet");
                    return ExitCode::LogicError;
                }
                case PinState::AlwaysLocal: {
                    if (isSyncingTmp) {
                        // Ignore this item until it is synchronized
                        isSyncing = true;
                    } else if (isHydrated) {
                        // Download
                    }
                    break;
                }
                case PinState::OnlineOnly: {
                    // Update metadata
                    std::string error;
                    _syncPal->vfsUpdateMetadata(
                            absolutePath,
                            syncOp->affectedNode()->createdAt().has_value() ? *syncOp->affectedNode()->createdAt() : 0,
                            syncOp->affectedNode()->lastmodified().has_value() ? *syncOp->affectedNode()->lastmodified() : 0,
                            syncOp->affectedNode()->size(),
                            syncOp->affectedNode()->id().has_value() ? *syncOp->affectedNode()->id() : std::string(), error);
                    // TODO: Vfs functions should return an ExitInfo struct
                    syncOp->setOmit(true); // Do not propagate change in file system, only in DB
                    if (!error.empty()) {
                        return {ExitCode::SystemError, ExitCause::FileAccessError};
                    }
                    break;
                }
                case PinState::Unspecified:
                default: {
                    LOGW_SYNCPAL_DEBUG(_logger, L"Ignore EDIT for file: " << Path2WStr(absolutePath));
                    ignoreItem = true;
                    return ExitCode::Ok;
                }
            }
        }
    }

    return ExitCode::Ok;
}

ExitInfo ExecutorWorker::handleMoveOp(SyncOpPtr syncOp, bool &ignored, bool &bypassProgressComplete) {
    // The three execution steps are as follows:
    // 1. If omit-flag is False, move the object on replica Y (where it still needs to be moved) from uY to vY, changing the
    // name to nameX.
    // 2. Update the database entry, to avoid detecting the move operation again.
    // 3. If the omit flag is False, update the updatetreeY structure to ensure that follow-up operations can execute
    // correctly, as they are based on the information in this structure.
    ignored = false;
    bypassProgressComplete = false;
    if (syncOp->omit()) {
        // Do not generate job, only push changes in DB and update tree
        if (syncOp->hasConflict()) {
            bool propagateChange = true;
            ExitInfo exitInfo = propagateConflictToDbAndTree(syncOp, propagateChange);

            Error err(_syncPal->syncDbId(),
                      syncOp->conflict().localNode() != nullptr
                              ? (syncOp->conflict().localNode()->id().has_value() ? syncOp->conflict().localNode()->id().value()
                                                                                  : "")
                              : "",
                      syncOp->conflict().remoteNode() != nullptr
                              ? (syncOp->conflict().remoteNode()->id().has_value() ? syncOp->conflict().remoteNode()->id().value()
                                                                                   : "")
                              : "",
                      syncOp->conflict().localNode() != nullptr ? syncOp->conflict().localNode()->type() : NodeType::Unknown,
                      syncOp->affectedNode()->moveOrigin().has_value() ? syncOp->affectedNode()->moveOrigin().value()
                                                                       : syncOp->affectedNode()->getPath(),
                      syncOp->conflict().type());

            _syncPal->addError(err);

            if (!propagateChange || !exitInfo) {
                return exitInfo;
            }
        }

        if (ExitInfo exitInfo = propagateMoveToDbAndTree(syncOp); !exitInfo) {
            LOGW_SYNCPAL_WARN(_logger, L"Failed to propagate changes in DB or update tree for: "
                                               << SyncName2WStr(syncOp->affectedNode()->name()));
            return exitInfo;
        }
    } else {
        if (ExitInfo exitInfo = generateMoveJob(syncOp, ignored, bypassProgressComplete); !exitInfo) {
            return exitInfo;
        }
    }
    return ExitCode::Ok;
}

ExitInfo ExecutorWorker::generateMoveJob(SyncOpPtr syncOp, bool &ignored, bool &bypassProgressComplete) {
    bypassProgressComplete = false;

    // 1. If omit-flag is False, move the object on replica Y (where it still needs to be moved) from uY to vY, changing the
    // name to nameX.
    std::shared_ptr<AbstractJob> job = nullptr;

    SyncPath relativeDestLocalFilePath;
    SyncPath absoluteDestLocalFilePath;
    SyncPath relativeOriginLocalFilePath;

    if (syncOp->targetSide() == ReplicaSide::Local) {
        // Target side is local, so corresponding node is on local side.
        std::shared_ptr<Node> correspondingNode = syncOp->correspondingNode();
        if (!correspondingNode) {
            LOGW_SYNCPAL_WARN(_logger, L"Corresponding node not found for item with "
                                               << Utility::formatSyncPath(syncOp->affectedNode()->getPath()));
            return ExitCode::DataError;
        }

        // Get the new parent node
        std::shared_ptr<Node> parentNode =
                syncOp->newParentNode() ? syncOp->newParentNode() : syncOp->affectedNode()->parentNode();
        if (!parentNode) {
            LOGW_SYNCPAL_WARN(_logger,
                              L"Parent node not found for item with " << Utility::formatSyncPath(correspondingNode->getPath()));
            return ExitCode::DataError;
        }

        relativeDestLocalFilePath = parentNode->getPath() / syncOp->newName();
        relativeOriginLocalFilePath = correspondingNode->getPath();
        absoluteDestLocalFilePath = _syncPal->localPath() / relativeDestLocalFilePath;
        SyncPath absoluteOriginLocalFilePath = _syncPal->localPath() / relativeOriginLocalFilePath;
        job = std::make_shared<LocalMoveJob>(absoluteOriginLocalFilePath, absoluteDestLocalFilePath);
    } else {
        // Target side is remote, so affected node is on local side.
        std::shared_ptr<Node> correspondingNode = syncOp->correspondingNode();
        if (!correspondingNode) {
            LOGW_SYNCPAL_WARN(_logger, L"Corresponding node not found for item "
                                               << Utility::formatSyncPath(syncOp->affectedNode()->getPath()));
            return ExitCode::DataError;
        }

        // Get the new parent node
        std::shared_ptr<Node> parentNode =
                syncOp->newParentNode() ? syncOp->newParentNode() : syncOp->affectedNode()->parentNode();
        if (!parentNode) {
            LOGW_SYNCPAL_WARN(_logger,
                              L"Parent node not found for item " << Utility::formatSyncPath(correspondingNode->getPath()));
            return ExitCode::DataError;
        }

        relativeDestLocalFilePath = parentNode->getPath() / syncOp->newName();
        relativeOriginLocalFilePath = correspondingNode->getPath();
        absoluteDestLocalFilePath = _syncPal->localPath() / relativeDestLocalFilePath;
        SyncPath absoluteOriginLocalFilePath = _syncPal->localPath() / relativeOriginLocalFilePath;
        job = std::make_shared<LocalMoveJob>(absoluteOriginLocalFilePath, absoluteDestLocalFilePath);

        if (relativeOriginLocalFilePath.parent_path() == relativeDestLocalFilePath.parent_path()) {
            // This is just a rename
            try {
                job = std::make_shared<RenameJob>(_syncPal->driveDbId(),
                                                  correspondingNode->id().has_value() ? *correspondingNode->id() : std::string(),
                                                  absoluteDestLocalFilePath);
            } catch (std::exception const &e) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in RenameJob::RenameJob for driveDbId=" << _syncPal->driveDbId() << L" : "
                                                                                           << Utility::s2ws(e.what()));
                return ExitCode::DataError;
            }
        } else {
            // This is a move

            // For all conflicts involving an "undo move" operation, the correct parent is already stored in the syncOp
            std::shared_ptr<Node> remoteParentNode = syncOp->conflict().type() == ConflictType::MoveParentDelete ||
                                                                     syncOp->conflict().type() == ConflictType::MoveMoveSource ||
                                                                     syncOp->conflict().type() == ConflictType::MoveMoveCycle
                                                             ? parentNode
                                                             : correspondingNodeInOtherTree(parentNode);
            if (!remoteParentNode) {
                LOGW_SYNCPAL_WARN(_logger, L"Parent node not found for item " << Path2WStr(parentNode->getPath()));
                return ExitCode::DataError;
            }
            try {
                job = std::make_shared<MoveJob>(_syncPal->driveDbId(), absoluteDestLocalFilePath,
                                                correspondingNode->id().has_value() ? *correspondingNode->id() : std::string(),
                                                remoteParentNode->id().has_value() ? *remoteParentNode->id() : std::string(),
                                                syncOp->newName());
            } catch (std::exception &e) {
                LOG_SYNCPAL_WARN(_logger, "Error in GetTokenFromAppPasswordJob::GetTokenFromAppPasswordJob: error=" << e.what());
                return ExitCode::DataError;
            }
        }

        if (syncOp->hasConflict()) {
            job->setBypassCheck(true);
        }

        // Set callbacks
        if (_syncPal->vfsMode() == VirtualFileMode::Mac || _syncPal->vfsMode() == VirtualFileMode::Win) {
            std::function<bool(const SyncPath &, bool, int, bool)> vfsForceStatusCallback =
                    std::bind(&SyncPal::vfsForceStatus, _syncPal, std::placeholders::_1, std::placeholders::_2,
                              std::placeholders::_3, std::placeholders::_4);
            job->setVfsForceStatusCallback(vfsForceStatusCallback);

            std::function<bool(const SyncPath &, bool &, bool &, bool &, int &)> vfsStatusCallback =
                    std::bind(&SyncPal::vfsStatus, _syncPal, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                              std::placeholders::_4, std::placeholders::_5);
            job->setVfsStatusCallback(vfsStatusCallback);
        }
    }

    job->setAffectedFilePath(relativeDestLocalFilePath);
    job->runSynchronously();

    if (job->exitCode() == ExitCode::Ok && syncOp->conflict().type() != ConflictType::None) {
        // Conflict fixing job finished successfully
        // Propagate changes to DB and update trees
        std::shared_ptr<Node> newNode = nullptr;
        if (ExitInfo exitInfo = propagateChangeToDbAndTree(syncOp, job, newNode); !exitInfo) {
            LOGW_WARN(_logger,
                      L"Failed to propagate changes in DB or update tree for: " << SyncName2WStr(syncOp->affectedNode()->name()));
            return exitInfo;
        }

        // Send conflict notification
        SyncFileItem syncItem;
        if (_syncPal->getSyncFileItem(syncOp->affectedNode()->getPath(), syncItem)) {
            NodeId localNodeId = syncOp->correspondingNode()->side() == ReplicaSide::Local
                                         ? syncItem.localNodeId().has_value() ? syncItem.localNodeId().value() : ""
                                         : "";
            NodeId remoteNodeId = syncOp->correspondingNode()->side() == ReplicaSide::Local ? ""
                                  : syncItem.remoteNodeId().has_value()                     ? syncItem.remoteNodeId().value()
                                                                                            : "";

            Error err(_syncPal->syncDbId(), localNodeId, remoteNodeId, syncItem.type(),
                      syncItem.newPath().has_value() ? syncItem.newPath().value() : syncItem.path(), syncItem.conflict(),
                      syncItem.inconsistency(), CancelType::None,
                      localNodeId.empty() ? relativeDestLocalFilePath : absoluteDestLocalFilePath);
            _syncPal->addError(err);
        }

        return ExitCode::Ok;
    }

    return handleFinishedJob(job, syncOp, syncOp->affectedNode()->getPath(), ignored, bypassProgressComplete);
}

ExitInfo ExecutorWorker::handleDeleteOp(SyncOpPtr syncOp, bool &ignored, bool &bypassProgressComplete) {
    // The three execution steps are as follows:
    // 1. If omit-flag is False, delete the file or directory on replicaY, because the objects till exists there
    // 2. Remove the entry from the database. If nX is a directory node, also remove all entries for each node n ∈ S. This
    // avoids that the object(s) are detected again by compute_ops() on the next sync iteration
    // 3. Update the update tree structures to ensure that follow-up operations can execute correctly, as they are based on
    // the information in these structures
    ignored = false;
    bypassProgressComplete = false;

    if (syncOp->omit()) {
        // Do not generate job, only push changes in DB and update tree
        if (syncOp->hasConflict() &&
            syncOp->conflict().type() !=
                    ConflictType::EditDelete) { // Error message handled with move operation in case Edit-Delete conflict
            bool propagateChange = true;
            ExitInfo exitInfo = propagateConflictToDbAndTree(syncOp, propagateChange);

            Error err(_syncPal->syncDbId(),
                      syncOp->conflict().localNode() != nullptr
                              ? (syncOp->conflict().localNode()->id().has_value() ? syncOp->conflict().localNode()->id().value()
                                                                                  : "")
                              : "",
                      syncOp->conflict().remoteNode() != nullptr
                              ? (syncOp->conflict().remoteNode()->id().has_value() ? syncOp->conflict().remoteNode()->id().value()
                                                                                   : "")
                              : "",
                      syncOp->conflict().localNode() != nullptr ? syncOp->conflict().localNode()->type() : NodeType::Unknown,
                      syncOp->affectedNode()->moveOrigin().has_value() ? syncOp->affectedNode()->moveOrigin().value()
                                                                       : syncOp->affectedNode()->getPath(),
                      syncOp->conflict().type());

            _syncPal->addError(err);

            if (!propagateChange || !exitInfo) {
                return exitInfo;
            }
        }

        if (ExitInfo exitInfo = propagateDeleteToDbAndTree(syncOp); !exitInfo) {
            LOGW_SYNCPAL_WARN(_logger, L"Failed to propagate changes in DB or update tree for: "
                                               << SyncName2WStr(syncOp->affectedNode()->name()));
            return exitInfo;
        }
    } else {
        if (ExitInfo exitInfo = generateDeleteJob(syncOp, ignored, bypassProgressComplete); !exitInfo) {
            return exitInfo;
        }
    }
    return ExitCode::Ok;
}

ExitInfo ExecutorWorker::generateDeleteJob(SyncOpPtr syncOp, bool &ignored, bool &bypassProgressComplete) {
    bypassProgressComplete = false;

    // 1. If omit-flag is False, delete the file or directory on replicaY, because the objects till exists there
    std::shared_ptr<AbstractJob> job = nullptr;
    SyncPath relativeLocalFilePath = syncOp->nodePath(ReplicaSide::Local);
    SyncPath absoluteLocalFilePath = _syncPal->localPath() / relativeLocalFilePath;
    if (syncOp->targetSide() == ReplicaSide::Local) {
        bool isDehydratedPlaceholder = false;
        if (_syncPal->vfsMode() != VirtualFileMode::Off) {
            bool isPlaceholder = false;
            bool isHydrated = false;
            bool isSyncing = false;
            int progress = 0;
            if (!_syncPal->vfsStatus(absoluteLocalFilePath, isPlaceholder, isHydrated, isSyncing, progress)) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in vfsStatus: " << Utility::formatSyncPath(absoluteLocalFilePath));
                return {ExitCode::SystemError, ExitCause::FileAccessError};
            }
            isDehydratedPlaceholder = isPlaceholder && !isHydrated;
        }

        NodeId remoteNodeId = syncOp->affectedNode()->id().has_value() ? syncOp->affectedNode()->id().value() : "";
        if (remoteNodeId.empty()) {
            LOGW_SYNCPAL_WARN(_logger, L"Failed to retrieve node ID");
            return ExitCode::DataError;
        }
        job = std::make_shared<LocalDeleteJob>(_syncPal->syncInfo(), relativeLocalFilePath, isDehydratedPlaceholder,
                                               remoteNodeId);
    } else {
        try {
            job = std::make_shared<DeleteJob>(
                    _syncPal->driveDbId(), syncOp->correspondingNode()->id() ? *syncOp->correspondingNode()->id() : std::string(),
                    syncOp->affectedNode()->id() ? *syncOp->affectedNode()->id() : std::string(), absoluteLocalFilePath);
        } catch (std::exception const &e) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in DeleteJob::DeleteJob for driveDbId=" << _syncPal->driveDbId() << L" : "
                                                                                       << Utility::s2ws(e.what()));
            return ExitCode::DataError;
        }
    }

    // If affected node has both create and delete events (node deleted and re-created with same name), then do not check
    job->setBypassCheck((syncOp->affectedNode()->hasChangeEvent(OperationType::Create) &&
                         syncOp->affectedNode()->hasChangeEvent(OperationType::Delete)) ||
                        syncOp->affectedNode()->isSharedFolder());

    job->setAffectedFilePath(relativeLocalFilePath);
    job->runSynchronously();
    return handleFinishedJob(job, syncOp, relativeLocalFilePath, ignored, bypassProgressComplete);
}

bool ExecutorWorker::isValidDestination(const SyncOpPtr syncOp) {
    if (syncOp->targetSide() == ReplicaSide::Remote && syncOp->type() == OperationType::Create) {
        std::shared_ptr<Node> newCorrespondingParentNode = nullptr;
        if (affectedUpdateTree(syncOp)->rootNode() == syncOp->affectedNode()->parentNode()) {
            newCorrespondingParentNode = targetUpdateTree(syncOp)->rootNode();
        } else {
            newCorrespondingParentNode = correspondingNodeInOtherTree(syncOp->affectedNode()->parentNode());
        }

        if (!newCorrespondingParentNode || !newCorrespondingParentNode->id().has_value()) {
            return false;
        }

        if (newCorrespondingParentNode->isCommonDocumentsFolder() && syncOp->nodeType() != NodeType::Directory) {
            return false;
        }

        if (newCorrespondingParentNode->isSharedFolder()) {
            return false;
        }
    }

    return true;
}

bool ExecutorWorker::enoughLocalSpace(SyncOpPtr syncOp) {
    if (syncOp->targetSide() != ReplicaSide::Local) {
        // This is not a download operation
        return true;
    }

    if (syncOp->affectedNode()->type() != NodeType::File) {
        return true;
    }

    int64_t newSize = syncOp->affectedNode()->size();
    if (syncOp->type() == OperationType::Edit) {
        // Keep only the difference between remote size and local size
        newSize -= syncOp->correspondingNode()->size();
    }

    const int64_t freeBytes = Utility::freeDiskSpace(_syncPal->localPath());
    if (freeBytes >= 0) {
        if (freeBytes < newSize + Utility::freeDiskSpaceLimit()) {
            LOGW_SYNCPAL_WARN(_logger, L"Disk almost full, only " << freeBytes << L"B available at "
                                                                  << Utility::formatSyncPath(_syncPal->localPath())
                                                                  << L". Synchronization canceled.");
            return false;
        }
    } else {
        LOGW_SYNCPAL_WARN(_logger,
                          L"Could not determine free space available at " << Utility::formatSyncPath(_syncPal->localPath()));
    }
    return true;
}

ExitInfo ExecutorWorker::waitForAllJobsToFinish() {
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

        if (ExitInfo exitInfo = deleteFinishedAsyncJobs(); !exitInfo) {
            cancelAllOngoingJobs();
            return exitInfo;
        }

        sendProgress();
    }
    return ExitCode::Ok;
}

ExitInfo ExecutorWorker::deleteFinishedAsyncJobs() {
    ExitInfo exitInfo = ExitCode::Ok;
    while (!_terminatedJobs.empty()) {
        // Delete all terminated jobs
        if (exitInfo && _ongoingJobs.find(_terminatedJobs.front()) != _ongoingJobs.end()) {
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
            SyncPath relativeLocalPath = syncOp->nodePath(ReplicaSide::Local);
            bool ignored = false;
            bool bypassProgressComplete = false;
            exitInfo = handleFinishedJob(job, syncOp, relativeLocalPath, ignored, bypassProgressComplete);
            if (exitInfo) {
                if (!ignored && exitInfo.cause() == ExitCause::OperationCanceled) {
                    setProgressComplete(syncOp, SyncFileStatus::Error);
                    exitInfo = ExitCode::Ok;
                } else {
                    if (ignored) {
                        setProgressComplete(syncOp, SyncFileStatus::Ignored);
                    } else {
                        setProgressComplete(syncOp, SyncFileStatus::Success);
                    }

                    if (syncOp->affectedNode()->id().has_value()) {
                        std::unordered_set<NodeId> whiteList;
                        SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), SyncNodeType::WhiteList, whiteList);
                        if (whiteList.find(syncOp->affectedNode()->id().value()) != whiteList.end()) {
                            // This item has been synchronized, it can now be removed from
                            // white list
                            whiteList.erase(syncOp->affectedNode()->id().value());
                            SyncNodeCache::instance()->update(_syncPal->syncDbId(), SyncNodeType::WhiteList, whiteList);
                        }
                    }
                }
            } else {
                increaseErrorCount(syncOp);
            }

            // Delete job
            _ongoingJobs.erase(_terminatedJobs.front());
        }
        _terminatedJobs.pop();
    }
    return exitInfo;
}

ExitInfo ExecutorWorker::handleManagedBackError(ExitCause jobExitCause, SyncOpPtr syncOp, bool isInconsistencyIssue,
                                                bool downloadImpossible) {
    if (jobExitCause == ExitCause::NotFound && !downloadImpossible) {
        // The operation failed because the destination does not exist anymore
        LOG_SYNCPAL_DEBUG(_logger, "Destination does not exist anymore, restarting sync.");
        return ExitCode::DataError;
    }

    if (jobExitCause == ExitCause::QuotaExceeded) {
        _syncPal->pause();
    } else {
        // The item should be temporarily blacklisted
        _syncPal->blacklistTemporarily(syncOp->affectedNode()->id() ? *syncOp->affectedNode()->id() : std::string(),
                                       syncOp->affectedNode()->getPath(), otherSide(syncOp->targetSide()));
    }

    if (!affectedUpdateTree(syncOp)->deleteNode(syncOp->affectedNode())) {
        LOGW_SYNCPAL_WARN(_logger,
                          L"Error in UpdateTree::deleteNode: node name=" << SyncName2WStr(syncOp->affectedNode()->name()));
        return ExitCode::DataError;
    }

    if (syncOp->correspondingNode()) {
        if (!targetUpdateTree(syncOp)->deleteNode(syncOp->correspondingNode())) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTree::deleteNode: node name="
                                               << SyncName2WStr(syncOp->correspondingNode()->name()));
            return ExitCode::DataError;
        }
    }

    NodeId locaNodeId;
    NodeId remoteNodeId;
    if (syncOp->targetSide() == ReplicaSide::Local) {
        if (syncOp->correspondingNode() && syncOp->correspondingNode()->id()) locaNodeId = *syncOp->correspondingNode()->id();
        remoteNodeId = syncOp->affectedNode()->id() ? *syncOp->affectedNode()->id() : std::string();
    } else {
        locaNodeId = syncOp->affectedNode()->id() ? *syncOp->affectedNode()->id() : std::string();
        if (syncOp->correspondingNode() && syncOp->correspondingNode()->id()) remoteNodeId = *syncOp->correspondingNode()->id();
    }

    Error error;
    if (isInconsistencyIssue) {
        error = Error(_syncPal->syncDbId(), locaNodeId, remoteNodeId, syncOp->affectedNode()->type(),
                      syncOp->affectedNode()->getPath(), ConflictType::None, InconsistencyType::ForbiddenChar);
    } else {
        error = Error(_syncPal->syncDbId(), locaNodeId, remoteNodeId, syncOp->affectedNode()->type(),
                      syncOp->affectedNode()->getPath(), ConflictType::None, InconsistencyType::None, CancelType::None, "",
                      ExitCode::BackError, jobExitCause);
    }
    _syncPal->addError(error);

    return ExitCode::Ok;
}

namespace details {
bool isManagedBackError(ExitCause exitCause) {
    static const std::set<ExitCause> managedExitCauses = {ExitCause::InvalidName,   ExitCause::ApiErr,
                                                          ExitCause::FileTooBig,    ExitCause::NotFound,
                                                          ExitCause::QuotaExceeded, ExitCause::UploadNotTerminated};

    return managedExitCauses.contains(exitCause);
}
} // namespace details

ExitInfo ExecutorWorker::handleFinishedJob(std::shared_ptr<AbstractJob> job, SyncOpPtr syncOp, const SyncPath &relativeLocalPath,
                                           bool &ignored, bool &bypassProgressComplete) {
    ignored = false;
    bypassProgressComplete = false;

    if (job->exitCode() == ExitCode::NeedRestart) {
        cancelAllOngoingJobs();
        _syncPal->setRestart(true);
        return ExitCode::Ok;
    }

    NodeId locaNodeId;
    NodeId remoteNodeId;
    if (syncOp->targetSide() == ReplicaSide::Local) {
        if (syncOp->correspondingNode() && syncOp->correspondingNode()->id()) locaNodeId = *syncOp->correspondingNode()->id();
        if (syncOp->affectedNode()->id()) remoteNodeId = *syncOp->affectedNode()->id();
    } else {
        if (syncOp->affectedNode()->id()) locaNodeId = *syncOp->affectedNode()->id();
        if (syncOp->correspondingNode() && syncOp->correspondingNode()->id()) remoteNodeId = *syncOp->correspondingNode()->id();
    }

    auto networkJob(std::dynamic_pointer_cast<AbstractNetworkJob>(job));
    if (const bool isInconsistencyIssue = job->exitCause() == ExitCause::InvalidName;
        job->exitCode() == ExitCode::BackError && details::isManagedBackError(job->exitCause())) {
        return handleManagedBackError(job->exitCause(), syncOp, isInconsistencyIssue,
                                      networkJob && networkJob->isDownloadImpossible());
    }

    if (job->exitCode() != ExitCode::Ok) {
        if (networkJob && (networkJob->getStatusCode() == Poco::Net::HTTPResponse::HTTP_FORBIDDEN ||
                           networkJob->getStatusCode() == Poco::Net::HTTPResponse::HTTP_CONFLICT)) {
            if (ExitInfo exitInfo = handleForbiddenAction(syncOp, relativeLocalPath, ignored); !exitInfo) {
                LOGW_SYNCPAL_WARN(_logger,
                                  L"Error in handleForbiddenAction for item: " << Utility::formatSyncPath(relativeLocalPath));
                return exitInfo;
            }
        } else if (!handleExecutorError(syncOp, {job->exitCode(), job->exitCause()})) {
            // Cancel all queued jobs
            LOGW_SYNCPAL_WARN(_logger,
                              L"Cancelling jobs. exit code: " << job->exitCode() << L" exit cause: " << job->exitCause());
            cancelAllOngoingJobs();
            return {job->exitCode(), job->exitCause()};

        } else { // The error is managed and the execution can continue.
            LOGW_DEBUG(_logger, L"Error successfully managed: " << job->exitCode() << L" " << job->exitCause() << L" on "
                                                                << syncOp->type() << L" operation for "
                                                                << Utility::formatSyncPath(syncOp->affectedNode()->getPath()));
            return {ExitCode::Ok, ExitCause::OperationCanceled};
        }
    } else {
        // Propagate changes to DB and update trees
        std::shared_ptr<Node> newNode;
        if (ExitInfo exitInfo = propagateChangeToDbAndTree(syncOp, job, newNode); !exitInfo) {
            cancelAllOngoingJobs();
            return exitInfo;
        }

        SyncFileStatus status = SyncFileStatus::Success;
        // Check for conflict or inconsistency
        if (SyncFileItem syncItem; _syncPal->getSyncFileItem(relativeLocalPath, syncItem)) {
            if (syncOp->conflict().type() != ConflictType::None) {
                status = SyncFileStatus::Conflict;
            } else if (syncItem.inconsistency() != InconsistencyType::None) {
                status = SyncFileStatus::Inconsistency;
            }

            if (status != SyncFileStatus::Success) {
                Error err(_syncPal->syncDbId(), syncItem.localNodeId() ? *syncItem.localNodeId() : "",
                          syncItem.remoteNodeId() ? *syncItem.remoteNodeId() : "", syncItem.type(),
                          syncItem.newPath() ? *syncItem.newPath() : syncItem.path(), syncItem.conflict(),
                          syncItem.inconsistency());
                _syncPal->addError(err);
            }
        }

        bypassProgressComplete = syncOp->affectedNode()->hasChangeEvent(OperationType::Create) &&
                                 syncOp->affectedNode()->hasChangeEvent(OperationType::Delete);
    }

    return ExitCode::Ok;
}

ExitInfo ExecutorWorker::handleForbiddenAction(SyncOpPtr syncOp, const SyncPath &relativeLocalPath, bool &ignored) {
    ExitInfo exitInfo = ExitCode::Ok;
    ignored = false;

    const SyncPath absoluteLocalFilePath = _syncPal->localPath() / relativeLocalPath;

    bool removeFromDb = true;
    CancelType cancelType = CancelType::None;
    switch (syncOp->type()) {
        case OperationType::Create: {
            cancelType = CancelType::Create;
            ignored = true;
            if (!PlatformInconsistencyCheckerUtility::renameLocalFile(
                        absoluteLocalFilePath, PlatformInconsistencyCheckerUtility::SuffixType::Blacklisted)) {
                LOGW_SYNCPAL_WARN(_logger, L"PlatformInconsistencyCheckerUtility::renameLocalFile failed for: "
                                                   << Utility::formatSyncPath(absoluteLocalFilePath));
                _syncPal->handleAccessDeniedItem(relativeLocalPath, ExitCause::FileAccessError);
                return ExitCode::Ok;
            }
            removeFromDb = false;
            break;
        }
        case OperationType::Move: {
            // Delete the item from local replica
            const NodeId remoteNodeId =
                    syncOp->correspondingNode()->id().has_value() ? syncOp->correspondingNode()->id().value() : "";
            if (!remoteNodeId.empty()) {
                LocalDeleteJob deleteJob(_syncPal->syncInfo(), relativeLocalPath, isLiteSyncActivated(), remoteNodeId);
                deleteJob.setBypassCheck(true);
                deleteJob.runSynchronously();
            }

            cancelType = CancelType::Move;
            break;
        }
        case OperationType::Edit: {
            // Rename the file so as not to lose any information
            SyncPath newSyncPath;
            PlatformInconsistencyCheckerUtility::renameLocalFile(
                    absoluteLocalFilePath, PlatformInconsistencyCheckerUtility::SuffixType::Conflict, &newSyncPath);

            // Exclude file from sync
            if (!_syncPal->vfsFileStatusChanged(newSyncPath, SyncFileStatus::Ignored)) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in SyncPal::vfsFileStatusChanged: " << Utility::formatSyncPath(newSyncPath));
            }

            cancelType = CancelType::Edit;
            break;
        }
        case OperationType::Delete: {
            cancelType = CancelType::Delete;
            break;
        }
        default: {
            break;
        }
    }

    if (SyncFileItem syncItem; _syncPal->getSyncFileItem(relativeLocalPath, syncItem)) {
        const Error err(
                _syncPal->syncDbId(), syncItem.localNodeId().has_value() ? syncItem.localNodeId().value() : "",
                syncItem.remoteNodeId().has_value() ? syncItem.remoteNodeId().value() : "", syncItem.type(),
                syncOp->affectedNode()->moveOrigin().has_value() ? syncOp->affectedNode()->moveOrigin().value() : syncItem.path(),
                syncItem.conflict(), syncItem.inconsistency(), cancelType,
                syncOp->affectedNode()->moveOrigin().has_value() ? relativeLocalPath : "");
        _syncPal->addError(err);
    }

    if (!exitInfo) return exitInfo;

    if (removeFromDb) {
        //  Remove the node from DB and tree so it will be re-created at its
        //  original location on next sync
        _syncPal->setRestart(true);
        if (exitInfo = propagateDeleteToDbAndTree(syncOp); !exitInfo) {
            LOGW_SYNCPAL_WARN(_logger, L"Failed to propagate changes in DB or update tree for: "
                                               << SyncName2WStr(syncOp->affectedNode()->name()));
        }
    }
    return exitInfo;
}

void ExecutorWorker::sendProgress() {
    std::chrono::duration<double> elapsed_seconds = std::chrono::steady_clock::now() - _fileProgressTimer;
    if (elapsed_seconds.count() > SEND_PROGRESS_DELAY) {
        _fileProgressTimer = std::chrono::steady_clock::now();

        for (const auto &jobInfo: _ongoingJobs) {
            if (!_syncPal->setProgress(jobInfo.second->affectedFilePath(), jobInfo.second->getProgress())) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in SyncPal::setProgress: "
                                                   << Utility::formatSyncPath(jobInfo.second->affectedFilePath()));
            }
        }
    }
}

ExitInfo ExecutorWorker::propagateConflictToDbAndTree(SyncOpPtr syncOp, bool &propagateChange) {
    propagateChange = true;

    switch (syncOp->conflict().type()) {
        case ConflictType::EditEdit: // Edit conflict pattern
        case ConflictType::CreateCreate: // Name clash conflict pattern
        case ConflictType::MoveCreate: // Name clash conflict pattern
        case ConflictType::MoveMoveDest: // Name clash conflict pattern
        case ConflictType::MoveMoveSource: // Name clash conflict pattern
        {
            if (syncOp->conflict().type() != ConflictType::MoveMoveSource) {
                if (const ExitInfo exitInfo = deleteFromDb(syncOp->conflict().localNode()); !exitInfo) {
                    if (exitInfo.code() == ExitCode::DataError) {
                        // The node was not found in DB, this ok since we wanted to remove it anyway
                        LOGW_SYNCPAL_INFO(_logger,
                                          L"Node `" << SyncName2WStr(syncOp->conflict().localNode()->name())
                                                    << L" not found in DB. This is ok since we wanted to remove to anyway.");
                    } else {
                        // Remove local node from DB
                        propagateChange = false;
                        return exitInfo;
                    }
                }
            }
            // Remove node from update tree
            if (!_syncPal->updateTree(ReplicaSide::Local)->deleteNode(syncOp->conflict().localNode())) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTree::deleteNode: node name="
                                                   << SyncName2WStr(syncOp->conflict().localNode()->name()));
            }

            if (!_syncPal->updateTree(ReplicaSide::Remote)->deleteNode(syncOp->conflict().remoteNode())) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTree::deleteNode: node name="
                                                   << SyncName2WStr(syncOp->conflict().remoteNode()->name()));
            }

            propagateChange = false;
            break;
        }
        case ConflictType::EditDelete: // Delete conflict pattern
        {
            if (syncOp->type() == OperationType::Delete) {
                // Just apply normal behavior for delete operations
                break;
            }

            // Do nothing about the move operation since the nodes will be
            // remove from DB anyway
            propagateChange = false;
            break;
        }
        case ConflictType::CreateParentDelete: // Indirect conflict pattern
        {
            // Remove node from update tree
            std::shared_ptr<UpdateTree> updateTree = affectedUpdateTree(syncOp);
            if (!updateTree->deleteNode(syncOp->affectedNode())) {
                LOGW_SYNCPAL_WARN(
                        _logger, L"Error in UpdateTree::deleteNode: node name=" << SyncName2WStr(syncOp->affectedNode()->name()));
            }

            // Do not propagate changes to the DB
            // The created node has been moved and will be discovered as new
            // on next sync
            propagateChange = false;
            break;
        }
        case ConflictType::MoveDelete: // Delete conflict pattern
        case ConflictType::MoveParentDelete: // Indirect conflict pattern
        case ConflictType::MoveMoveCycle: // Name clash conflict pattern
        case ConflictType::None:
        default:
            // Just apply normal behavior
            break;
    }
    return ExitCode::Ok;
}

ExitInfo ExecutorWorker::propagateChangeToDbAndTree(SyncOpPtr syncOp, std::shared_ptr<AbstractJob> job,
                                                    std::shared_ptr<Node> &node) {
    if (syncOp->hasConflict()) {
        bool propagateChange = true;
        ExitInfo exitInfo = propagateConflictToDbAndTree(syncOp, propagateChange);
        if (!propagateChange || !exitInfo) {
            return exitInfo;
        }
    }

    switch (syncOp->type()) {
        case OperationType::Create:
        case OperationType::Edit: {
            NodeId nodeId;
            SyncTime modtime = 0;
            if (syncOp->targetSide() == ReplicaSide::Local) {
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
                    return ExitCode::SystemError;
                }
            }

            if (syncOp->type() == OperationType::Create) {
                return propagateCreateToDbAndTree(syncOp, nodeId, modtime, node);
            } else {
                return propagateEditToDbAndTree(syncOp, nodeId, modtime, node);
            }
        }
        case OperationType::Move: {
            return propagateMoveToDbAndTree(syncOp);
        }
        case OperationType::Delete: {
            return propagateDeleteToDbAndTree(syncOp);
        }
        default: {
            LOGW_SYNCPAL_WARN(_logger, L"Unknown operation type " << syncOp->type() << L" on file "
                                                                  << SyncName2WStr(syncOp->affectedNode()->name()));
            return ExitCode::SystemError;
        }
    }
    return ExitCode::LogicError;
}

ExitInfo ExecutorWorker::propagateCreateToDbAndTree(SyncOpPtr syncOp, const NodeId &newNodeId,
                                                    std::optional<SyncTime> newLastModTime, std::shared_ptr<Node> &node) {
    std::shared_ptr<Node> newCorrespondingParentNode = nullptr;
    if (affectedUpdateTree(syncOp)->rootNode() == syncOp->affectedNode()->parentNode()) {
        newCorrespondingParentNode = targetUpdateTree(syncOp)->rootNode();
    } else {
        newCorrespondingParentNode = correspondingNodeInOtherTree(syncOp->affectedNode()->parentNode());
    }

    if (!newCorrespondingParentNode) {
        LOG_SYNCPAL_WARN(_logger, "Corresponding parent node not found");
        return ExitCode::DataError;
    }

    // 2. Insert a new entry into the database, to avoid that the object is
    // detected again by compute_ops() on the next sync iteration.
    std::string localId = syncOp->targetSide() == ReplicaSide::Local
                                  ? newNodeId
                                  : (syncOp->affectedNode()->id().has_value() ? *syncOp->affectedNode()->id() : "");
    std::string remoteId = syncOp->targetSide() == ReplicaSide::Local
                                   ? (syncOp->affectedNode()->id().has_value() ? *syncOp->affectedNode()->id() : "")
                                   : newNodeId;
    SyncName localName = syncOp->targetSide() == ReplicaSide::Local ? syncOp->newName() : syncOp->nodeName(ReplicaSide::Local);
    SyncName remoteName = syncOp->targetSide() == ReplicaSide::Remote ? syncOp->newName() : syncOp->nodeName(ReplicaSide::Remote);

    if (localId.empty() || remoteId.empty()) {
        LOGW_SYNCPAL_WARN(_logger, L"Empty " << (localId.empty() ? L"local" : L"remote") << L" id for item "
                                             << SyncName2WStr(syncOp->affectedNode()->name()));
        return ExitCode::DataError;
    }

    DbNode dbNode(0, newCorrespondingParentNode->idb(), localName, remoteName, localId, remoteId,
                  syncOp->affectedNode()->createdAt(), newLastModTime, newLastModTime, syncOp->affectedNode()->type(),
                  syncOp->affectedNode()->size(),
                  "" // TODO : change it once we start using content checksum
                  ,
                  syncOp->omit() ? SyncFileStatus::Success : SyncFileStatus::Unknown);

    if (ParametersCache::isExtendedLogEnabled()) {
        LOGW_SYNCPAL_DEBUG(
                _logger, L"Inserting in DB: "
                                 << L" localName=" << SyncName2WStr(localName) << L" / remoteName=" << SyncName2WStr(remoteName)
                                 << L" / localId=" << Utility::s2ws(localId) << L" / remoteId=" << Utility::s2ws(remoteId)
                                 << L" / parent DB ID="
                                 << (newCorrespondingParentNode->idb().has_value() ? newCorrespondingParentNode->idb().value()
                                                                                   : -1)
                                 << L" / createdAt="
                                 << (syncOp->affectedNode()->createdAt().has_value() ? *syncOp->affectedNode()->createdAt() : -1)
                                 << L" / lastModTime=" << (newLastModTime.has_value() ? *newLastModTime : -1) << L" / type="
                                 << syncOp->affectedNode()->type());
    }

    if (dbNode.nameLocal().empty() || dbNode.nameRemote().empty() || !dbNode.nodeIdLocal().has_value() ||
        !dbNode.nodeIdRemote().has_value() || !dbNode.created().has_value() || !dbNode.lastModifiedLocal().has_value() ||
        !dbNode.lastModifiedRemote().has_value() || dbNode.type() == NodeType::Unknown) {
        LOG_SYNCPAL_ERROR(_logger, "Error inserting new node in DB!!!");
        assert(false);
    }

    DbNodeId newDbNodeId;
    bool constraintError = false;
    if (!_syncPal->_syncDb->insertNode(dbNode, newDbNodeId, constraintError)) {
        LOGW_SYNCPAL_WARN(
                _logger,
                L"Failed to insert node into DB:"
                        << L" local ID: " << Utility::s2ws(localId) << L", remote ID: " << Utility::s2ws(remoteId)
                        << L", local name: " << SyncName2WStr(localName) << L", remote name: " << SyncName2WStr(remoteName)
                        << L", parent DB ID: "
                        << (newCorrespondingParentNode->idb().has_value() ? newCorrespondingParentNode->idb().value() : -1));

        if (!constraintError) {
            return {ExitCode::DbError, ExitCause::DbAccessError};
        }

        // Manage DELETE events not reported by the folder watcher
        // Some apps save files with DELETE + CREATE operations, but sometimes,
        // the DELETE operation is not reported
        // => The local snapshot will contain 2 nodes with the same remote id
        // => A unique constraint error on the remote node id will occur when
        // inserting the new node in DB
        DbNodeId dbNodeId;
        bool found = false;
        if (!_syncPal->_syncDb->dbId(ReplicaSide::Remote, remoteId, dbNodeId, found)) {
            LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbId");
            return {ExitCode::DbError, ExitCause::DbAccessError};
        }
        if (found) {
            // Delete old node
            if (!_syncPal->_syncDb->deleteNode(dbNodeId, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::deleteNode");
                return {ExitCode::DbError, ExitCause::DbAccessError};
            }
        }

        // Create new node
        if (!_syncPal->_syncDb->insertNode(dbNode, newDbNodeId, constraintError)) {
            LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::insertNode");
            return {constraintError ? ExitCode::DataError : ExitCode::DbError, ExitCause::DbAccessError};
        }

        // The snapshot must be invalidated before the next sync
        _snapshotToInvalidate = true;
    }

    // 3. Update the update tree structures to ensure that follow-up operations
    // can execute correctly, as they are based on the information in these
    // structures.
    if (syncOp->omit()) {
        // Update existing nodes
        syncOp->affectedNode()->setIdb(newDbNodeId);
        syncOp->correspondingNode()->setIdb(newDbNodeId);
        node = syncOp->correspondingNode();
    } else {
        // insert new node
        node = std::shared_ptr<Node>(
                new Node(newDbNodeId, syncOp->targetSide() == ReplicaSide::Local ? ReplicaSide::Local : ReplicaSide::Remote,
                         remoteName, syncOp->affectedNode()->type(), OperationType::None, newNodeId, newLastModTime,
                         newLastModTime, syncOp->affectedNode()->size(), newCorrespondingParentNode));
        if (node == nullptr) {
            std::cout << "Failed to allocate memory" << std::endl;
            LOG_SYNCPAL_ERROR(_logger, "Failed to allocate memory");
            return {ExitCode::SystemError, ExitCause::NotEnoughtMemory};
        }

        std::shared_ptr<UpdateTree> updateTree = targetUpdateTree(syncOp);
        updateTree->insertNode(node);

        if (!newCorrespondingParentNode->insertChildren(node)) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in Node::insertChildren: node name="
                                               << SyncName2WStr(node->name()) << L" parent node name="
                                               << SyncName2WStr(newCorrespondingParentNode->name()));
            return ExitCode::DataError;
        }

        // Affected node does not have a valid DB ID yet, update it
        syncOp->affectedNode()->setIdb(newDbNodeId);
    }

    return ExitCode::Ok;
}

ExitInfo ExecutorWorker::propagateEditToDbAndTree(SyncOpPtr syncOp, const NodeId &newNodeId,
                                                  std::optional<SyncTime> newLastModTime, std::shared_ptr<Node> &node) {
    DbNode dbNode;
    bool found = false;
    if (!_syncPal->_syncDb->node(*syncOp->correspondingNode()->idb(), dbNode, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::node");
        return {ExitCode::DbError, ExitCause::DbAccessError};
    }
    if (!found) {
        LOG_SYNCPAL_DEBUG(_logger, "Failed to retrieve node for dbId=" << *syncOp->correspondingNode()->idb());
        return {ExitCode::DataError, ExitCause::DbEntryNotFound};
    }

    // 2. Update the database entry, to avoid detecting the edit operation again.
    std::string localId = syncOp->targetSide() == ReplicaSide::Local ? newNodeId
                          : syncOp->affectedNode()->id().has_value() ? *syncOp->affectedNode()->id()
                                                                     : std::string();
    std::string remoteId = syncOp->targetSide() == ReplicaSide::Local
                                   ? syncOp->affectedNode()->id().has_value() ? *syncOp->affectedNode()->id() : std::string()
                                   : newNodeId;
    const SyncName localName = syncOp->nodeName(ReplicaSide::Local);
    const SyncName remoteName = syncOp->nodeName(ReplicaSide::Remote);

    if (localId.empty() || remoteId.empty()) {
        LOGW_SYNCPAL_WARN(_logger, L"Empty " << (localId.empty() ? L"local" : L"remote") << L" id for item "
                                             << SyncName2WStr(syncOp->affectedNode()->name()));
        return ExitCode::DataError;
    }

    dbNode.setNodeIdLocal(localId);
    dbNode.setNodeIdRemote(remoteId);
    dbNode.setLastModifiedLocal(newLastModTime);
    dbNode.setLastModifiedRemote(newLastModTime);
    dbNode.setSize(syncOp->affectedNode()->size());
    dbNode.setChecksum(""); // TODO : change it once we start using content checksum
    if (syncOp->omit()) {
        dbNode.setStatus(SyncFileStatus::Success);
    }

    if (ParametersCache::isExtendedLogEnabled()) {
        LOGW_SYNCPAL_DEBUG(
                _logger,
                L"Updating DB: " << L" / localName=" << SyncName2WStr(localName) << L" / remoteName=" << SyncName2WStr(remoteName)
                                 << L" / localId=" << Utility::s2ws(localId) << L" / remoteId=" << Utility::s2ws(remoteId)
                                 << L" / parent DB ID="
                                 << (dbNode.parentNodeId().has_value() ? dbNode.parentNodeId().value() : -1) << L" / createdAt="
                                 << (syncOp->affectedNode()->createdAt().has_value() ? *syncOp->affectedNode()->createdAt() : -1)
                                 << L" / lastModTime=" << (newLastModTime.has_value() ? *newLastModTime : -1) << L" / type="
                                 << syncOp->affectedNode()->type());
    }

    if (!_syncPal->_syncDb->updateNode(dbNode, found)) {
        LOGW_SYNCPAL_WARN(_logger, L"Failed to update node into DB: "
                                           << L"local ID: " << Utility::s2ws(localId) << L", remote ID: "
                                           << Utility::s2ws(remoteId) << L", local name: " << SyncName2WStr(localName)
                                           << L", remote name: " << SyncName2WStr(remoteName) << L", parent DB ID: "
                                           << (dbNode.parentNodeId().has_value() ? dbNode.parentNodeId().value() : -1));
        return {ExitCode::DbError, ExitCause::DbAccessError};
    }
    if (!found) {
        return {ExitCode::DataError, ExitCause::DbEntryNotFound};
    }

    // 3. If the omit flag is False, update the updatetreeY structure to ensure
    // that follow-up operations can execute correctly, as they are based on the
    // information in this structure
    if (!syncOp->omit()) {
        syncOp->correspondingNode()->setId(syncOp->targetSide() == ReplicaSide::Local ? localId
                                                                                      : remoteId); // ID might have changed in the
                                                                                                   // case of a delete+create
        syncOp->correspondingNode()->setLastModified(newLastModTime);
    }
    node = syncOp->correspondingNode();

    return ExitCode::Ok;
}

ExitInfo ExecutorWorker::propagateMoveToDbAndTree(SyncOpPtr syncOp) {
    std::shared_ptr<Node> correspondingNode =
            syncOp->correspondingNode() ? syncOp->correspondingNode() : syncOp->affectedNode(); // No corresponding node => rename

    if (!correspondingNode || !correspondingNode->idb().has_value()) {
        LOG_SYNCPAL_WARN(_logger, "Invalid corresponding node");
        return ExitCode::DataError;
    }

    DbNode dbNode;
    bool found = false;
    if (!_syncPal->_syncDb->node(*correspondingNode->idb(), dbNode, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::node");
        return {ExitCode::DbError, ExitCause::DbAccessError};
    }
    if (!found) {
        LOG_SYNCPAL_DEBUG(_logger, "Failed to retrieve node for dbId=" << *correspondingNode->idb());
        return {ExitCode::DataError, ExitCause::DbEntryNotFound};
    }

    // 2. Update the database entry, to avoid detecting the move operation again.
    std::shared_ptr<Node> parentNode =
            syncOp->newParentNode()
                    ? syncOp->newParentNode()
                    : (syncOp->correspondingNode() ? correspondingNodeInOtherTree(syncOp->affectedNode()->parentNode())
                                                   : correspondingNode->parentNode());
    if (!parentNode) {
        LOGW_SYNCPAL_DEBUG(_logger,
                           L"Failed to get corresponding parent node: " << SyncName2WStr(syncOp->affectedNode()->name()));
        return ExitCode::DataError;
    }

    std::string localId = syncOp->targetSide() == ReplicaSide::Local
                                  ? correspondingNode->id().has_value() ? *correspondingNode->id() : std::string()
                          : syncOp->affectedNode()->id().has_value() ? *syncOp->affectedNode()->id()
                                                                     : std::string();
    std::string remoteId = syncOp->targetSide() == ReplicaSide::Local
                                   ? syncOp->affectedNode()->id().has_value() ? *syncOp->affectedNode()->id() : std::string()
                           : correspondingNode->id().has_value() ? *correspondingNode->id()
                                                                 : std::string();
    SyncName localName = syncOp->targetSide() == ReplicaSide::Local ? syncOp->newName() : syncOp->nodeName(ReplicaSide::Local);
    SyncName remoteName = syncOp->targetSide() == ReplicaSide::Remote ? syncOp->newName() : syncOp->nodeName(ReplicaSide::Remote);

    if (localId.empty() || remoteId.empty()) {
        LOGW_SYNCPAL_WARN(_logger, L"Empty " << (localId.empty() ? L"local" : L"remote") << L" id for item "
                                             << SyncName2WStr(syncOp->affectedNode()->name()));
        return ExitCode::DataError;
    }

    dbNode.setParentNodeId(parentNode->idb());
    dbNode.setNameLocal(localName);
    dbNode.setNameRemote(remoteName);
    if (syncOp->omit()) {
        dbNode.setStatus(SyncFileStatus::Success);
    }

    if (ParametersCache::isExtendedLogEnabled()) {
        LOGW_SYNCPAL_DEBUG(
                _logger,
                L"Updating DB: " << L" localName=" << SyncName2WStr(localName) << L" / remoteName=" << SyncName2WStr(remoteName)
                                 << L" / localId=" << Utility::s2ws(localId) << L" / remoteId=" << Utility::s2ws(remoteId)
                                 << L" / parent DB ID="
                                 << (dbNode.parentNodeId().has_value() ? dbNode.parentNodeId().value() : -1) << L" / createdAt="
                                 << (syncOp->affectedNode()->createdAt().has_value() ? *syncOp->affectedNode()->createdAt() : -1)
                                 << L" / lastModTime="
                                 << (syncOp->affectedNode()->lastmodified().has_value() ? *syncOp->affectedNode()->lastmodified()
                                                                                        : -1)
                                 << L" / type=" << syncOp->affectedNode()->type());
    }

    if (!_syncPal->_syncDb->updateNode(dbNode, found)) {
        LOGW_SYNCPAL_WARN(_logger, L"Failed to update node into DB: "
                                           << L"local ID: " << Utility::s2ws(localId) << L", remote ID: "
                                           << Utility::s2ws(remoteId) << L", local name: " << SyncName2WStr(localName)
                                           << L", remote name: " << SyncName2WStr(remoteName) << L", parent DB ID: "
                                           << (dbNode.parentNodeId().has_value() ? dbNode.parentNodeId().value() : -1));
        return {ExitCode::DbError, ExitCause::DbAccessError};
    }
    if (!found) {
        return {ExitCode::DataError, ExitCause::DbEntryNotFound};
    }

    // 3. If the omit flag is False, update the updatetreeY structure to ensure
    // that follow-up operations can execute correctly, as they are based on the
    // information in this structure.
    if (!syncOp->omit()) {
        auto prevParent = correspondingNode->parentNode();
        prevParent->deleteChildren(correspondingNode);

        correspondingNode->setName(remoteName);

        if (!correspondingNode->setParentNode(parentNode)) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in Node::setParentNode: node name=" << SyncName2WStr(parentNode->name())
                                                                                   << L" parent node name="
                                                                                   << SyncName2WStr(correspondingNode->name()));
            return ExitCode::DataError;
        }

        if (!correspondingNode->parentNode()->insertChildren(correspondingNode)) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in Node::insertChildren: node name="
                                               << SyncName2WStr(correspondingNode->name()) << L" parent node name="
                                               << SyncName2WStr(correspondingNode->parentNode()->name()));
            return ExitCode::DataError;
        }
    }

    return ExitCode::Ok;
}

ExitInfo ExecutorWorker::propagateDeleteToDbAndTree(SyncOpPtr syncOp) {
    // avoids that the object(s) are detected again by compute_ops() on the next
    // sync iteration
    if (ExitInfo exitInfo = deleteFromDb(syncOp->affectedNode()); !exitInfo) {
        return exitInfo;
    }

    // 3. Remove nX and nY from the update tree structures.
    if (!affectedUpdateTree(syncOp)->deleteNode(syncOp->affectedNode())) {
        LOGW_SYNCPAL_WARN(_logger,
                          L"Error in UpdateTree::deleteNode: node name=" << SyncName2WStr(syncOp->affectedNode()->name()));
        return ExitCode::DataError;
    }

    if (!targetUpdateTree(syncOp)->deleteNode(syncOp->correspondingNode())) {
        LOGW_SYNCPAL_WARN(_logger,
                          L"Error in UpdateTree::deleteNode: node name=" << SyncName2WStr(syncOp->correspondingNode()->name()));
        return ExitCode::DataError;
    }

    return ExitCode::Ok;
}

ExitInfo ExecutorWorker::deleteFromDb(std::shared_ptr<Node> node) {
    if (!node->idb().has_value()) {
        LOGW_SYNCPAL_WARN(_logger, L"Node " << SyncName2WStr(node->name()) << L" does not have a DB ID");
        return {ExitCode::DataError, ExitCause::DbEntryNotFound};
    }

    // Remove item (and children by cascade) from DB
    bool found = false;
    if (!_syncPal->_syncDb->deleteNode(*node->idb(), found)) {
        LOG_SYNCPAL_WARN(_logger, "Failed to remove node " << *node->idb() << " from DB");
        return {ExitCode::DbError, ExitCause::DbAccessError};
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "Node DB ID " << *node->idb() << " not found");
        return {ExitCode::DataError, ExitCause::DbEntryNotFound};
    }

    if (ParametersCache::isExtendedLogEnabled()) {
        LOGW_SYNCPAL_DEBUG(_logger, L"Item \"" << SyncName2WStr(node->name()) << L"\" removed from DB");
    }

    return ExitCode::Ok;
}

ExitInfo ExecutorWorker::runCreateDirJob(SyncOpPtr syncOp, std::shared_ptr<AbstractJob> job) {
    job->runSynchronously();

    std::string errorCode;
    auto tokenJob(std::dynamic_pointer_cast<AbstractTokenNetworkJob>(job));
    if (tokenJob && tokenJob->hasErrorApi(&errorCode)) {
        const auto code = getNetworkErrorCode(errorCode);
        if (code == NetworkErrorCode::destinationAlreadyExists) {
            // Folder is already there, ignore this error
        } else if (code == NetworkErrorCode::forbiddenError) {
            // The item should be blacklisted
            _syncPal->blacklistTemporarily(
                    syncOp->affectedNode()->id().has_value() ? syncOp->affectedNode()->id().value() : std::string(),
                    syncOp->affectedNode()->getPath(), ReplicaSide::Local);
            Error error(_syncPal->syncDbId(),
                        syncOp->affectedNode()->id().has_value() ? syncOp->affectedNode()->id().value() : std::string(), "",
                        syncOp->affectedNode()->type(), syncOp->affectedNode()->getPath(), ConflictType::None,
                        InconsistencyType::None, CancelType::None, "", ExitCode::BackError, ExitCause::HttpErrForbidden);
            _syncPal->addError(error);

            if (!affectedUpdateTree(syncOp)->deleteNode(syncOp->affectedNode())) {
                LOGW_SYNCPAL_WARN(
                        _logger, L"Error in UpdateTree::deleteNode: node name=" << SyncName2WStr(syncOp->affectedNode()->name()));
                return ExitCode::DataError;
            }

            if (syncOp->correspondingNode()) {
                if (!targetUpdateTree(syncOp)->deleteNode(syncOp->correspondingNode())) {
                    LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTree::deleteNode: node name="
                                                       << SyncName2WStr(syncOp->correspondingNode()->name()));
                    return ExitCode::DataError;
                }
            }
            return ExitCode::Ok;
        }
    }

    if (job->exitCode() != ExitCode::Ok) {
        LOGW_SYNCPAL_WARN(_logger, L"Failed to create directory: " << SyncName2WStr(syncOp->affectedNode()->name()));
        return {job->exitCode(), job->exitCause()};
    }

    NodeId newNodeId;
    SyncTime newModTime = 0;

    if (syncOp->targetSide() == ReplicaSide::Local) {
        auto castJob(std::dynamic_pointer_cast<LocalCreateDirJob>(job));
        newNodeId = castJob->nodeId();
        newModTime = castJob->modtime();
    } else {
        auto castJob(std::dynamic_pointer_cast<CreateDirJob>(job));
        newNodeId = castJob->nodeId();
        newModTime = castJob->modtime();
    }

    if (newNodeId.empty()) {
        LOGW_SYNCPAL_WARN(_logger, L"Failed to retreive ID for directory: " << SyncName2WStr(syncOp->affectedNode()->name()));
        return {ExitCode::DataError, ExitCause::ApiErr};
    }

    std::shared_ptr<Node> newNode = nullptr;
    if (ExitInfo exitInfo = propagateCreateToDbAndTree(syncOp, newNodeId, newModTime, newNode); !exitInfo) {
        LOGW_SYNCPAL_WARN(_logger, L"Failed to propagate changes in DB or update tree for: "
                                           << SyncName2WStr(syncOp->affectedNode()->name()));
        return exitInfo;
    }

    return ExitCode::Ok;
}

void ExecutorWorker::cancelAllOngoingJobs(bool reschedule /*= false*/) {
    LOG_SYNCPAL_DEBUG(_logger, "Cancelling all queued executor jobs");

    // First, abort all jobs that are not running yet to avoid starting them for
    // nothing
    std::list<std::shared_ptr<AbstractJob>> remainingJobs;
    for (const auto &job: _ongoingJobs) {
        if (!job.second->isRunning()) {
            LOG_SYNCPAL_DEBUG(_logger, "Cancelling job: " << job.second->jobId());
            job.second->setAdditionalCallback(nullptr);
            job.second->abort();
            if (reschedule) {
                _opList.push_front(_jobToSyncOpMap[job.first]->id());
            }
        } else {
            remainingJobs.push_back(job.second);
        }
    }

    // Then cancel jobs that are currently running
    for (const auto &job: remainingJobs) {
        LOG_SYNCPAL_DEBUG(_logger, "Cancelling job: " << job->jobId());
        job->setAdditionalCallback(nullptr);
        job->abort();

        if (reschedule) {
            _opList.push_front(_jobToSyncOpMap[job->jobId()]->id());
        }
    }
    _ongoingJobs.clear();
    if (!reschedule) {
        _opList.clear();
    }

    LOG_SYNCPAL_DEBUG(_logger, "All queued executor jobs cancelled.");
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

        if (!affectedUpdateTree(syncOp)->deleteNode(syncOp->affectedNode())) {
            LOGW_SYNCPAL_WARN(_logger,
                              L"Error in UpdateTree::deleteNode: node name=" << SyncName2WStr(syncOp->affectedNode()->name()));
        }

        if (syncOp->correspondingNode() && syncOp->correspondingNode()->id().has_value()) {
            if (!targetUpdateTree(syncOp)->deleteNode(syncOp->correspondingNode())) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTree::deleteNode: node name="
                                                   << SyncName2WStr(syncOp->correspondingNode()->name()));
            }
        }
    }
}

ExitInfo ExecutorWorker::getFileSize(const SyncPath &path, uint64_t &size) {
    IoError ioError = IoError::Unknown;
    if (!IoHelper::getFileSize(path, size, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::getFileSize for " << Utility::formatIoError(path, ioError));
        return ExitCode::SystemError;
    }

    if (ioError == IoError::NoSuchFileOrDirectory) { // The synchronization will
                                                     // be re-started.
        LOGW_WARN(_logger, L"File doesn't exist: " << Utility::formatSyncPath(path));
        return ExitCode::DataError;
    }

    if (ioError == IoError::AccessDenied) { // An action from the user is requested.
        LOGW_WARN(_logger, L"File search permission missing: " << Utility::formatSyncPath(path));
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    assert(ioError == IoError::Success);
    if (ioError != IoError::Success) {
        LOGW_WARN(_logger, L"Unable to read file size for " << Utility::formatSyncPath(path));
        return ExitCode::SystemError;
    }

    return ExitCode::Ok;
}

ExitInfo ExecutorWorker::handleExecutorError(SyncOpPtr syncOp, ExitInfo opsExitInfo) {
    assert((syncOp && !opsExitInfo) && "syncOp is nullptr in ExecutorWorker::handleExecutorError");
    if (!syncOp) {
        LOG_WARN(_logger, "syncOp is nullptr in ExecutorWorker::handleExecutorError");
        return ExitCode::DataError;
    }
    if (opsExitInfo) {
        return opsExitInfo;
    }

    // Handle specific errors
    switch (static_cast<int>(opsExitInfo)) {
        case static_cast<int>(ExitInfo(ExitCode::SystemError, ExitCause::FileAccessError)):
        case static_cast<int>(ExitInfo(ExitCode::SystemError, ExitCause::MoveToTrashFailed)): {
            return handleOpsFileAccessError(syncOp, opsExitInfo);
        }
        case static_cast<int>(ExitInfo(ExitCode::SystemError, ExitCause::NotFound)): {
            return handleOpsFileNotFound(syncOp, opsExitInfo);
        }
        case static_cast<int>(ExitInfo(ExitCode::BackError, ExitCause::FileAlreadyExist)):
        case static_cast<int>(ExitInfo(ExitCode::DataError, ExitCause::FileAlreadyExist)): {
            return handleOpsAlreadyExistError(syncOp, opsExitInfo);
        }
        default: {
            break;
        }
    };
    LOG_WARN(_logger,
             "Unhandled error in ExecutorWorker::handleExecutorError: " << opsExitInfo.code() << " " << opsExitInfo.cause());
    return opsExitInfo;
}

ExitInfo ExecutorWorker::handleOpsFileAccessError(SyncOpPtr syncOp, ExitInfo opsExitInfo) {
    if (syncOp->targetSide() == ReplicaSide::Local && syncOp->type() == OperationType::Create) {
        // The item does not exist yet locally, we will only tmpBlacklist the remote item
        if (ExitInfo exitInfo = _syncPal->handleAccessDeniedItem(syncOp->affectedNode()->getPath()); !exitInfo) {
            return exitInfo;
        }
    } else {
        // Both local and remote item will be temporarily blacklisted
        auto localNode = syncOp->targetSide() == ReplicaSide::Remote ? syncOp->affectedNode() : syncOp->correspondingNode();
        if (!localNode) return ExitCode::LogicError;
        SyncPath relativeLocalFilePath = localNode->getPath();
        NodeId localNodeId = localNode->id().has_value() ? *localNode->id() : NodeId();
        if (ExitInfo exitInfo = _syncPal->handleAccessDeniedItem(relativeLocalFilePath, opsExitInfo.cause()); !exitInfo) {
            return exitInfo;
        }
    }
    _syncPal->setRestart(true);
    return removeDependentOps(syncOp);
}

ExitInfo ExecutorWorker::handleOpsFileNotFound(SyncOpPtr syncOp, ExitInfo opsExitInfo) {
    if (syncOp->targetSide() != ReplicaSide::Remote) {
        LOGW_SYNCPAL_WARN(_logger, L"Unhandled target side for " << opsExitInfo << L": " << syncOp->targetSide());
        return opsExitInfo; // Unable to handle this error
    }

    _syncPal->setRestart(true);
    return removeDependentOps(syncOp);
}

ExitInfo ExecutorWorker::handleOpsAlreadyExistError(SyncOpPtr syncOp, ExitInfo opsExitInfo) {
    // If the file/directory already exist either on local or remote side, we blacklist it localy and the remote
    // verson will be downloaded again.

    if (syncOp->type() != OperationType::Create &&
        syncOp->type() != OperationType::Move) { // The above handling is only for create and move/rename operations.
        return opsExitInfo;
    }

    SyncPath relativeLocalPath;
    SyncPath relativeRemotePath;
    if (syncOp->targetSide() == ReplicaSide::Local) {
        relativeRemotePath = syncOp->affectedNode()->getPath();
        // In case of a create the corresponding node is not set, remote and local relative path are the same
        relativeLocalPath = syncOp->correspondingNode() ? syncOp->correspondingNode()->parentNode()->getPath() / syncOp->newName()
                                                        : relativeRemotePath;

        if (_syncPal->isTmpBlacklisted(relativeLocalPath, ReplicaSide::Local)) {
            LOGW_SYNCPAL_DEBUG(_logger, Utility::formatSyncPath(relativeLocalPath)
                                                << L" is already blacklisted locally, blacklisting remote corresponding node.");
            _syncPal->blacklistTemporarily(
                    syncOp->affectedNode()->id().has_value() ? *syncOp->affectedNode()->id() : std::string(), relativeLocalPath,
                    ReplicaSide::Remote);
            if (!affectedUpdateTree(syncOp)->deleteNode(syncOp->affectedNode())) {
                LOGW_SYNCPAL_WARN(
                        _logger, L"Error in UpdateTree::deleteNode: node name=" << SyncName2WStr(syncOp->affectedNode()->name()));
                return ExitCode::DataError;
            }

            if (syncOp->correspondingNode() && !targetUpdateTree(syncOp)->deleteNode(syncOp->correspondingNode())) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTree::deleteNode: node name="
                                                   << SyncName2WStr(syncOp->correspondingNode()->name()));
                return ExitCode::DataError;
            }
            _syncPal->setRestart(true);
            return removeDependentOps(syncOp);
        }
    } else if (syncOp->targetSide() == ReplicaSide::Remote) {
        relativeLocalPath = syncOp->affectedNode()->parentNode()->getPath() / syncOp->newName();
    } else {
        return ExitCode::LogicError;
    }

    SyncPath absoluteLocalPath = _syncPal->localPath() / relativeLocalPath;
    LOGW_SYNCPAL_WARN(_logger, L"Item: " << Utility::formatSyncPath(absoluteLocalPath)
                                         << L" already exist. Blacklisting it on "
                                            L"local replica.");

    if (ExitInfo exitInfo = PlatformInconsistencyCheckerUtility::renameLocalFile(
                absoluteLocalPath, PlatformInconsistencyCheckerUtility::SuffixType::Blacklisted);
        !exitInfo) {
        LOGW_WARN(_logger, L"Failed to blacklist file: " << Utility::formatSyncPath(absoluteLocalPath) << L" ExitCode::"
                                                         << exitInfo.code() << L" ExitCause::" << exitInfo.cause());
        return ExitCode::DataError; // The synchronization will be re-started.
    }


    _syncPal->setRestart(true);
    if (ExitInfo exitInfo = propagateDeleteToDbAndTree(syncOp); !exitInfo) {
        LOGW_SYNCPAL_WARN(_logger, L"Failed to propagate changes in DB or update tree for: "
                                           << SyncName2WStr(syncOp->affectedNode()->name()));
        return exitInfo;
    }
    return removeDependentOps(syncOp);
}

ExitInfo ExecutorWorker::removeDependentOps(SyncOpPtr syncOp) {
    auto localNode = syncOp->affectedNode()->side() == ReplicaSide::Local ? syncOp->affectedNode() : syncOp->correspondingNode();
    auto remoteNode =
            syncOp->affectedNode()->side() == ReplicaSide::Remote ? syncOp->affectedNode() : syncOp->correspondingNode();

    std::list<UniqueId> dependentOps;
    for (const auto &opId: _opList) {
        SyncOpPtr syncOp2 = _syncPal->_syncOps->getOp(opId);
        if (!syncOp2) {
            LOGW_SYNCPAL_WARN(_logger, L"Operation doesn't exist anymore: id=" << opId);
            continue;
        }
        auto localNode2 =
                syncOp2->affectedNode()->side() == ReplicaSide::Local ? syncOp2->affectedNode() : syncOp2->correspondingNode();
        auto remoteNode2 =
                syncOp2->affectedNode()->side() == ReplicaSide::Remote ? syncOp2->affectedNode() : syncOp2->correspondingNode();
        SyncName nodeName = localNode2 ? localNode2->name() : SyncName();
        if (nodeName.empty()) nodeName = remoteNode2 ? remoteNode2->name() : SyncName();

        if (localNode && localNode2 && (localNode->isParentOf(localNode2))) {
            LOGW_SYNCPAL_DEBUG(_logger, L"Removing " << syncOp2->type() << L" operation on " << SyncName2WStr(nodeName)
                                                     << L" because it depends on " << syncOp->type() << L" operation on "
                                                     << SyncName2WStr(localNode->name()) << L" wich failed.");
            dependentOps.push_back(opId);
            continue;
        }

        if (remoteNode && remoteNode2 && (remoteNode->isParentOf(remoteNode2))) {
            LOGW_SYNCPAL_DEBUG(_logger, L"Removing " << syncOp2->type() << L" operation on " << SyncName2WStr(nodeName)
                                                     << L" because it depends on " << syncOp->type() << L" operation on "
                                                     << SyncName2WStr(remoteNode->name()) << L"wich failed.");
            dependentOps.push_back(opId);
        }
    }

    for (const auto &opId: dependentOps) {
        _opList.remove(opId);
    }
    for (const auto &opId: dependentOps) {
        removeDependentOps(_syncPal->_syncOps->getOp(opId));
    }
    if (!dependentOps.empty()) {
        LOGW_SYNCPAL_DEBUG(_logger, L"Removed " << dependentOps.size() << L" dependent operations.");
    }

    return ExitCode::Ok;
}

} // namespace KDC
