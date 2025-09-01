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

#include "executorworker.h"

#include "filerescuer.h"
#include "jobs/local/localcreatedirjob.h"
#include "jobs/local/localdeletejob.h"
#include "jobs/local/localmovejob.h"
#include "jobs/network/kDrive_API/createdirjob.h"
#include "jobs/network/kDrive_API/deletejob.h"
#include "jobs/network/kDrive_API/downloadjob.h"
#include "jobs/network/kDrive_API/movejob.h"
#include "jobs/network/kDrive_API/renamejob.h"
#include "jobs/network/kDrive_API/getfilelistjob.h"
#include "reconciliation/platform_inconsistency_checker/platforminconsistencycheckerutility.h"
#include "update_detection/file_system_observer/filesystemobserverworker.h"
#include "update_detection/update_detector/updatetree.h"
#include "jobs/jobmanager.h"
#include "jobs/network/kDrive_API/upload/uploadjob.h"
#include "jobs/network/kDrive_API/upload/upload_session/driveuploadsession.h"
#include "libcommon/log/sentry/ptraces.h"
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
    OperationProcessor(syncPal, name, shortName, false) {}

void ExecutorWorker::executorCallback(UniqueId jobId) {
    _terminatedJobs.push(jobId);
}

void ExecutorWorker::removeSyncNodeFromWhitelistIfSynced(const NodeId &nodeId) {
    if (SyncNodeCache::instance()->contains(_syncPal->syncDbId(), SyncNodeType::WhiteList, nodeId)) {
        // This item has been synchronized, it can now be removed from white list
        (void) SyncNodeCache::instance()->deleteSyncNode(_syncPal->syncDbId(), nodeId);
    }
}

void ExecutorWorker::execute() {
    ExitInfo executorExitInfo = ExitCode::Ok;
    _snapshotToInvalidate = false;

    _jobToSyncOpMap.clear();

    LOG_SYNCPAL_DEBUG(_logger, "Worker started: name=" << name());

    // Keep a copy of the sorted list
    _opList = _syncPal->_syncOps->opSortedList();
    initProgressManager();
    uint64_t changesCounter = 0;
    while (!_opList.empty()) { // Same loop twice because we might reschedule the jobs after a pause TODO : refactor double loop
        // Create all the jobs
        sentry::pTraces::scoped::JobGeneration perfMonitor(syncDbId());
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

            UniqueId opId = 0;
            _opListMutex.lock();
            if (!_opList.empty()) {
                opId = _opList.front();
                _opList.pop_front();
            }
            _opListMutex.unlock();

            if (!opId) break;

            SyncOpPtr syncOp = _syncPal->_syncOps->getOp(opId);

            if (!syncOp) {
                LOG_SYNCPAL_WARN(_logger, "Operation doesn't exist anymore: id=" << opId);
                continue;
            }

            changesCounter++;

            std::shared_ptr<AbstractJob> job = nullptr;
            bool ignored = false;
            bool bypassProgressComplete = false;
            bool hydrating = false;
            switch (syncOp->type()) {
                case OperationType::Create: {
                    executorExitInfo = handleCreateOp(syncOp, job, ignored, hydrating);
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
                    LOGW_SYNCPAL_WARN(_logger, L"Unknown operation type: "
                                                       << syncOp->type() << L" on file "
                                                       << Utility::formatSyncName(syncOp->affectedNode()->name()));
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
                    increaseErrorCount(syncOp, executorExitInfo);
                    cancelAllOngoingJobs();
                    break;
                } else { // If the error is handled, continue the execution
                    if (!bypassProgressComplete) setProgressComplete(syncOp, SyncFileStatus::Error);
                    continue;
                }
                if (!bypassProgressComplete) setProgressComplete(syncOp, SyncFileStatus::Error);
            }

            if (job) {
                std::function<void(UniqueId)> callback =
                        std::bind(&ExecutorWorker::executorCallback, this, std::placeholders::_1);
                job->setAdditionalCallback(callback);
                JobManager::instance()->queueAsyncJob(job, Poco::Thread::PRIO_NORMAL);
                _ongoingJobs.insert({job->jobId(), job});
                _jobToSyncOpMap.insert({job->jobId(), syncOp});
            } else {
                if (!bypassProgressComplete) {
                    if (ignored) {
                        setProgressComplete(syncOp, SyncFileStatus::Ignored);
                    } else if (syncOp->affectedNode() && syncOp->affectedNode()->inconsistencyType() != InconsistencyType::None) {
                        setProgressComplete(syncOp, SyncFileStatus::Inconsistency);
                    } else {
                        setProgressComplete(syncOp, hydrating ? SyncFileStatus::Syncing : SyncFileStatus::Success);
                    }
                }

                if (syncOp->affectedNode()->id()) removeSyncNodeFromWhitelistIfSynced(*syncOp->affectedNode()->id());
            }
        }

        perfMonitor.stop();
        sentry::pTraces::scoped::waitForAllJobsToFinish perfMonitorwaitForAllJobsToFinish(syncDbId());
        if (ExitInfo exitInfo = waitForAllJobsToFinish(); !exitInfo) {
            executorExitInfo = exitInfo;
            break;
        }
        perfMonitorwaitForAllJobsToFinish.stop();
    }

    _syncPal->_syncOps->clear();
    _syncPal->_remoteFSObserverWorker->forceUpdate();

    if (changesCounter > SNAPSHOT_INVALIDATION_THRESHOLD || _snapshotToInvalidate) {
        // If there are too many changes on the local filesystem, the OS stops sending events at some point.
        // Also, on some specific errors, we want to force the snapshot reconstruction.
        LOG_SYNCPAL_INFO(_logger, "Forcing local snapshot invalidation.");
        _syncPal->_localFSObserverWorker->invalidateSnapshot();
    }

    _syncPal->vfs()->cleanUpStatuses();

    setExitCause(executorExitInfo.cause());
    LOG_SYNCPAL_DEBUG(_logger, "Worker stopped: name=" << name() << " " << executorExitInfo);
    setDone(executorExitInfo.code());
}

void ExecutorWorker::initProgressManager() {
    sentry::pTraces::scoped::InitProgress perfMonitor(syncDbId());

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
    syncItem.setModTime(syncOp->affectedNode()->modificationTime().value_or(0));
    syncItem.setCreationTime(syncOp->affectedNode()->createdAt().value_or(0));

    if (bitWiseEnumToBool(syncOp->type() & OperationType::Move)) {
        syncItem.setInstruction(SyncFileInstruction::Move);
        syncItem.setPath(syncOp->affectedNode()->moveOriginInfos().path());
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

void ExecutorWorker::setProgressComplete(const SyncOpPtr syncOp, SyncFileStatus status, const NodeId &newRemoteNodeId) {
    SyncPath relativeLocalFilePath;
    if (syncOp->type() == OperationType::Create || syncOp->type() == OperationType::Edit) {
        relativeLocalFilePath = syncOp->nodePath(ReplicaSide::Local);
    } else {
        relativeLocalFilePath = syncOp->affectedNode()->getPath();
    }

    if (!_syncPal->setProgressComplete(relativeLocalFilePath, status, newRemoteNodeId)) {
        LOGW_SYNCPAL_WARN(_logger, L"Error in SyncPal::setProgressComplete: " << Utility::formatSyncPath(relativeLocalFilePath));
    }
}

ExitInfo ExecutorWorker::handleCreateOp(SyncOpPtr syncOp, std::shared_ptr<AbstractJob> &job, bool &ignored, bool &hydrating) {
    // The execution of the create operation consists of three steps:
    // 1. If omit-flag is False, propagate the file or directory to target replica, because the object is missing there.
    // 2. Insert a new entry into the database, to avoid that the object is detected again by compute_ops() on the next
    // sync iteration.
    // 3. Update the update tree structures to ensure that follow-up operations can execute correctly, as they are based
    // on the information in these structures.
    ignored = false;

    SyncPath relativeLocalFilePath = syncOp->nodePath(ReplicaSide::Local);
    assert(!relativeLocalFilePath.empty());
    SyncPath absoluteLocalFilePath = _syncPal->localPath() / relativeLocalFilePath;
    if (isLiteSyncActivated() && !syncOp->omit()) {
        bool isDehydratedPlaceholder = false;
        if (ExitInfo exitInfo = checkLiteSyncInfoForCreate(syncOp, absoluteLocalFilePath, isDehydratedPlaceholder); !exitInfo) {
            LOG_SYNCPAL_WARN(_logger, "Error in checkLiteSyncInfoForCreate" << " " << exitInfo);
            return exitInfo;
        }

        if (isDehydratedPlaceholder) {
            // Blacklist dehydrated placeholder
            PlatformInconsistencyCheckerUtility::renameLocalFile(absoluteLocalFilePath,
                                                                 PlatformInconsistencyCheckerUtility::SuffixType::Blacklisted);

            // Clear update tree
            if (!deleteOpNodes(syncOp)) {
                LOG_SYNCPAL_WARN(_logger, "Error in ExecutorWorker::deleteOpNodes");
                return ExitCode::DataError;
            }

            return ExitCode::Ok;
        }
    }

    if (syncOp->omit()) {
        // Do not generate job, only push changes in DB and update tree
        std::shared_ptr<Node> node;
        if (ExitInfo exitInfo = propagateCreateToDbAndTree(syncOp, syncOp->correspondingNode()->id().value_or(""),
                                                           syncOp->affectedNode()->createdAt(),
                                                           syncOp->affectedNode()->modificationTime(), node);
            !exitInfo) {
            LOGW_SYNCPAL_WARN(_logger, L"Failed to propagate changes in DB or update tree for "
                                               << Utility::formatSyncName(syncOp->affectedNode()->name()) << L" " << exitInfo);
            return exitInfo;
        }
    } else {
        if (!isLiteSyncActivated() && !enoughLocalSpace(syncOp)) {
            _syncPal->addError(Error(_syncPal->syncDbId(), shortName(), ExitCode::SystemError, ExitCause::NotEnoughDiskSpace));
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
                if (!exists) return {ExitCode::DataError, ExitCause::NotFound};

                // Ignore operation
                if (SyncFileItem syncItem; _syncPal->getSyncFileItem(relativeLocalFilePath, syncItem)) {
                    const Error err(_syncPal->syncDbId(), syncItem.localNodeId().value_or(""),
                                    syncItem.remoteNodeId().value_or(""), syncItem.type(), syncItem.path(), syncItem.conflict(),
                                    syncItem.inconsistency(), CancelType::Create);
                    _syncPal->addError(err);
                }

                if (const std::shared_ptr<UpdateTree> sourceUpdateTree = affectedUpdateTree(syncOp);
                    !sourceUpdateTree->deleteNode(syncOp->affectedNode())) {
                    LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTree::deleteNode: node "
                                                       << Utility::formatSyncName(syncOp->affectedNode()->name()));
                    return ExitCode::DataError;
                }
            }

            ignored = true;
            LOGW_SYNCPAL_INFO(_logger,
                              L"Forbidden destination, operation ignored: " << Utility::formatSyncPath(absoluteLocalFilePath));
            return ExitCode::Ok;
        }

        if (ExitInfo exitInfo = generateCreateJob(syncOp, job, hydrating); !exitInfo) {
            LOGW_SYNCPAL_WARN(_logger, L"Failed to generate create job for: " << SyncName2WStr(syncOp->affectedNode()->name())
                                                                              << L" " << exitInfo);
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
                        LOG_SYNCPAL_WARN(_logger,
                                         "Error in ExecutorWorker::checkAlreadyExcluded" << " " << exitInfoCheckAlreadyExcluded);
                        return exitInfoCheckAlreadyExcluded;
                    }

                    if (const ExitInfo exitInfo = handleForbiddenAction(syncOp, relativeLocalFilePath, ignored); !exitInfo) {
                        LOGW_SYNCPAL_WARN(_logger, L"Error in handleForbiddenAction for item: "
                                                           << Utility::formatSyncPath(relativeLocalFilePath) << L" " << exitInfo);
                        return exitInfo;
                    }
                    return {ExitCode::BackError, ExitCause::FileAccessError};
                }
                return exitInfoRunCreateDirJob;
            }

            if (const ExitInfo exitInfo =
                        convertToPlaceholder(relativeLocalFilePath, syncOp->targetSide() == ReplicaSide::Remote);
                !exitInfo) {
                LOGW_SYNCPAL_WARN(_logger, L"Failed to convert to placeholder for: "
                                                   << Utility::formatSyncName(syncOp->affectedNode()->name()) << L" "
                                                   << exitInfo);
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
    std::shared_ptr<GetFileListJob> job = nullptr;
    try {
        job = std::make_shared<GetFileListJob>(_syncPal->driveDbId(), parentId);
    } catch (const std::exception &e) {
        LOG_SYNCPAL_WARN(Log::instance()->getLogger(), "Error in GetFileListJob::GetFileListJob for driveDbId="
                                                               << _syncPal->driveDbId() << " nodeId=" << parentId.c_str()
                                                               << " error=" << e.what());
        return AbstractTokenNetworkJob::exception2ExitCode(e);
    }

    if (const auto exitInfo = job->runSynchronously(); !exitInfo) {
        LOG_SYNCPAL_WARN(_logger, "Error in GetFileListJob::runSynchronously for driveDbId="
                                          << _syncPal->driveDbId() << " nodeId=" << parentId << " : " << job->exitInfo());
        return exitInfo;
    }

    Poco::JSON::Object::Ptr resObj = job->jsonRes();
    if (!resObj) {
        LOG_SYNCPAL_WARN(Log::instance()->getLogger(),
                         "GetFileListJob failed for driveDbId=" << _syncPal->driveDbId() << " nodeId=" << parentId);
        return {ExitCode::BackError, ExitCause::ApiErr};
    }

    Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
    if (!dataArray) {
        LOG_SYNCPAL_WARN(Log::instance()->getLogger(),
                         "GetFileListJob failed for driveDbId=" << _syncPal->driveDbId() << " nodeId=" << parentId);
        return {ExitCode::BackError, ExitCause::ApiErr};
    }

    for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
        Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
        SyncName name;
        if (!JsonParserUtility::extractValue(obj, nameKey, name)) {
            LOG_SYNCPAL_WARN(Log::instance()->getLogger(),
                             "GetFileListJob failed for driveDbId=" << _syncPal->driveDbId() << " nodeId=" << parentId);
            return {ExitCode::BackError, ExitCause::ApiErr};
        }

        if (name == absolutePath.filename()) {
            alreadyExist = true;
            break;
        }
    }

    if (!alreadyExist) {
        return ExitCode::Ok;
    }
    return {ExitCode::DataError, ExitCause::FileExists};
}

ExitInfo ExecutorWorker::generateCreateJob(SyncOpPtr syncOp, std::shared_ptr<AbstractJob> &job, bool &hydrating) noexcept {
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
            LOGW_SYNCPAL_WARN(_logger, L"Failed to get corresponding parent node: "
                                               << Utility::formatSyncName(syncOp->affectedNode()->name()));
            return ExitCode::DataError;
        }
    }

    if (syncOp->targetSide() == ReplicaSide::Local) {
        SyncPath relativeLocalFilePath = newCorrespondingParentNode->getPath() / syncOp->newName();
        SyncPath absoluteLocalFilePath = _syncPal->localPath() / relativeLocalFilePath;
        syncOp->setLocalCreationTargetPath(relativeLocalFilePath);

        bool placeholderCreation = isLiteSyncActivated() && syncOp->affectedNode()->type() == NodeType::File;
        if (placeholderCreation && syncOp->affectedNode()->id().has_value()) {
            const bool isLink = _syncPal->snapshot(ReplicaSide::Remote)->isLink(*syncOp->affectedNode()->id());
            placeholderCreation = !isLink;
        }

        if (placeholderCreation) {
            if (ExitInfo exitInfo = createPlaceholder(relativeLocalFilePath); !exitInfo) {
                LOGW_SYNCPAL_WARN(_logger, L"Failed to create placeholder for: "
                                                   << Utility::formatSyncName(syncOp->affectedNode()->name()) << L" "
                                                   << exitInfo);
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
            if (ExitInfo exitInfo =
                        propagateCreateToDbAndTree(syncOp, std::to_string(fileStat.inode), syncOp->affectedNode()->createdAt(),
                                                   syncOp->affectedNode()->modificationTime(), newNode);
                !exitInfo) {
                LOGW_SYNCPAL_WARN(_logger, L"Failed to propagate changes in DB or update tree for: "
                                                   << Utility::formatSyncName(syncOp->affectedNode()->name()) << L" "
                                                   << exitInfo);
                return exitInfo;
            }

            PinState pinState = _syncPal->vfs()->pinState(absoluteLocalFilePath);
            hydrating = pinState == PinState::AlwaysLocal;

        } else {
            if (syncOp->affectedNode()->type() == NodeType::Directory) {
                job = std::make_shared<LocalCreateDirJob>(absoluteLocalFilePath, syncOp->affectedNode()->isSpecialFolder());
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
                        job = std::make_shared<DownloadJob>(_syncPal->vfs(), _syncPal->driveDbId(),
                                                            syncOp->affectedNode()->id().value_or(""), absoluteLocalFilePath,
                                                            syncOp->affectedNode()->size(),
                                                            syncOp->affectedNode()->createdAt().value_or(0),
                                                            syncOp->affectedNode()->modificationTime().value_or(0), true);
                    } catch (std::exception const &e) {
                        LOGW_SYNCPAL_WARN(_logger, L"Error in DownloadJob::DownloadJob for driveDbId="
                                                           << _syncPal->driveDbId() << L" : " << CommonUtility::s2ws(e.what()));
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
                LOGW_SYNCPAL_WARN(_logger, L"Failed to convert to placeholder for: "
                                                   << Utility::formatSyncName(syncOp->affectedNode()->name()) << L" "
                                                   << exitInfo);
                _syncPal->setRestart(true);

                if (!_syncPal->updateTree(ReplicaSide::Local)->deleteNode(syncOp->affectedNode())) {
                    LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTree::deleteNode: node name="
                                                       << Utility::formatSyncName(syncOp->affectedNode()->name()) << L" "
                                                       << exitInfo);
                }

                return exitInfo;
            }

            try {
                job = std::make_shared<CreateDirJob>(_syncPal->vfs(), _syncPal->driveDbId(), absoluteLocalFilePath,
                                                     newCorrespondingParentNode->id().value_or(""), syncOp->newName());
            } catch (std::exception const &e) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in CreateDirJob::CreateDirJob for driveDbId="
                                                   << _syncPal->driveDbId() << L" : " << CommonUtility::s2ws(e.what()));
                return ExitCode::DataError;
            }
        } else {
#if defined(KD_WINDOWS)
            // Don't do this on macOS as status and pin state are set at the end of the upload
            if (ExitInfo exitInfo = convertToPlaceholder(relativeLocalFilePath, true); !exitInfo) {
                LOGW_SYNCPAL_WARN(_logger, L"Failed to convert to placeholder for: "
                                                   << Utility::formatSyncName(syncOp->affectedNode()->name()) << L" "
                                                   << exitInfo);
                return exitInfo;
            }
#endif

            uint64_t filesize = 0;
            if (ExitInfo exitInfo = getFileSize(absoluteLocalFilePath, filesize); !exitInfo) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in ExecutorWorker::getFileSize for "
                                                   << Utility::formatSyncPath(absoluteLocalFilePath) << L" " << exitInfo);
                return exitInfo;
            }

            if (filesize > bigFileThreshold) {
                try {
                    const int uploadSessionParallelJobs = ParametersCache::instance()->parameters().uploadSessionParallelJobs();
                    job = std::make_shared<DriveUploadSession>(_syncPal->vfs(), _syncPal->driveDbId(), _syncPal->syncDb(),
                                                               absoluteLocalFilePath, syncOp->affectedNode()->name(),
                                                               newCorrespondingParentNode->id().value_or(""),
                                                               syncOp->affectedNode()->createdAt().value_or(0),
                                                               syncOp->affectedNode()->modificationTime().value_or(0),
                                                               isLiteSyncActivated(), uploadSessionParallelJobs);
                } catch (std::exception const &e) {
                    LOGW_SYNCPAL_WARN(_logger,
                                      L"Error in DriveUploadSession::DriveUploadSession: " << CommonUtility::s2ws(e.what()));
                    return ExitCode::DataError;
                }
            } else {
                try {
                    job = std::make_shared<UploadJob>(
                            _syncPal->vfs(), _syncPal->driveDbId(), absoluteLocalFilePath, syncOp->affectedNode()->name(),
                            newCorrespondingParentNode->id().value_or(""), syncOp->affectedNode()->createdAt().value_or(0),
                            syncOp->affectedNode()->modificationTime().value_or(0));
                } catch (std::exception const &e) {
                    LOGW_SYNCPAL_WARN(_logger, L"Error in UploadJob::UploadJob for driveDbId=" << _syncPal->driveDbId() << L" : "
                                                                                               << CommonUtility::s2ws(e.what()));
                    return ExitCode::DataError;
                }
            }

            job->setAffectedFilePath(relativeLocalFilePath);
        }
    }

    return ExitCode::Ok;
}

ExitInfo ExecutorWorker::checkLiteSyncInfoForCreate(SyncOpPtr syncOp, const SyncPath &path, bool &isDehydratedPlaceholder) {
    isDehydratedPlaceholder = false;

    if (syncOp->targetSide() == ReplicaSide::Remote) {
        if (syncOp->affectedNode()->type() == NodeType::Directory) {
            return ExitCode::Ok;
        }

        VfsStatus vfsStatus;
        if (ExitInfo exitInfo = _syncPal->vfs()->status(path, vfsStatus); !exitInfo) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in vfsStatus: " << Utility::formatSyncPath(path) << L" " << exitInfo);
            return exitInfo;
        }

        if (vfsStatus.isPlaceholder && !vfsStatus.isHydrated && !vfsStatus.isSyncing) {
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

    if (ExitInfo exitInfo = _syncPal->vfs()->createPlaceholder(relativeLocalPath, syncItem); !exitInfo) {
        return exitInfo;
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

#if defined(KD_MACOS)
    // VfsMac::convertToPlaceholder needs only SyncFileItem::_dehydrated
    syncItem.setDehydrated(!hydrated);
#elif defined(KD_WINDOWS)
    // VfsWin::convertToPlaceholder needs only SyncFileItem::_localNodeId
    FileStat fileStat;
    IoError ioError = IoError::Success;
    if (!IoHelper::getFileStat(absoluteLocalFilePath, &fileStat, ioError)) {
        LOGW_SYNCPAL_WARN(_logger, L"Error in IoHelper::getFileStat: " << Utility::formatIoError(absoluteLocalFilePath, ioError));
        return ExitCode::SystemError;
    }
    if (ioError == IoError::NoSuchFileOrDirectory) {
        LOGW_SYNCPAL_WARN(_logger, L"Item does not exist anymore: " << Utility::formatSyncPath(absoluteLocalFilePath));
        return {ExitCode::SystemError, ExitCause::NotFound};
    } else if (ioError == IoError::AccessDenied) {
        LOGW_SYNCPAL_WARN(_logger, L"Item misses search permission: " << Utility::formatSyncPath(absoluteLocalFilePath));
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    syncItem.setLocalNodeId(std::to_string(fileStat.inode));
#endif

    if (ExitInfo exitInfo = _syncPal->vfs()->convertToPlaceholder(absoluteLocalFilePath, syncItem); !exitInfo) {
        return exitInfo;
    }

    if (ExitInfo exitInfo =
                _syncPal->vfs()->setPinState(absoluteLocalFilePath, hydrated ? PinState::AlwaysLocal : PinState::OnlineOnly);
        !exitInfo) {
        LOGW_SYNCPAL_WARN(_logger, L"Error in vfsSetPinState: " << Utility::formatSyncPath(absoluteLocalFilePath) << exitInfo);
        return exitInfo;
    }

    return ExitCode::Ok;
}

ExitInfo ExecutorWorker::handleEditOp(SyncOpPtr syncOp, std::shared_ptr<AbstractJob> &job, bool &ignored) {
    // The execution of the edit operation consists of three steps:
    // 1. If omit-flag is False, propagate the file to replicaY, replacing the existing one.
    // 2. Insert a new entry into the database, to avoid that the object is detected again by compute_ops() on the next
    // sync iteration.
    // 3. If the omit flag is False, update the updatetreeY structure to ensure that follow-up operations can execute
    // correctly, as they are based on the information in this structure.

    ignored = false;
    SyncPath relativeLocalFilePath = syncOp->nodePath(ReplicaSide::Local);
    if (relativeLocalFilePath.empty()) {
        return ExitCode::DataError;
    }

    if (isLiteSyncActivated() && !syncOp->omit()) {
        SyncPath absoluteLocalFilePath = _syncPal->localPath() / relativeLocalFilePath;
        bool ignoreItem = false;
        bool isSyncing = false;
        if (ExitInfo exitInfo = checkLiteSyncInfoForEdit(syncOp, absoluteLocalFilePath, ignoreItem, isSyncing); !exitInfo) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in checkLiteSyncInfoForEdit " << exitInfo);
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
        if (ExitInfo exitInfo = propagateEditToDbAndTree(syncOp, syncOp->correspondingNode()->id().value_or(""),
                                                         syncOp->affectedNode()->createdAt(),
                                                         syncOp->affectedNode()->modificationTime().value_or(0), node);
            !exitInfo) {
            LOGW_SYNCPAL_WARN(_logger, L"Failed to propagate changes in DB or update tree for "
                                               << Utility::formatSyncName(syncOp->affectedNode()->name()));
            return exitInfo;
        }
        return ExitCode::Ok;
    }

    if (!enoughLocalSpace(syncOp)) {
        _syncPal->addError(Error(_syncPal->syncDbId(), shortName(), ExitCode::SystemError, ExitCause::NotEnoughDiskSpace));
        return {ExitCode::SystemError, ExitCause::NotEnoughDiskSpace};
    }

    if (ExitInfo exitInfo = generateEditJob(syncOp, job); !exitInfo) {
        LOGW_SYNCPAL_WARN(_logger, L"Failed to generate edit job for: " << SyncName2WStr(syncOp->affectedNode()->name()) << L" "
                                                                        << exitInfo);
        return exitInfo;
    }
    return ExitCode::Ok;
}

ExitInfo ExecutorWorker::generateEditJob(SyncOpPtr syncOp, std::shared_ptr<AbstractJob> &job) {
    // 1. If omit-flag is False, propagate the file to replicaY, replacing the existing one.
    if (syncOp->targetSide() == ReplicaSide::Local) {
        SyncPath relativeLocalFilePath = syncOp->nodePath(ReplicaSide::Local);
        SyncPath absoluteLocalFilePath = _syncPal->localPath() / relativeLocalFilePath;

        try {
            job = std::make_shared<DownloadJob>(_syncPal->vfs(), _syncPal->driveDbId(), syncOp->affectedNode()->id().value_or(""),
                                                absoluteLocalFilePath, syncOp->affectedNode()->size(),
                                                syncOp->affectedNode()->createdAt().value_or(0),
                                                syncOp->affectedNode()->modificationTime().value_or(0), false);
        } catch (std::exception const &e) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in DownloadJob::DownloadJob for driveDbId=" << _syncPal->driveDbId() << L" : "
                                                                                           << CommonUtility::s2ws(e.what()));
            return ExitCode::DataError;
        }

        job->setAffectedFilePath(relativeLocalFilePath);
    } else {
        SyncPath relativeLocalFilePath = syncOp->nodePath(ReplicaSide::Local);
        SyncPath absoluteLocalFilePath = _syncPal->localPath() / relativeLocalFilePath;

        uint64_t filesize;
        if (ExitInfo exitInfo = getFileSize(absoluteLocalFilePath, filesize); !exitInfo) {
            LOGW_WARN(_logger, L"Error in ExecutorWorker::getFileSize for " << Utility::formatSyncPath(absoluteLocalFilePath)
                                                                            << L" " << exitInfo);
            return exitInfo;
        }

        if (!syncOp->correspondingNode()->id()) {
            // Should not happen
            LOGW_SYNCPAL_WARN(_logger, L"Edit operation with empty corresponding node id for "
                                               << Utility::formatSyncPath(absoluteLocalFilePath));
            sentry::Handler::captureMessage(sentry::Level::Warning, "ExecutorWorker::generateEditJob",
                                            "Edit operation with empty corresponding node id");
            return ExitCode::DataError;
        }

        if (filesize > bigFileThreshold) {
            try {
                int uploadSessionParallelJobs = ParametersCache::instance()->parameters().uploadSessionParallelJobs();
                job = std::make_shared<DriveUploadSession>(_syncPal->vfs(), _syncPal->driveDbId(), _syncPal->syncDb(),
                                                           absoluteLocalFilePath, syncOp->correspondingNode()->id().value_or(""),
                                                           syncOp->affectedNode()->modificationTime().value_or(0),
                                                           isLiteSyncActivated(), uploadSessionParallelJobs);
            } catch (std::exception const &e) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in DriveUploadSession::DriveUploadSession: " << CommonUtility::s2ws(e.what()));
                return ExitCode::DataError;
            };
        } else {
            try {
                job = std::make_shared<UploadJob>(_syncPal->vfs(), _syncPal->driveDbId(), absoluteLocalFilePath,
                                                  syncOp->correspondingNode()->id().value_or(""),
                                                  syncOp->affectedNode()->modificationTime().value_or(0));
            } catch (std::exception const &e) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in UploadJob::UploadJob for driveDbId=" << _syncPal->driveDbId() << L" : "
                                                                                           << CommonUtility::s2ws(e.what()));
                return ExitCode::DataError;
            }
        }

        job->setAffectedFilePath(relativeLocalFilePath);
    }
    return ExitCode::Ok;
}

ExitInfo ExecutorWorker::fixModificationDate(SyncOpPtr syncOp, const SyncPath &absolutePath) {
    const auto id = syncOp->affectedNode()->id().value_or("");
    LOGW_SYNCPAL_DEBUG(_logger, L"Do not upload dehydrated placeholders: " << Utility::formatSyncPath(absolutePath) << L" ("
                                                                           << CommonUtility::s2ws(id) << L")");

    // Update last modification date in order to avoid generating more EDIT operations.
    bool found = false;
    DbNode dbNode;
    if (!_syncPal->syncDb()->node(*syncOp->correspondingNode()->idb(), dbNode, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::node");
        return {ExitCode::DbError, ExitCause::DbAccessError};
    }
    if (!found) {
        LOG_SYNCPAL_DEBUG(_logger, "Failed to retrieve node for dbId=" << *syncOp->correspondingNode()->idb());
        return {ExitCode::DataError, ExitCause::DbEntryNotFound};
    }

    if (const IoError ioError = IoHelper::setFileDates(absolutePath, dbNode.created().value_or(0),
                                                       dbNode.lastModifiedRemote().value_or(0), false);
        ioError == IoError::Success) {
        LOGW_SYNCPAL_INFO(_logger,
                          L"Last modification date updated locally to avoid further wrongly generated EDIT operations for file: "
                                  << Utility::formatSyncPath(absolutePath));
    } else if (ioError == IoError::NoSuchFileOrDirectory) {
        // If file does not exist anymore, do nothing special. This is fine, it will not generate EDIT operations anymore.
    } else {
        LOGW_SYNCPAL_WARN(_logger, L"Error in IoHelper::setFileDates: " << Utility::formatSyncPath(absolutePath));
    }

    return ExitCode::Ok;
}

ExitInfo ExecutorWorker::checkLiteSyncInfoForEdit(SyncOpPtr syncOp, const SyncPath &absolutePath, bool &ignoreItem,
                                                  bool &isSyncing) {
    ignoreItem = false;

    VfsStatus vfsStatus;
    if (ExitInfo exitInfo = _syncPal->vfs()->status(absolutePath, vfsStatus); !exitInfo) {
        LOGW_SYNCPAL_WARN(_logger, L"Error in vfsStatus: " << Utility::formatSyncPath(absolutePath) << L": " << exitInfo);
        return exitInfo;
    }
    isSyncing = vfsStatus.isSyncing;

    switch (syncOp->targetSide()) {
        case ReplicaSide::Remote:
            if (vfsStatus.isPlaceholder && !vfsStatus.isHydrated) {
                ignoreItem = true;
                return fixModificationDate(syncOp, absolutePath);
            }
            break;
        case ReplicaSide::Local:
            if (vfsStatus.isPlaceholder && !isSyncing) {
                if (!vfsStatus.isHydrated) {
                    // Update metadata
                    std::string error;
                    if (const auto exitInfo = _syncPal->vfs()->updateMetadata(
                                absolutePath, syncOp->affectedNode()->createdAt().value_or(0),
                                syncOp->affectedNode()->modificationTime().value_or(0), syncOp->affectedNode()->size(),
                                syncOp->affectedNode()->id().value_or(""));
                        !exitInfo) {
                        return exitInfo;
                    }
                    syncOp->setOmit(true);
                } // else: the file is hydrated, we can proceed with download
            }
            break;
        default:
            LOGW_WARN(_logger, L"Invalid target side: " << syncOp->targetSide());
            return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo ExecutorWorker::handleMoveOp(SyncOpPtr syncOp, bool &ignored, bool &bypassProgressComplete) {
    // The three execution steps are as follows:
    // 1. If omit-flag is False, move the object on replica Y (where it still needs to be moved) from uY to vY, changing
    // the name to nameX.
    // 2. Update the database entry, to avoid detecting the move operation again.
    // 3. If the omit flag is False, update the updatetreeY structure to ensure that follow-up operations can execute
    // correctly, as they are based on the information in this structure.
    ignored = false;
    bypassProgressComplete = false;
    if (syncOp->omit()) {
        // Do not generate job, only push changes in DB and update tree
        if (syncOp->hasConflict()) {
            bool propagateChange = true;
            if (ExitInfo exitInfo = propagateConflictToDbAndTree(syncOp, propagateChange); !propagateChange || !exitInfo) {
                return exitInfo;
            }
        }

        if (const auto exitInfo = propagateMoveToDbAndTree(syncOp); !exitInfo) {
            LOGW_SYNCPAL_WARN(_logger, L"Failed to propagate changes in DB or update tree for: "
                                               << Utility::formatSyncName(syncOp->affectedNode()->name()) << L" " << exitInfo);
            return exitInfo;
        }
    } else if (syncOp->isRescueOperation()) {
        FileRescuer fileRescuer(_syncPal);
        if (const auto exitInfo = fileRescuer.executeRescueMoveJob(syncOp); !exitInfo) return exitInfo;
    } else {
        if (ExitInfo exitInfo = generateMoveJob(syncOp, ignored, bypassProgressComplete); !exitInfo) {
            LOGW_SYNCPAL_WARN(_logger, L"Failed to generate move job for: "
                                               << Utility::formatSyncName(syncOp->affectedNode()->name()) << L" " << exitInfo);
            return exitInfo;
        }
    }
    return ExitCode::Ok;
}

ExitInfo ExecutorWorker::generateMoveJob(SyncOpPtr syncOp, bool &ignored, bool &bypassProgressComplete) {
    bypassProgressComplete = false;

    // 1. If omit-flag is False, move the object on replica Y (where it still needs to be moved) from uY to vY, changing
    // the name to nameX.
    std::shared_ptr<AbstractJob> job = nullptr;

    SyncPath relativeDestLocalFilePath;
    SyncPath absoluteDestLocalFilePath;
    SyncPath relativeOriginLocalFilePath;
    SyncPath absoluteOriginLocalFilePath;

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
        absoluteOriginLocalFilePath = _syncPal->localPath() / relativeOriginLocalFilePath;

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
        absoluteOriginLocalFilePath = _syncPal->localPath() / relativeOriginLocalFilePath;

        if (syncOp->isBreakingCycleOp() || relativeOriginLocalFilePath.parent_path() == relativeDestLocalFilePath.parent_path()) {
            // This is just a rename
            try {
                job = std::make_shared<RenameJob>(_syncPal->vfs(), _syncPal->driveDbId(), correspondingNode->id().value_or(""),
                                                  absoluteDestLocalFilePath);
            } catch (std::exception const &e) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in RenameJob::RenameJob for driveDbId=" << _syncPal->driveDbId() << L" : "
                                                                                           << CommonUtility::s2ws(e.what()));
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

            const NodeId fileId = correspondingNode->id().value_or("");
            const NodeId destDirId = remoteParentNode->id().value_or("");
            try {
                job = std::make_shared<MoveJob>(_syncPal->vfs(), _syncPal->driveDbId(), absoluteDestLocalFilePath, fileId,
                                                destDirId, syncOp->newName());
            } catch (std::exception &e) {
                LOG_SYNCPAL_WARN(_logger, "Error in GetTokenFromAppPasswordJob::GetTokenFromAppPasswordJob: error=" << e.what());
                return ExitCode::DataError;
            }
        }

        if (syncOp->hasConflict() || syncOp->isBreakingCycleOp()) {
            job->setBypassCheck(true);
        }
    }

    job->setAffectedFilePath(relativeDestLocalFilePath);
    job->runSynchronously();

    VfsStatus vfsStatus;
    _syncPal->vfs()->status(absoluteDestLocalFilePath, vfsStatus);
    vfsStatus.isSyncing = false;
    vfsStatus.progress = 100;
    _syncPal->vfs()->forceStatus(absoluteDestLocalFilePath, vfsStatus);

    if (job->exitInfo().code() == ExitCode::Ok && syncOp->conflict().type() != ConflictType::None) {
        // Conflict fixing job finished successfully
        // Propagate changes to DB and update trees
        std::shared_ptr<Node> newNode = nullptr;
        if (ExitInfo exitInfo = propagateChangeToDbAndTree(syncOp, job, newNode); !exitInfo) {
            LOGW_WARN(_logger, L"Failed to propagate changes in DB or update tree for: "
                                       << Utility::formatSyncName(syncOp->affectedNode()->name()) << L" " << exitInfo);
            return exitInfo;
        }

        switch (syncOp->conflict().type()) {
            case ConflictType::CreateCreate:
            case ConflictType::EditEdit:
            case ConflictType::MoveMoveSource:
            case ConflictType::MoveMoveDest:
            case ConflictType::MoveMoveCycle:
            case ConflictType::MoveCreate: {
                // Send conflict notification
                Error err(_syncPal->syncDbId(), *syncOp->localNode()->id(), *syncOp->remoteNode()->id(),
                          syncOp->localNode()->type(), syncOp->relativeOriginPath(), syncOp->conflict().type(),
                          InconsistencyType::None, CancelType::None, syncOp->relativeDestinationPath());
                _syncPal->addError(err);
                break;
            }
            default:
                // Do not show error message for other conflicts
                break;
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
    // 3. Update the update tree structures to ensure that follow-up operations can execute correctly, as they are based
    // on the information in these structures
    ignored = false;
    bypassProgressComplete = false;

    if (syncOp->omit()) {
        // Do not generate job, only push changes in DB and update tree
        if (syncOp->hasConflict()) { // Error message handled with move operation in case Edit-Delete conflict
            bool propagateChange = true;
            if (const ExitInfo exitInfo = propagateConflictToDbAndTree(syncOp, propagateChange); !propagateChange || !exitInfo) {
                return exitInfo;
            }
        }

        if (const ExitInfo exitInfo = propagateDeleteToDbAndTree(syncOp); !exitInfo) {
            LOGW_SYNCPAL_WARN(_logger, L"Failed to propagate changes in DB or update tree for: "
                                               << Utility::formatSyncName(syncOp->affectedNode()->name()) << L" " << exitInfo);
            return exitInfo;
        }
    } else {
        if (const ExitInfo exitInfo = generateDeleteJob(syncOp, ignored, bypassProgressComplete); !exitInfo) {
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
    bool isDehydratedPlaceholder = false;
    if (syncOp->targetSide() == ReplicaSide::Local) {
        if (_syncPal->vfsMode() != VirtualFileMode::Off && syncOp->localNode()->type() == NodeType::File) {
            VfsStatus vfsStatus;
            if (ExitInfo exitInfo = _syncPal->vfs()->status(absoluteLocalFilePath, vfsStatus); !exitInfo) {
                LOGW_SYNCPAL_WARN(
                        _logger, L"Error in vfsStatus: " << Utility::formatSyncPath(absoluteLocalFilePath) << L" : " << exitInfo);
                return exitInfo;
            }
            isDehydratedPlaceholder = vfsStatus.isPlaceholder && !vfsStatus.isHydrated && !vfsStatus.isSyncing;
        }

        if (syncOp->isDehydratedPlaceholder() && !isDehydratedPlaceholder) {
            LOGW_SYNCPAL_INFO(_logger,
                              L"Placeholder is not dehydrated anymore for " << Utility::formatSyncPath(absoluteLocalFilePath));
            return ExitCode::DataError;
        }

        NodeId remoteNodeId = syncOp->affectedNode()->id().value_or("");
        if (remoteNodeId.empty()) {
            LOGW_SYNCPAL_WARN(_logger, L"Failed to retrieve node ID");
            return ExitCode::DataError;
        }
        job = std::make_shared<LocalDeleteJob>(_syncPal->syncInfo(), relativeLocalFilePath, isDehydratedPlaceholder,
                                               remoteNodeId);
    } else {
        try {
            job = std::make_shared<DeleteJob>(_syncPal->driveDbId(), syncOp->correspondingNode()->id().value_or(""),
                                              syncOp->affectedNode()->id().value_or(""), absoluteLocalFilePath,
                                              syncOp->affectedNode()->type());
        } catch (std::exception const &e) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in DeleteJob::DeleteJob for driveDbId=" << _syncPal->driveDbId() << L" : "
                                                                                       << CommonUtility::s2ws(e.what()));
            return ExitCode::DataError;
        }
    }

    // If affected node has both create and delete events (node deleted and re-created with same name), then do not check
    job->setBypassCheck((syncOp->affectedNode()->hasChangeEvent(OperationType::Create) &&
                         syncOp->affectedNode()->hasChangeEvent(OperationType::Delete)) ||
                        syncOp->affectedNode()->isSharedFolder() ||
                        (syncOp->conflict().type() != ConflictType::None && isDehydratedPlaceholder));

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

    const int64_t freeBytes = Utility::getFreeDiskSpace(_syncPal->localPath());
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
        std::scoped_lock lock(_terminatedJobs);
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
                    } else if (syncOp->type() == OperationType::Create && syncOp->targetSide() == ReplicaSide::Remote) {
                        std::shared_ptr<UploadJob> uploadJob = std::dynamic_pointer_cast<UploadJob>(job);
                        NodeId newRemoteNodeId;
                        if (uploadJob) newRemoteNodeId = uploadJob->nodeId();
                        setProgressComplete(syncOp, SyncFileStatus::Success, newRemoteNodeId);
                    } else {
                        setProgressComplete(syncOp, SyncFileStatus::Success);
                    }

                    if (syncOp->affectedNode()->id()) removeSyncNodeFromWhitelistIfSynced(*syncOp->affectedNode()->id());
                }
            } else {
                increaseErrorCount(syncOp, exitInfo);
            }

            // Delete job
            _ongoingJobs.erase(_terminatedJobs.front());
        }
        _terminatedJobs.pop();
    }
    return exitInfo;
}

ExitInfo ExecutorWorker::handleManagedBackError(const ExitInfo &jobExitInfo, const SyncOpPtr syncOp, const bool invalidName) {
    if (jobExitInfo.cause() == ExitCause::QuotaExceeded) {
        _syncPal->pause();
    } else if (jobExitInfo.cause() == ExitCause::NotFound) {
        // The file might have been already deleted, only increase error counter in this case.
        increaseErrorCount(syncOp, jobExitInfo);
    } else {
        // The item should be temporarily blacklisted
        _syncPal->blacklistTemporarily(syncOp->affectedNode()->id().value_or(""), syncOp->affectedNode()->getPath(),
                                       otherSide(syncOp->targetSide()));
    }

    // Clear update tree
    if (!deleteOpNodes(syncOp)) {
        LOG_SYNCPAL_WARN(_logger, "Error in ExecutorWorker::deleteOpNodes");
        return ExitCode::DataError;
    }

    NodeId localNodeId;
    NodeId remoteNodeId;
    getNodeIdsFromOp(syncOp, localNodeId, remoteNodeId);

    Error error;
    if (invalidName) {
        error = Error(_syncPal->syncDbId(), localNodeId, remoteNodeId, syncOp->affectedNode()->type(),
                      syncOp->affectedNode()->getPath(), ConflictType::None, InconsistencyType::ForbiddenChar);
    }
    if (jobExitInfo.cause() != ExitCause::NotFound) {
        error = Error(_syncPal->syncDbId(), localNodeId, remoteNodeId, syncOp->affectedNode()->type(),
                      syncOp->affectedNode()->getPath(), ConflictType::None, InconsistencyType::None, CancelType::None, "",
                      jobExitInfo.code(), jobExitInfo.cause());
    }
    _syncPal->addError(error);

    return ExitCode::Ok;
}

namespace details {
bool isManagedBackError(const ExitCause exitCause) {
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

    NodeId localNodeId;
    NodeId remoteNodeId;
    getNodeIdsFromOp(syncOp, localNodeId, remoteNodeId);

    auto networkJob(std::dynamic_pointer_cast<AbstractNetworkJob>(job));
    if (job->exitInfo().code() == ExitCode::BackError && details::isManagedBackError(job->exitInfo().cause())) {
        bypassProgressComplete = true;
        return handleManagedBackError(job->exitInfo(), syncOp, job->exitInfo().cause() == ExitCause::InvalidName);
    }

    if (job->exitInfo().code() != ExitCode::Ok) {
        if (networkJob &&
            (networkJob->getStatusCode() == Poco::Net::HTTPResponse::HTTP_FORBIDDEN ||
             networkJob->getStatusCode() == Poco::Net::HTTPResponse::HTTP_CONFLICT) &&
            job->exitInfo() != ExitInfo(ExitCode::BackError, ExitCause::FileLocked)) {
            if (ExitInfo exitInfo = handleForbiddenAction(syncOp, relativeLocalPath, ignored); !exitInfo) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in handleForbiddenAction for item: "
                                                   << Utility::formatSyncPath(relativeLocalPath) << L" " << exitInfo);
                return exitInfo;
            }
        } else if (const auto exitInfo = handleExecutorError(syncOp, job->exitInfo()); !exitInfo) {
            // Cancel all queued jobs
            LOGW_SYNCPAL_WARN(_logger, L"Cancelling jobs. " << job->exitInfo());
            cancelAllOngoingJobs();
            return exitInfo;
        } else { // The error is managed and the execution can continue.
            LOGW_DEBUG(_logger, L"Error successfully managed: " << job->exitInfo() << L" on " << syncOp->type()
                                                                << L" operation for "
                                                                << Utility::formatSyncPath(syncOp->affectedNode()->getPath()));
            bypassProgressComplete = true;
            return {ExitCode::Ok, ExitCause::OperationCanceled};
        }
    } else {
        // Propagate changes to DB and update trees
        std::shared_ptr<Node> newNode;
        if (ExitInfo exitInfo = propagateChangeToDbAndTree(syncOp, job, newNode); !exitInfo) {
            cancelAllOngoingJobs();
            return exitInfo;
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
                LOGW_SYNCPAL_WARN(_logger, L"PlatformInconsistencyCheckerUtility::renameLocalFile failed for "
                                                   << Utility::formatSyncPath(absoluteLocalFilePath));
                _syncPal->handleAccessDeniedItem(relativeLocalPath);
                return ExitCode::Ok;
            }
            removeFromDb = false;
            break;
        }
        case OperationType::Move: {
            // Delete the item from local replica
            const NodeId remoteNodeId = syncOp->correspondingNode()->id().value_or("");
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
            if (!_syncPal->vfs()->fileStatusChanged(newSyncPath, SyncFileStatus::Ignored)) {
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
        const Error err(_syncPal->syncDbId(), syncItem.localNodeId().value_or(""), syncItem.remoteNodeId().value_or(""),
                        syncItem.type(), syncItem.path(), syncItem.conflict(), syncItem.inconsistency(), cancelType,
                        syncOp->affectedNode()->hasChangeEvent(OperationType::Move) ? relativeLocalPath : "");
        _syncPal->addError(err);
    }

    if (!exitInfo) return exitInfo;

    if (removeFromDb) {
        //  Remove the node from DB and tree so it will be re-created at its
        //  original location on next sync
        _syncPal->setRestart(true);
        if (exitInfo = propagateDeleteToDbAndTree(syncOp); !exitInfo) {
            LOGW_SYNCPAL_WARN(_logger, L"Failed to propagate changes in DB or update tree for "
                                               << Utility::formatSyncName(syncOp->affectedNode()->name()));
        }
    }
    return exitInfo;
}

void ExecutorWorker::sendProgress() {
    if (_timer.elapsed<DoubleSeconds>().count() > SEND_PROGRESS_DELAY) {
        _timer.restart();

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
            if (syncOp->conflict().type() != ConflictType::MoveMoveSource &&
                syncOp->conflict().type() != ConflictType::CreateCreate) {
                if (const ExitInfo exitInfo = deleteFromDb(syncOp->conflict().localNode()); !exitInfo) {
                    if (exitInfo.code() == ExitCode::DataError && exitInfo.cause() == ExitCause::DbEntryNotFound) {
                        // The node was not found in DB, this ok since we wanted to remove it anyway
                        LOGW_SYNCPAL_INFO(_logger,
                                          L"Node `" << Utility::formatSyncName(syncOp->conflict().localNode()->name())
                                                    << L" not found in DB. This is ok since we wanted to remove to anyway.");
                    } else {
                        // Remove local node from DB failed!
                        LOGW_SYNCPAL_WARN(_logger, L"deleteFromDb failed for "
                                                           << Utility::formatSyncName(syncOp->conflict().localNode()->name()));
                        propagateChange = false;
                        return exitInfo;
                    }
                }
            }
            // Remove node from update tree
            if (!_syncPal->updateTree(ReplicaSide::Local)->deleteNode(syncOp->conflict().localNode())) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTree::deleteNode: node "
                                                   << Utility::formatSyncName(syncOp->conflict().localNode()->name()));
            }

            if (!_syncPal->updateTree(ReplicaSide::Remote)->deleteNode(syncOp->conflict().remoteNode())) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTree::deleteNode: node "
                                                   << Utility::formatSyncName(syncOp->conflict().remoteNode()->name()));
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
            // removed from DB anyway
            propagateChange = false;
            break;
        }
        case ConflictType::CreateParentDelete: // Indirect conflict pattern
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
            SyncTime newCreationTime = 0;
            SyncTime newModificationTime = 0;
            int64_t newSize = -1;
            if (syncOp->targetSide() == ReplicaSide::Local) {
                auto downloadJob(std::dynamic_pointer_cast<DownloadJob>(job));
                nodeId = downloadJob->localNodeId();
                newCreationTime = downloadJob->creationTime();
                newModificationTime = downloadJob->modificationTime();
                newSize = downloadJob->size();
            } else {
                bool jobOk = false;
                auto uploadJob(std::dynamic_pointer_cast<UploadJob>(job));
                if (uploadJob) {
                    nodeId = uploadJob->nodeId();
                    newCreationTime = uploadJob->creationTime();
                    newModificationTime = uploadJob->modificationTime();
                    newSize = uploadJob->size();
                    jobOk = true;
                } else {
                    auto uploadSessionJob(std::dynamic_pointer_cast<DriveUploadSession>(job));
                    if (uploadSessionJob) {
                        nodeId = uploadSessionJob->nodeId();
                        newCreationTime = uploadSessionJob->creationTime();
                        newModificationTime = uploadSessionJob->modificationTime();
                        newSize = uploadSessionJob->size();
                        jobOk = true;
                    }
                }

                if (!jobOk) {
                    LOGW_SYNCPAL_WARN(_logger, L"Failed to cast upload job " << job->jobId());
                    return ExitCode::SystemError;
                }
            }

            if (syncOp->type() == OperationType::Create) {
                return propagateCreateToDbAndTree(syncOp, nodeId, newCreationTime, newModificationTime, node, newSize);
            } else {
                return propagateEditToDbAndTree(syncOp, nodeId, newCreationTime, newModificationTime, node, newSize);
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
                                                                  << Utility::formatSyncName(syncOp->affectedNode()->name()));
            return ExitCode::SystemError;
        }
    }
    return ExitCode::LogicError;
}

ExitInfo ExecutorWorker::propagateCreateToDbAndTree(SyncOpPtr syncOp, const NodeId &newNodeId,
                                                    std::optional<SyncTime> newCreationTime,
                                                    std::optional<SyncTime> newLastModificationTime, std::shared_ptr<Node> &node,
                                                    const int64_t newSize) {
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
    const int64_t size = newSize > -1 ? newSize : syncOp->affectedNode()->size();
    std::string localId = syncOp->targetSide() == ReplicaSide::Local ? newNodeId : (syncOp->affectedNode()->id().value_or(""));
    std::string remoteId = syncOp->targetSide() == ReplicaSide::Local ? (syncOp->affectedNode()->id().value_or("")) : newNodeId;
    SyncName localName = syncOp->targetSide() == ReplicaSide::Local ? syncOp->newName() : syncOp->nodeName(ReplicaSide::Local);
    SyncName remoteName = syncOp->targetSide() == ReplicaSide::Remote ? syncOp->newName() : syncOp->nodeName(ReplicaSide::Remote);

    if (localId.empty() || remoteId.empty()) {
        LOGW_SYNCPAL_WARN(_logger, L"Empty " << (localId.empty() ? L"local" : L"remote") << L" id for item "
                                             << Utility::formatSyncName(syncOp->affectedNode()->name()));
        return ExitCode::DataError;
    }

    DbNode dbNode(0, newCorrespondingParentNode->idb(), localName, remoteName, localId, remoteId, newCreationTime,
                  newLastModificationTime, newLastModificationTime, syncOp->affectedNode()->type(), size,
                  "" // TODO : change it once we start using content checksum
                  ,
                  syncOp->omit() ? SyncFileStatus::Success : SyncFileStatus::Unknown);

    if (ParametersCache::isExtendedLogEnabled()) {
        LOGW_SYNCPAL_DEBUG(_logger, L"Inserting in DB: " << L" localName=" << Utility::quotedSyncName(localName)
                                                         << L" / remoteName=" << Utility::quotedSyncName(remoteName)
                                                         << L" / localId=" << CommonUtility::s2ws(localId) << L" / remoteId="
                                                         << CommonUtility::s2ws(remoteId) << L" / parent DB ID="
                                                         << newCorrespondingParentNode->idb().value_or(-1) << L" / createdAt="
                                                         << newCreationTime.value_or(-1) << L" / lastModTime="
                                                         << newLastModificationTime.value_or(-1) << L" / type="
                                                         << syncOp->affectedNode()->type() << L" / size=" << size);
    }

    if (dbNode.nameLocal().empty() || dbNode.nameRemote().empty() || !dbNode.nodeIdLocal().has_value() ||
        !dbNode.nodeIdRemote().has_value() || !dbNode.created().has_value() || !dbNode.lastModifiedLocal().has_value() ||
        !dbNode.lastModifiedRemote().has_value() || dbNode.type() == NodeType::Unknown) {
        LOG_SYNCPAL_ERROR(_logger, "Error inserting new node in DB!!!");
        assert(false);
    }

    DbNodeId newDbNodeId;
    bool constraintError = false;
    if (!_syncPal->syncDb()->insertNode(dbNode, newDbNodeId, constraintError)) {
        LOGW_SYNCPAL_WARN(_logger, L"Failed to insert node into DB:" << L" local ID: " << CommonUtility::s2ws(localId)
                                                                     << L", remote ID: " << CommonUtility::s2ws(remoteId)
                                                                     << L", local name: " << Utility::quotedSyncName(localName)
                                                                     << L", remote name: " << Utility::quotedSyncName(remoteName)
                                                                     << L", parent DB ID: "
                                                                     << (newCorrespondingParentNode->idb().value_or(-1)));

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
        if (!_syncPal->syncDb()->dbId(ReplicaSide::Remote, remoteId, dbNodeId, found)) {
            LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbId");
            return {ExitCode::DbError, ExitCause::DbAccessError};
        }
        if (found) {
            // Delete old node
            if (!_syncPal->syncDb()->deleteNode(dbNodeId, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::deleteNode");
                return {ExitCode::DbError, ExitCause::DbAccessError};
            }
        }

        // Create new node
        if (!_syncPal->syncDb()->insertNode(dbNode, newDbNodeId, constraintError)) {
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
                         remoteName, syncOp->affectedNode()->type(), OperationType::None, newNodeId, newCreationTime,
                         newLastModificationTime, size, newCorrespondingParentNode));
        if (node == nullptr) {
            std::cout << "Failed to allocate memory" << std::endl;
            LOG_SYNCPAL_ERROR(_logger, "Failed to allocate memory");
            return {ExitCode::SystemError, ExitCause::NotEnoughMemory};
        }

        std::shared_ptr<UpdateTree> updateTree = targetUpdateTree(syncOp);
        updateTree->insertNode(node);

        if (!newCorrespondingParentNode->insertChildren(node)) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in Node::insertChildren: node "
                                               << Utility::formatSyncName(node->name()) << L" parent node "
                                               << Utility::formatSyncName(newCorrespondingParentNode->name()));
            return ExitCode::DataError;
        }

        // Affected node does not have a valid DB ID yet, update it
        syncOp->affectedNode()->setIdb(newDbNodeId);
    }

    return ExitCode::Ok;
}

ExitInfo ExecutorWorker::propagateEditToDbAndTree(SyncOpPtr syncOp, const NodeId &newNodeId,
                                                  std::optional<SyncTime> newCreationTime,
                                                  std::optional<SyncTime> newLastModificationTime, std::shared_ptr<Node> &node,
                                                  const int64_t newSize) {
    DbNode dbNode;
    bool found = false;
    if (!_syncPal->syncDb()->node(*syncOp->correspondingNode()->idb(), dbNode, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::node");
        return {ExitCode::DbError, ExitCause::DbAccessError};
    }
    if (!found) {
        LOG_SYNCPAL_DEBUG(_logger, "Failed to retrieve node for dbId=" << *syncOp->correspondingNode()->idb());
        return {ExitCode::DataError, ExitCause::DbEntryNotFound};
    }

    // 2. Update the database entry, to avoid detecting the edit operation again.
    std::string localId = syncOp->targetSide() == ReplicaSide::Local ? newNodeId : syncOp->affectedNode()->id().value_or("");
    std::string remoteId = syncOp->targetSide() == ReplicaSide::Local ? syncOp->affectedNode()->id().value_or("") : newNodeId;
    const SyncName localName = syncOp->nodeName(ReplicaSide::Local);
    const SyncName remoteName = syncOp->nodeName(ReplicaSide::Remote);

    if (localId.empty() || remoteId.empty()) {
        LOGW_SYNCPAL_WARN(_logger, L"Empty " << (localId.empty() ? L"local" : L"remote") << L" id for item "
                                             << Utility::formatSyncName(syncOp->affectedNode()->name()));
        return ExitCode::DataError;
    }

    // In case of Delete+Create, the encoding might have changed. Therefor, we update the name anyway.
    const int64_t size = newSize > -1 ? newSize : syncOp->affectedNode()->size();
    dbNode.setNameLocal(localName);
    dbNode.setNameRemote(remoteName);

    dbNode.setNodeIdLocal(localId);
    dbNode.setNodeIdRemote(remoteId);
    dbNode.setCreated(newCreationTime);
    dbNode.setLastModifiedLocal(newLastModificationTime);
    dbNode.setLastModifiedRemote(newLastModificationTime);
    dbNode.setSize(size);
    dbNode.setChecksum(""); // TODO : change it once we start using content checksum
    if (syncOp->omit()) {
        dbNode.setStatus(SyncFileStatus::Success);
    }

    if (ParametersCache::isExtendedLogEnabled()) {
        LOGW_SYNCPAL_DEBUG(_logger, L"Updating DB: " << L" / localName=" << Utility::quotedSyncName(localName)
                                                     << L" / remoteName=" << Utility::quotedSyncName(remoteName) << L" / localId="
                                                     << CommonUtility::s2ws(localId) << L" / remoteId="
                                                     << CommonUtility::s2ws(remoteId) << L" / parent DB ID="
                                                     << dbNode.parentNodeId().value_or(-1) << L" / createdAt="
                                                     << newCreationTime.value_or(-1) << L" / lastModTime="
                                                     << newLastModificationTime.value_or(-1) << L" / type="
                                                     << syncOp->affectedNode()->type() << L" / size=" << size);
    }

    if (!_syncPal->syncDb()->updateNode(dbNode, found)) {
        LOGW_SYNCPAL_WARN(_logger, L"Failed to update node into DB: " << L"local ID: " << CommonUtility::s2ws(localId)
                                                                      << L", remote ID: " << CommonUtility::s2ws(remoteId)
                                                                      << L", local name: " << Utility::quotedSyncName(localName)
                                                                      << L", remote name: " << Utility::quotedSyncName(remoteName)
                                                                      << L", parent DB ID: "
                                                                      << dbNode.parentNodeId().value_or(-1));
        return {ExitCode::DbError, ExitCause::DbAccessError};
    }
    if (!found) {
        return {ExitCode::DataError, ExitCause::DbEntryNotFound};
    }

    // 3. If the omit flag is `false`, update the updatetreeY structure to ensure
    // that follow-up operations can execute correctly, as they are based on the
    // information in this structure
    if (!syncOp->omit()) {
        // ID might have changed in the case of a delete+create
        if (!_syncPal->updateTree(syncOp->targetSide())
                     ->updateNodeId(syncOp->correspondingNode(),
                                    syncOp->targetSide() == ReplicaSide::Local ? localId : remoteId)) {
            LOG_SYNCPAL_WARN(_logger, "Error in UpdateTreeWorker::updateNodeId");
            return ExitCode::DataError;
        }
        syncOp->correspondingNode()->setCreatedAt(newCreationTime);
        syncOp->correspondingNode()->setModificationTime(newLastModificationTime);
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
    if (!_syncPal->syncDb()->node(*correspondingNode->idb(), dbNode, found)) {
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
        LOGW_SYNCPAL_DEBUG(
                _logger, L"Failed to get corresponding parent node: " << Utility::formatSyncName(syncOp->affectedNode()->name()));
        return ExitCode::DataError;
    }

    std::string localId = syncOp->targetSide() == ReplicaSide::Local ? correspondingNode->id().value_or("")
                                                                     : syncOp->affectedNode()->id().value_or("");
    std::string remoteId = syncOp->targetSide() == ReplicaSide::Local ? syncOp->affectedNode()->id().value_or("")
                                                                      : correspondingNode->id().value_or("");
    if (localId.empty() || remoteId.empty()) {
        LOGW_SYNCPAL_WARN(_logger, L"Empty " << (localId.empty() ? L"local" : L"remote") << L" id for item "
                                             << Utility::formatSyncName(syncOp->affectedNode()->name()));
        return ExitCode::DataError;
    }

    dbNode.setParentNodeId(parentNode->idb());
    dbNode.setNameLocal(syncOp->newName());
    dbNode.setNameRemote(syncOp->newName());
    if (syncOp->omit()) {
        dbNode.setStatus(SyncFileStatus::Success);
    }

    if (ParametersCache::isExtendedLogEnabled()) {
        LOGW_SYNCPAL_DEBUG(_logger, L"Updating DB: " << L" localName=" << Utility::quotedSyncName(syncOp->newName())
                                                     << L" / remoteName=" << Utility::quotedSyncName(syncOp->newName())
                                                     << L" / localId=" << CommonUtility::s2ws(localId) << L" / remoteId="
                                                     << CommonUtility::s2ws(remoteId) << L" / parent DB ID="
                                                     << dbNode.parentNodeId().value_or(-1) << L" / createdAt="
                                                     << syncOp->affectedNode()->createdAt().value_or(-1) << L" / lastModTime="
                                                     << syncOp->affectedNode()->modificationTime().value_or(-1) << L" / type="
                                                     << syncOp->affectedNode()->type());
    }

    if (!_syncPal->syncDb()->updateNode(dbNode, found)) {
        LOGW_SYNCPAL_WARN(_logger, L"Failed to update node into DB: "
                                           << L"local ID: " << CommonUtility::s2ws(localId) << L", remote ID: "
                                           << CommonUtility::s2ws(remoteId) << L", local name: "
                                           << Utility::quotedSyncName(syncOp->newName()) << L", remote name: "
                                           << Utility::quotedSyncName(syncOp->newName()) << L", parent DB ID: "
                                           << dbNode.parentNodeId().value_or(-1));
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

        correspondingNode->setName(syncOp->newName());

        if (!correspondingNode->setParentNode(parentNode)) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in Node::setParentNode: node name="
                                               << Utility::formatSyncName(parentNode->name()) << L" parent node name="
                                               << Utility::formatSyncName(correspondingNode->name()));
            return ExitCode::DataError;
        }

        if (!correspondingNode->parentNode()->insertChildren(correspondingNode)) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in Node::insertChildren: node name="
                                               << Utility::formatSyncName(correspondingNode->name()) << L" parent node name="
                                               << Utility::formatSyncName(correspondingNode->parentNode()->name()));
            return ExitCode::DataError;
        }
    }

    return ExitCode::Ok;
}

ExitInfo ExecutorWorker::propagateDeleteToDbAndTree(SyncOpPtr syncOp) {
    // avoids that the object(s) are detected again by compute_ops() on the next
    // sync iteration
    if (const auto exitInfo = deleteFromDb(syncOp->correspondingNode()); !exitInfo) {
        return exitInfo;
    }

    // Clear update tree
    if (!deleteOpNodes(syncOp)) {
        LOG_SYNCPAL_WARN(_logger, "Error in ExecutorWorker::deleteOpNodes");
        return ExitCode::DataError;
    }

    return ExitCode::Ok;
}

ExitInfo ExecutorWorker::deleteFromDb(std::shared_ptr<Node> node) {
    if (!node->idb().has_value()) {
        LOGW_SYNCPAL_WARN(_logger, L"Node " << Utility::formatSyncName(node->name()) << L" does not have a DB ID");
        return {ExitCode::DataError, ExitCause::DbEntryNotFound};
    }

    // Remove item (and children by cascade) from DB
    bool found = false;
    if (!_syncPal->syncDb()->deleteNode(*node->idb(), found)) {
        LOG_SYNCPAL_WARN(_logger, "Failed to remove node " << *node->idb() << " from DB");
        return {ExitCode::DbError, ExitCause::DbAccessError};
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "Node DB ID " << *node->idb() << " not found");
        return {ExitCode::DataError, ExitCause::DbEntryNotFound};
    }

    if (ParametersCache::isExtendedLogEnabled()) {
        LOGW_SYNCPAL_DEBUG(_logger, L"Item \"" << Utility::formatSyncName(node->name()) << L"\" removed from DB");
    }

    return ExitCode::Ok;
}

ExitInfo ExecutorWorker::runCreateDirJob(SyncOpPtr syncOp, std::shared_ptr<AbstractJob> job) {
    job->runSynchronously();

    std::string errorCode;
    auto tokenJob(std::dynamic_pointer_cast<AbstractTokenNetworkJob>(job));
    if (tokenJob && tokenJob->hasErrorApi(&errorCode)) {
        const auto code = getNetworkErrorCode(errorCode);
        if (code == NetworkErrorCode::DestinationAlreadyExists) {
            // Folder is already there, ignore this error
        } else if (code == NetworkErrorCode::ForbiddenError) {
            // The item should be blacklisted
            _syncPal->blacklistTemporarily(syncOp->affectedNode()->id().value_or(""), syncOp->affectedNode()->getPath(),
                                           ReplicaSide::Local);
            Error error(_syncPal->syncDbId(), syncOp->affectedNode()->id().value_or(""), "", syncOp->affectedNode()->type(),
                        syncOp->affectedNode()->getPath(), ConflictType::None, InconsistencyType::None, CancelType::None, "",
                        ExitCode::BackError, ExitCause::HttpErrForbidden);
            _syncPal->addError(error);

            // Clear update tree
            if (!deleteOpNodes(syncOp)) {
                LOG_SYNCPAL_WARN(_logger, "Error in ExecutorWorker::deleteOpNodes");
                return ExitCode::DataError;
            }

            return ExitCode::Ok;
        }
    }

    if (job->exitInfo().code() != ExitCode::Ok) {
        LOGW_SYNCPAL_WARN(_logger, L"Failed to create directory: " << Utility::formatSyncName(syncOp->affectedNode()->name()));
        return job->exitInfo();
    }

    NodeId newNodeId;
    SyncTime newCreationTime = 0;
    SyncTime newModificationTime = 0;
    if (syncOp->targetSide() == ReplicaSide::Local) {
        auto castJob(std::dynamic_pointer_cast<LocalCreateDirJob>(job));
        newNodeId = castJob->nodeId();
        newCreationTime = castJob->creationTime();
        newModificationTime = castJob->modtime();
    } else {
        auto castJob(std::dynamic_pointer_cast<CreateDirJob>(job));
        newNodeId = castJob->nodeId();
        newCreationTime = syncOp->affectedNode()->createdAt().value_or(0);
        newModificationTime = castJob->modtime();
    }

    if (newNodeId.empty()) {
        LOGW_SYNCPAL_WARN(_logger,
                          L"Failed to retrieve ID for directory: " << Utility::formatSyncName(syncOp->affectedNode()->name()));
        return {ExitCode::DataError, ExitCause::ApiErr};
    }

    std::shared_ptr<Node> newNode = nullptr;
    if (ExitInfo exitInfo = propagateCreateToDbAndTree(syncOp, newNodeId, newCreationTime, newModificationTime, newNode);
        !exitInfo) {
        LOGW_SYNCPAL_WARN(_logger, L"Failed to propagate changes in DB or update tree for: "
                                           << Utility::formatSyncName(syncOp->affectedNode()->name()) << L" " << exitInfo);
        return exitInfo;
    }

    return ExitCode::Ok;
}

void ExecutorWorker::cancelAllOngoingJobs() {
    LOG_SYNCPAL_DEBUG(_logger, "Cancelling all queued executor jobs");

    const std::scoped_lock lock(_opListMutex);

    // First, abort all jobs that are not running yet to avoid starting them for
    // nothing
    std::list<std::shared_ptr<AbstractJob>> remainingJobs;
    for (const auto &job: _ongoingJobs) {
        if (!job.second->isRunning()) {
            LOG_SYNCPAL_DEBUG(_logger, "Cancelling job: " << job.second->jobId());
            job.second->setAdditionalCallback(nullptr);
            job.second->abort();
        } else {
            remainingJobs.push_back(job.second);
        }
    }

    // Then cancel jobs that are currently running
    for (const auto &job: remainingJobs) {
        LOG_SYNCPAL_DEBUG(_logger, "Cancelling job: " << job->jobId());
        job->setAdditionalCallback(nullptr);
        job->abort();
    }
    _ongoingJobs.clear();
    _opList.clear();
    LOG_SYNCPAL_DEBUG(_logger, "All queued executor jobs cancelled.");
}

void ExecutorWorker::increaseErrorCount(const SyncOpPtr syncOp, const ExitInfo exitInfo /*= ExitInfo()*/) {
    if (exitInfo == ExitInfo(ExitCode::SystemError, ExitCause::SyncDirAccessError)) {
        return; // Ignore error if sync folder is not accessible.
    }

    if (syncOp->affectedNode() && syncOp->affectedNode()->id().has_value()) {
        _syncPal->increaseErrorCount(*syncOp->affectedNode()->id(), syncOp->affectedNode()->type(),
                                     syncOp->affectedNode()->getPath(), otherSide(syncOp->targetSide()), exitInfo);

        // Clear update tree
        if (!deleteOpNodes(syncOp)) {
            LOG_SYNCPAL_WARN(_logger, "Error in ExecutorWorker::deleteOpNodes");
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

bool ExecutorWorker::deleteOpNodes(const SyncOpPtr syncOp) {
    if (!affectedUpdateTree(syncOp)->deleteNode(syncOp->affectedNode())) {
        LOGW_SYNCPAL_WARN(_logger,
                          L"Error in UpdateTree::deleteNode: node " << Utility::formatSyncName(syncOp->affectedNode()->name()));
        return false;
    }

    if (syncOp->correspondingNode() && !targetUpdateTree(syncOp)->deleteNode(syncOp->correspondingNode())) {
        LOGW_SYNCPAL_WARN(_logger, L"Error in UpdateTree::deleteNode: node "
                                           << Utility::formatSyncName(syncOp->correspondingNode()->name()));
        return false;
    }

    return true;
}

ExitInfo ExecutorWorker::handleExecutorError(SyncOpPtr syncOp, const ExitInfo &opsExitInfo) {
    assert((syncOp && !opsExitInfo) && "syncOp is nullptr in ExecutorWorker::handleExecutorError");
    if (!syncOp) {
        LOG_SYNCPAL_WARN(_logger, "syncOp is nullptr in ExecutorWorker::handleExecutorError");
        return ExitCode::DataError;
    }
    if (opsExitInfo) {
        return opsExitInfo;
    }

    LOG_SYNCPAL_WARN(_logger, "Handling " << opsExitInfo << " in ExecutorWorker::handleExecutorError");

    if (opsExitInfo.cause() == ExitCause::NotFound || opsExitInfo.cause() == ExitCause::FileAccessError) {
        // Check that the root of the sync folder is still accessible
        bool exists = false;
        if (IoError ioError = IoError::Success; !IoHelper::checkIfPathExists(_syncPal->localPath(), exists, ioError)) {
            LOGW_WARN(_logger,
                      L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_syncPal->localPath(), ioError));
            return {ExitCode::SystemError, ExitCause::FileAccessError};
        }
        if (!exists) {
            LOGW_DEBUG(_logger, L"Sync dir " << Utility::formatSyncPath(_syncPal->localPath()) << L" not accessible anymore");
            _snapshotToInvalidate = true; // The snapshot must be invalidated before the next sync
            return {ExitCode::SystemError, ExitCause::SyncDirAccessError};
        }
    }

    // Handle specific errors
    switch (static_cast<int>(opsExitInfo)) {
        case static_cast<int>(ExitInfo(ExitCode::BackError, ExitCause::FileLocked)): {
            return handleOpsRemoteFileLocked(syncOp, opsExitInfo);
        }
        case static_cast<int>(ExitInfo(ExitCode::SystemError, ExitCause::FileAccessError)): {
            return handleOpsLocalFileAccessError(syncOp, opsExitInfo);
        }
        case static_cast<int>(ExitInfo(ExitCode::SystemError, ExitCause::NotFound)): {
            return handleOpsFileNotFound(syncOp, opsExitInfo);
        }
        case static_cast<int>(ExitInfo(ExitCode::BackError, ExitCause::FileExists)):
        case static_cast<int>(ExitInfo(ExitCode::SystemError, ExitCause::FileExists)):
        case static_cast<int>(ExitInfo(ExitCode::DataError, ExitCause::FileExists)): {
            return handleOpsAlreadyExistError(syncOp, opsExitInfo);
        }
        default: {
            break;
        }
    }
    LOG_SYNCPAL_WARN(_logger, "Unhandled error in ExecutorWorker::handleExecutorError: " << opsExitInfo);
    return opsExitInfo;
}

ExitInfo ExecutorWorker::handleOpsLocalFileAccessError(const SyncOpPtr syncOp, const ExitInfo &opsExitInfo) {
    std::shared_ptr<Node> localBlacklistedNode = nullptr;
    std::shared_ptr<Node> remoteBlacklistedNode = nullptr;
    if (syncOp->targetSide() == ReplicaSide::Local && syncOp->type() == OperationType::Create) {
        // The item does not exist yet locally, we will only tmpBlacklist the remote item
        if (ExitInfo exitInfo = _syncPal->handleAccessDeniedItem(syncOp->localCreationTargetPath(), localBlacklistedNode,
                                                                 remoteBlacklistedNode, opsExitInfo.cause());
            !exitInfo) {
            return exitInfo;
        }
    } else {
        // Both local and remote item will be temporarily blacklisted
        const auto localNode = syncOp->targetSide() == ReplicaSide::Remote ? syncOp->affectedNode() : syncOp->correspondingNode();
        if (!localNode) return ExitCode::LogicError;

        const SyncPath relativeLocalFilePath = localNode->getPath();
        if (ExitInfo exitInfo = _syncPal->handleAccessDeniedItem(relativeLocalFilePath, localBlacklistedNode,
                                                                 remoteBlacklistedNode, opsExitInfo.cause());
            !exitInfo) {
            return exitInfo;
        }
    }
    _syncPal->setRestart(true);
    return removeDependentOps(localBlacklistedNode, remoteBlacklistedNode, syncOp->type());
}

ExitInfo ExecutorWorker::handleOpsFileNotFound(const SyncOpPtr syncOp, [[maybe_unused]] const ExitInfo &opsExitInfo) {
    _syncPal->setRestart(true);
    _syncPal->tryToInvalidateSnapshots(); // There is a file/dir missing; we need to recompute the snapshot.
    return removeDependentOps(syncOp);
}

ExitInfo ExecutorWorker::handleOpsRemoteFileLocked(SyncOpPtr syncOp, const ExitInfo &opsExitInfo) {
    // Add error
    NodeId localNodeId;
    NodeId remoteNodeId;
    getNodeIdsFromOp(syncOp, localNodeId, remoteNodeId);
    const auto affectedPath = syncOp->affectedNode()->getPath();
    const auto error =
            Error(_syncPal->syncDbId(), localNodeId, remoteNodeId, syncOp->affectedNode()->type(), affectedPath,
                  ConflictType::None, InconsistencyType::None, CancelType::None, "", opsExitInfo.code(), opsExitInfo.cause());
    _syncPal->addError(error);

    // Blacklist the item
    if (!localNodeId.empty() && syncOp->localNode()) {
        _syncPal->blacklistTemporarily(localNodeId, syncOp->localNode()->getPath(), ReplicaSide::Local);
    }

    if (!remoteNodeId.empty() && syncOp->remoteNode()) {
        _syncPal->blacklistTemporarily(remoteNodeId, syncOp->remoteNode()->getPath(), ReplicaSide::Remote);
    }
    // Clear update tree
    if (!deleteOpNodes(syncOp)) {
        LOG_SYNCPAL_WARN(_logger, "Error in ExecutorWorker::deleteOpNodes");
        return ExitCode::DataError;
    }

    _syncPal->setRestart(true);
    return removeDependentOps(syncOp);
}

ExitInfo ExecutorWorker::handleOpsAlreadyExistError(const SyncOpPtr syncOp, const ExitInfo &opsExitInfo) {
    // If the item already exists either on local or remote side, we blacklist it.
    if (syncOp->type() != OperationType::Create &&
        syncOp->type() != OperationType::Move) { // The below handling is only for Create and Move/Rename operations.
        return opsExitInfo;
    }

    // Check if the local item is already blacklisted
    const auto affectedPath = syncOp->affectedNode()->getPath();
    LOGW_SYNCPAL_DEBUG(_logger, L"blacklisting item " << Utility::formatSyncPath(affectedPath));
    _syncPal->blacklistTemporarily(syncOp->affectedNode()->id().value(), affectedPath, syncOp->affectedNode()->side());

    if (syncOp->correspondingNode()) {
        const auto correspondingPath = syncOp->affectedNode()->getPath();
        LOGW_SYNCPAL_DEBUG(_logger, L"blacklisting item " << Utility::formatSyncPath(correspondingPath));
        _syncPal->blacklistTemporarily(syncOp->correspondingNode()->id().value(), correspondingPath,
                                       syncOp->correspondingNode()->side());
    }

    // Add error
    NodeId localNodeId;
    NodeId remoteNodeId;
    getNodeIdsFromOp(syncOp, localNodeId, remoteNodeId);
    const auto error =
            Error(_syncPal->syncDbId(), localNodeId, remoteNodeId, syncOp->affectedNode()->type(), affectedPath,
                  ConflictType::None, InconsistencyType::None, CancelType::None, "", opsExitInfo.code(), opsExitInfo.cause());
    _syncPal->addError(error);

    // Clear update tree
    if (!deleteOpNodes(syncOp)) {
        LOG_SYNCPAL_WARN(_logger, "Error in ExecutorWorker::deleteOpNodes");
        return ExitCode::DataError;
    }

    _syncPal->setRestart(true);
    return removeDependentOps(syncOp);
}

ExitInfo ExecutorWorker::removeDependentOps(const SyncOpPtr syncOp) {
    const auto localNode =
            syncOp->affectedNode()->side() == ReplicaSide::Local ? syncOp->affectedNode() : syncOp->correspondingNode();
    const auto remoteNode =
            syncOp->affectedNode()->side() == ReplicaSide::Remote ? syncOp->affectedNode() : syncOp->correspondingNode();

    return removeDependentOps(localNode, remoteNode, syncOp->type());
}

ExitInfo ExecutorWorker::removeDependentOps(const std::shared_ptr<Node> localNode, const std::shared_ptr<Node> remoteNode,
                                            const OperationType opType) {
    const std::scoped_lock lock(_opListMutex);

    std::list<UniqueId> dependentOps;
    for (const auto &opId: _opList) {
        const SyncOpPtr syncOp2 = _syncPal->_syncOps->getOp(opId);
        if (!syncOp2) {
            LOGW_SYNCPAL_WARN(_logger, L"Operation doesn't exist anymore: id=" << opId);
            continue;
        }
        const auto localNode2 =
                syncOp2->affectedNode()->side() == ReplicaSide::Local ? syncOp2->affectedNode() : syncOp2->correspondingNode();
        const auto remoteNode2 =
                syncOp2->affectedNode()->side() == ReplicaSide::Remote ? syncOp2->affectedNode() : syncOp2->correspondingNode();
        SyncName nodeName = localNode2 ? localNode2->name() : SyncName();
        if (nodeName.empty()) nodeName = remoteNode2 ? remoteNode2->name() : SyncName();

        if (localNode && localNode2 && (localNode->isParentOf(localNode2))) {
            LOGW_SYNCPAL_DEBUG(_logger, L"Removing " << syncOp2->type() << L" operation on " << Utility::formatSyncName(nodeName)
                                                     << L" because it depends on " << opType << L" operation on "
                                                     << Utility::formatSyncName(localNode->name()) << L" which failed.");

            dependentOps.push_back(opId);
            continue;
        }

        if (remoteNode && remoteNode2 && (remoteNode->isParentOf(remoteNode2))) {
            LOGW_SYNCPAL_DEBUG(_logger, L"Removing " << syncOp2->type() << L" operation on " << Utility::formatSyncName(nodeName)
                                                     << L" because it depends on " << opType << L" operation on "
                                                     << Utility::formatSyncName(remoteNode->name()) << L" which failed.");

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

void ExecutorWorker::getNodeIdsFromOp(SyncOpPtr syncOp, NodeId &localNodeId, NodeId &remoteNodeId) {
    if (syncOp->targetSide() == ReplicaSide::Local) {
        if (syncOp->correspondingNode() && syncOp->correspondingNode()->id()) localNodeId = *syncOp->correspondingNode()->id();
        remoteNodeId = syncOp->affectedNode()->id().value_or("");
    } else {
        localNodeId = syncOp->affectedNode()->id().value_or("");
        if (syncOp->correspondingNode() && syncOp->correspondingNode()->id()) remoteNodeId = *syncOp->correspondingNode()->id();
    }
}

} // namespace KDC
