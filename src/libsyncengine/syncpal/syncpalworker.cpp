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

#include "syncpalworker.h"
#include "update_detection/file_system_observer/filesystemobserverworker.h"
#include "update_detection/file_system_observer/computefsoperationworker.h"
#include "update_detection/update_detector/updatetreeworker.h"
#include "reconciliation/platform_inconsistency_checker/platforminconsistencycheckerworker.h"
#include "reconciliation/conflict_finder/conflictfinderworker.h"
#include "reconciliation/conflict_resolver/conflictresolverworker.h"
#include "reconciliation/operation_generator/operationgeneratorworker.h"
#include "propagation/operation_sorter/operationsorterworker.h"
#include "propagation/executor/executorworker.h"
#include "libcommonserver/utility/utility.h"

#include <log4cplus/loggingmacros.h>

#define UPDATE_PROGRESS_DELAY 1

namespace KDC {

SyncPalWorker::SyncPalWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName)
    : ISyncWorker(syncPal, name, shortName),
      _step(SyncStepIdle),
      _pauseTime(std::chrono::time_point<std::chrono::system_clock>()) {}

void SyncPalWorker::execute() {
    ExitCode exitCode(ExitCodeUnknown);

    LOG_SYNCPAL_INFO(_logger, "Worker " << name().c_str() << " started");

    if (_syncPal->_vfsMode != VirtualFileModeOff) {
        // Reset vfs files status
        if (!resetVfsFilesStatus()) {
            LOG_SYNCPAL_WARN(_logger, "Error in resetVfsFilesStatus for syncDbId=" << _syncPal->syncDbId());
        }

        if (_syncPal->_vfsMode == VirtualFileModeMac) {
            // Reset nodes syncing flag
            if (!_syncPal->_syncDb->updateNodesSyncing(false)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::updateNodesSyncing for syncDbId=" << _syncPal->syncDbId());
            }
        }
    }

    // Sync loop
    bool isFSOInProgress[2] = {false, false};
    std::shared_ptr<ISyncWorker> fsoWorkers[2] = {_syncPal->_localFSObserverWorker, _syncPal->_remoteFSObserverWorker};
    bool isStepInProgress = false;
    std::shared_ptr<ISyncWorker> stepWorkers[2] = {nullptr, nullptr};
    std::shared_ptr<SharedObject> inputSharedObject[2] = {nullptr, nullptr};
    time_t lastEstimateUpdate = 0;
    for (;;) {
        // Check File System Observer workers status
        for (int index = 0; index < 2; index++) {
            if (fsoWorkers[index] && !fsoWorkers[index]->isRunning()) {
                if (isFSOInProgress[index]) {
                    // Pause sync
                    LOG_SYNCPAL_DEBUG(_logger, "Stop FSO worker " << index);
                    isFSOInProgress[index] = false;
                    fsoWorkers[index]->stop();
                    pause();
                } else {
                    // Start worker
                    LOG_SYNCPAL_DEBUG(_logger, "Start FSO worker " << index);
                    isFSOInProgress[index] = true;
                    fsoWorkers[index]->start();
                    if (isPaused()) {
                        unpause();
                    }
                }
            }
        }

        // Manage stop
        if (stopAsked()) {
            // Stop all workers
            stopAndWaitForExitOfAllWorkers(fsoWorkers, stepWorkers);

            // Exit
            exitCode = ExitCodeOk;
            break;
        }

        // Manage pause
        while (pauseAsked() || isPaused()) {
            if (!isPaused()) {
                // Pause workers
                _pauseTime = std::chrono::system_clock::now();
                pauseAllWorkers(stepWorkers);
            }

            Utility::msleep(LOOP_PAUSE_SLEEP_PERIOD);

            // Manage unpause
            if (unpauseAsked()) {
                // Unpause workers
                unpauseAllWorkers(stepWorkers);
            }
        }

        // Manage progress info
        time_t currentTime = std::time(nullptr);
        if (currentTime > lastEstimateUpdate + UPDATE_PROGRESS_DELAY) {
            _syncPal->updateEstimates();
            lastEstimateUpdate = currentTime;
        }

        if (isStepInProgress) {
            // Check workers status
            ExitCode workersExitCode[2];
            for (int index = 0; index < 2; index++) {
                workersExitCode[index] =
                    (stepWorkers[index] && !stepWorkers[index]->isRunning() ? stepWorkers[index]->exitCode() : ExitCodeUnknown);
            }

            if ((!stepWorkers[0] || workersExitCode[0] == ExitCodeOk) && (!stepWorkers[1] || workersExitCode[1] == ExitCodeOk)) {
                // Next step
                SyncStep step = nextStep();
                if (step != _step) {
                    LOG_SYNCPAL_INFO(_logger, "***** Step " << stepName(_step).c_str() << " has finished");
                    initStep(step, stepWorkers, inputSharedObject);
                    isStepInProgress = false;
                }
            } else if ((stepWorkers[0] && workersExitCode[0] == ExitCodeNetworkError) ||
                       (stepWorkers[1] && workersExitCode[1] == ExitCodeNetworkError)) {
                LOG_SYNCPAL_INFO(_logger, "***** Step " << stepName(_step).c_str() << " has aborted");

                // Stop the step workers and pause sync
                stopAndWaitForExitOfWorkers(stepWorkers);
                isStepInProgress = false;
                pause();
                continue;
            } else if ((stepWorkers[0] && workersExitCode[0] == ExitCodeDataError) ||
                       (stepWorkers[1] && workersExitCode[1] == ExitCodeDataError) ||
                       (stepWorkers[0] && workersExitCode[0] == ExitCodeBackError) ||
                       (stepWorkers[1] && workersExitCode[1] == ExitCodeBackError) ||
                       (stepWorkers[0] && workersExitCode[0] == ExitCodeLogicError) ||
                       (stepWorkers[1] && workersExitCode[1] == ExitCodeLogicError)) {
                LOG_SYNCPAL_INFO(_logger, "***** Step " << stepName(_step).c_str() << " has aborted");

                // Stop the step workers and restart a full sync
                stopAndWaitForExitOfWorkers(stepWorkers);
                _syncPal->_localFSObserverWorker->invalidateSnapshot();
                _syncPal->_remoteFSObserverWorker->forceUpdate();
                _syncPal->_remoteFSObserverWorker->invalidateSnapshot();
                initStepFirst(stepWorkers, inputSharedObject, true);
                continue;
            } else if ((stepWorkers[0] && workersExitCode[0] == ExitCodeDbError) ||
                       (stepWorkers[1] && workersExitCode[1] == ExitCodeDbError) ||
                       (stepWorkers[0] && workersExitCode[0] == ExitCodeSystemError) ||
                       (stepWorkers[1] && workersExitCode[1] == ExitCodeSystemError) ||
                       (stepWorkers[0] && workersExitCode[0] == ExitCodeUpdateRequired) ||
                       (stepWorkers[1] && workersExitCode[1] == ExitCodeUpdateRequired)) {
                LOG_SYNCPAL_INFO(_logger, "***** Step " << stepName(_step).c_str() << " has aborted");

                // Stop all workers and exit
                stopAndWaitForExitOfAllWorkers(fsoWorkers, stepWorkers);
                if ((stepWorkers[0] && workersExitCode[0] == ExitCodeSystemError &&
                     (stepWorkers[0]->exitCause() == ExitCauseNotEnoughDiskSpace ||
                      stepWorkers[0]->exitCause() == ExitCauseFileAccessError)) ||
                    (stepWorkers[1] && workersExitCode[1] == ExitCodeSystemError &&
                     (stepWorkers[1]->exitCause() == ExitCauseNotEnoughDiskSpace ||
                      stepWorkers[1]->exitCause() == ExitCauseFileAccessError))) {
                    // Exit without error
                    exitCode = ExitCodeOk;
                } else if ((stepWorkers[0] && workersExitCode[0] == ExitCodeUpdateRequired) ||
                           (stepWorkers[1] && workersExitCode[1] == ExitCodeUpdateRequired)) {
                    exitCode = ExitCodeUpdateRequired;
                } else {
                    exitCode = ExitCodeFatalError;
                    setExitCause(ExitCauseWorkerExited);
                }
                break;
            } else if (interruptCondition()) {
                LOG_SYNCPAL_INFO(_logger, "***** Step " << stepName(_step).c_str() << " interruption");

                // Stop the step workers and restart a sync
                stopAndWaitForExitOfWorkers(stepWorkers);
                initStepFirst(stepWorkers, inputSharedObject, false);
                continue;
            }
        } else {
            // Start workers
            LOG_SYNCPAL_INFO(_logger, "***** Step " << stepName(_step).c_str() << " start");
            isStepInProgress = true;
            for (int index = 0; index < 2; index++) {
                if (inputSharedObject[index]) {
                    inputSharedObject[index]->startRead();
                }
                if (stepWorkers[index]) {
                    stepWorkers[index]->start();
                }
            }
        }

        if (exitCode != ExitCodeUnknown) {
            break;
        }

        Utility::msleep(LOOP_EXEC_SLEEP_PERIOD);
    }

    setDone(exitCode);
    LOG_SYNCPAL_INFO(_logger, "Worker " << name().c_str() << " stoped");
}

std::string SyncPalWorker::stepName(SyncStep step) {
    std::string name;

    name = "<";

    switch (step) {
        case SyncStepNone:
            name += "None";
            break;
        case SyncStepIdle:
            name += "Idle";
            break;
        case SyncStepUpdateDetection1:
            name += "Compute FS operations";
            break;
        case SyncStepUpdateDetection2:
            name += "Update Trees";
            break;
        case SyncStepReconciliation1:
            name += "Platform Inconsistency Checker";
            break;
        case SyncStepReconciliation2:
            name += "Conflict Finder";
            break;
        case SyncStepReconciliation3:
            name += "Conflict Resolver";
            break;
        case SyncStepReconciliation4:
            name += "Operation Generator";
            break;
        case SyncStepPropagation1:
            name += "Sorter";
            break;
        case SyncStepPropagation2:
            name += "Executor";
            break;
        case SyncStepDone:
            name += "Done";
            break;
    }

    name += ">";

    return name;
}

void SyncPalWorker::initStep(SyncStep step, std::shared_ptr<ISyncWorker> (&workers)[2],
                             std::shared_ptr<SharedObject> (&inputSharedObject)[2]) {
    _step = step;

    switch (step) {
        case SyncStepIdle:
            workers[0] = nullptr;
            workers[1] = nullptr;
            inputSharedObject[0] = nullptr;
            inputSharedObject[1] = nullptr;
            _syncPal->resetEstimateUpdates();
            _syncPal->refreshTmpBlacklist();
            break;
        case SyncStepUpdateDetection1:
            workers[0] = _syncPal->_computeFSOperationsWorker;
            workers[1] = nullptr;
            _syncPal->copySnapshots();
            inputSharedObject[0] = _syncPal->snapshot(ReplicaSideLocal, true);
            inputSharedObject[1] = _syncPal->snapshot(ReplicaSideRemote, true);
            _syncPal->_restart = false;
            break;
        case SyncStepUpdateDetection2:
            workers[0] = _syncPal->_localUpdateTreeWorker;
            workers[1] = _syncPal->_remoteUpdateTreeWorker;
            inputSharedObject[0] = _syncPal->operationSet(ReplicaSideLocal);
            inputSharedObject[1] = _syncPal->operationSet(ReplicaSideRemote);
            break;
        case SyncStepReconciliation1:
            workers[0] = _syncPal->_platformInconsistencyCheckerWorker;
            workers[1] = nullptr;
            inputSharedObject[0] = _syncPal->updateTree(ReplicaSideRemote);
            inputSharedObject[1] = nullptr;
            break;
        case SyncStepReconciliation2:
            workers[0] = _syncPal->_conflictFinderWorker;
            workers[1] = nullptr;
            inputSharedObject[0] = nullptr;
            inputSharedObject[1] = nullptr;
            break;
        case SyncStepReconciliation3:
            workers[0] = _syncPal->_conflictResolverWorker;
            workers[1] = nullptr;
            inputSharedObject[0] = nullptr;
            inputSharedObject[1] = nullptr;
            break;
        case SyncStepReconciliation4:
            workers[0] = _syncPal->_operationsGeneratorWorker;
            workers[1] = nullptr;
            inputSharedObject[0] = nullptr;
            inputSharedObject[1] = nullptr;
            break;
        case SyncStepPropagation1:
            workers[0] = _syncPal->_operationsSorterWorker;
            workers[1] = nullptr;
            inputSharedObject[0] = nullptr;
            inputSharedObject[1] = nullptr;
            _syncPal->startEstimateUpdates();
            break;
        case SyncStepPropagation2:
            workers[0] = _syncPal->_executorWorker;
            workers[1] = nullptr;
            inputSharedObject[0] = nullptr;
            inputSharedObject[1] = nullptr;
            break;
        case SyncStepDone:
            workers[0] = nullptr;
            workers[1] = nullptr;
            inputSharedObject[0] = nullptr;
            inputSharedObject[1] = nullptr;
            _syncPal->stopEstimateUpdates();
            if (!_syncPal->_restart) {
                _syncPal->setSyncHasFullyCompleted(true);
            }
            break;
        default:
            LOG_SYNCPAL_WARN(_logger, "Invalid status");
            workers[0] = nullptr;
            workers[1] = nullptr;
            inputSharedObject[0] = nullptr;
            inputSharedObject[1] = nullptr;
            break;
    }
}

void SyncPalWorker::initStepFirst(std::shared_ptr<ISyncWorker> (&workers)[2],
                                  std::shared_ptr<SharedObject> (&inputSharedObject)[2], bool reset) {
    LOG_SYNCPAL_DEBUG(_logger, "Restart sync");

    if (reset) {
        _syncPal->resetSharedObjects();
    }

    *_syncPal->_interruptSync = false;

    initStep(SyncStepIdle, workers, inputSharedObject);
}

bool SyncPalWorker::interruptCondition() const {
    switch (_step) {
        case SyncStepIdle:
            return false;
            break;
        case SyncStepUpdateDetection1:
        case SyncStepUpdateDetection2:
        case SyncStepReconciliation1:
        case SyncStepReconciliation2:
        case SyncStepReconciliation3:
        case SyncStepReconciliation4:
        case SyncStepPropagation1:
        case SyncStepPropagation2:
        case SyncStepDone:
            return _syncPal->interruptSync();
            break;
        default:
            LOG_SYNCPAL_WARN(_logger, "Invalid status");
            return false;
            break;
    }
}

SyncStep SyncPalWorker::nextStep() const {
    switch (_step) {
        case SyncStepIdle:
            return (_syncPal->isSnapshotValid(ReplicaSideLocal) && _syncPal->isSnapshotValid(ReplicaSideRemote) &&
                    !_syncPal->_localFSObserverWorker->updating() && !_syncPal->_remoteFSObserverWorker->updating() &&
                    (_syncPal->snapshot(ReplicaSideLocal)->updated() || _syncPal->snapshot(ReplicaSideRemote)->updated() ||
                     _syncPal->_restart))
                       ? SyncStepUpdateDetection1
                       : SyncStepIdle;
            break;
        case SyncStepUpdateDetection1: {
            auto logNbOps = [=](const ReplicaSide side) {
                auto opsSet = _syncPal->operationSet(side);
                LOG_SYNCPAL_DEBUG(_logger, opsSet->nbOps()
                                               << " " << Utility::side2Str(side).c_str()
                                               << " operations detected (# CREATE: " << opsSet->nbOpsByType(OperationTypeCreate)
                                               << ", # EDIT: " << opsSet->nbOpsByType(OperationTypeEdit)
                                               << ", # MOVE: " << opsSet->nbOpsByType(OperationTypeMove)
                                               << ", # DELETE: " << opsSet->nbOpsByType(OperationTypeDelete) << ")");
            };
            logNbOps(ReplicaSideLocal);
            logNbOps(ReplicaSideRemote);

            if (!_syncPal->_computeFSOperationsWorker->getFileSizeMismatchMap().empty()) {
                _syncPal->fixCorruptedFile(_syncPal->_computeFSOperationsWorker->getFileSizeMismatchMap());
                _syncPal->_restart = true;
                return SyncStepIdle;
            }

            return (_syncPal->operationSet(ReplicaSideLocal)->updated() || _syncPal->operationSet(ReplicaSideRemote)->updated())
                       ? SyncStepUpdateDetection2
                       : SyncStepDone;
            break;
        }
        case SyncStepUpdateDetection2:
            return (_syncPal->updateTree(ReplicaSideLocal)->updated() || _syncPal->updateTree(ReplicaSideRemote)->updated())
                       ? SyncStepReconciliation1
                       : SyncStepDone;
            break;
        case SyncStepReconciliation1:
            return SyncStepReconciliation2;
            break;
        case SyncStepReconciliation2:
            LOG_SYNCPAL_DEBUG(_logger, _syncPal->_conflictQueue->size() << " conflicts found");
            return _syncPal->_conflictQueue->empty() ? SyncStepReconciliation4 : SyncStepReconciliation3;
            break;
        case SyncStepReconciliation3:
        case SyncStepReconciliation4:
            LOG_SYNCPAL_DEBUG(_logger, _syncPal->_syncOps->size() << " operations generated");
            return _syncPal->_conflictQueue->empty() ? SyncStepPropagation1 : SyncStepPropagation2;
            break;
        case SyncStepPropagation1:
            return SyncStepPropagation2;
            break;
        case SyncStepPropagation2:
            return SyncStepDone;
            break;
        case SyncStepDone:
            return SyncStepIdle;
            break;
        default:
            LOG_SYNCPAL_WARN(_logger, "Invalid status");
            return SyncStepIdle;
            break;
    }
}

void SyncPalWorker::stopWorkers(std::shared_ptr<ISyncWorker> workers[2]) {
    for (int index = 0; index < 2; index++) {
        if (workers[index]) {
            workers[index]->stop();
        }
    }
}

void SyncPalWorker::waitForExitOfWorkers(std::shared_ptr<ISyncWorker> workers[2]) {
    for (int index = 0; index < 2; index++) {
        if (workers[index]) {
            workers[index]->waitForExit();
        }
    }
}

void SyncPalWorker::stopAndWaitForExitOfWorkers(std::shared_ptr<ISyncWorker> workers[2]) {
    stopWorkers(workers);
    waitForExitOfWorkers(workers);
}

void SyncPalWorker::stopAndWaitForExitOfAllWorkers(std::shared_ptr<ISyncWorker> fsoWorkers[2],
                                                   std::shared_ptr<ISyncWorker> stepWorkers[2]) {
    LOG_SYNCPAL_INFO(_logger, "***** Stop");

    // Stop workers
    stopWorkers(fsoWorkers);
    stopWorkers(stepWorkers);

    // Wait for workers to exit
    waitForExitOfWorkers(fsoWorkers);
    waitForExitOfWorkers(stepWorkers);

    LOG_SYNCPAL_INFO(_logger, "***** Stop done");
}

void SyncPalWorker::pauseAllWorkers(std::shared_ptr<ISyncWorker> workers[2]) {
    LOG_SYNCPAL_INFO(_logger, "***** Pause");

    for (int index = 0; index < 2; index++) {
        if (workers[index]) {
            workers[index]->pause();
        }
    }

    setPauseDone();
    LOG_SYNCPAL_INFO(_logger, "***** Pause done");
}

void SyncPalWorker::unpauseAllWorkers(std::shared_ptr<ISyncWorker> workers[2]) {
    LOG_SYNCPAL_INFO(_logger, "***** Resume");

    for (int index = 0; index < 2; index++) {
        if (workers[index]) {
            workers[index]->unpause();
        }
    }

    setUnpauseDone();
    LOG_SYNCPAL_INFO(_logger, "***** Resume done");
}

bool SyncPalWorker::resetVfsFilesStatus() {
    bool ok = true;
    try {
        std::error_code ec;
        auto dirIt = std::filesystem::recursive_directory_iterator(
            _syncPal->_localPath, std::filesystem::directory_options::skip_permission_denied, ec);
        if (ec) {
            LOGW_SYNCPAL_WARN(_logger, "Error in resetVfsFilesStatus: " << Utility::formatStdError(ec).c_str());
            return false;
        }
        for (; dirIt != std::filesystem::recursive_directory_iterator(); ++dirIt) {
#ifdef _WIN32
            // skip_permission_denied doesn't work on Windows
            try {
                bool dummy = dirIt->exists();
                (void)(dummy);
            } catch (std::filesystem::filesystem_error &) {
                dirIt.disable_recursion_pending();
                continue;
            }
#endif

            const SyncPath absolutePath = dirIt->path();

            // Check if the directory entry is managed
            bool isManaged = true;
            bool isLink = false;
            IoError ioError = IoErrorSuccess;
            if (!Utility::checkIfDirEntryIsManaged(dirIt, isManaged, isLink, ioError)) {
                LOGW_SYNCPAL_WARN(
                    _logger, L"Error in Utility::checkIfDirEntryIsManaged : " << Utility::formatSyncPath(absolutePath).c_str());
                dirIt.disable_recursion_pending();
                ok = false;
                continue;
            }

            if (ioError == IoErrorNoSuchFileOrDirectory) {
                LOGW_SYNCPAL_DEBUG(_logger,
                                   L"Directory entry does not exist anymore : " << Utility::formatSyncPath(absolutePath).c_str());
                dirIt.disable_recursion_pending();
                continue;
            }

            if (ioError == IoErrorAccessDenied) {
                LOGW_SYNCPAL_DEBUG(_logger,
                                   L"Directory misses search permission : " << Utility::formatSyncPath(absolutePath).c_str());
                dirIt.disable_recursion_pending();
                continue;
            }

            if (!isManaged) {
                LOGW_SYNCPAL_DEBUG(_logger,
                                   L"Directory entry is not managed : " << Utility::formatSyncPath(absolutePath).c_str());
                dirIt.disable_recursion_pending();
                continue;
            }

            if (dirIt->is_directory()) {
                continue;
            }

            bool isPlaceholder = false;
            bool isHydrated = false;
            bool isSyncing = false;
            int progress = 0;
            if (!_syncPal->vfsStatus(dirIt->path(), isPlaceholder, isHydrated, isSyncing, progress)) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in vfsStatus : " << Utility::formatSyncPath(dirIt->path()).c_str());
                ok = false;
                continue;
            }

            PinState pinState;
            if (!_syncPal->vfsPinState(dirIt->path(), pinState)) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in vfsPinState : " << Utility::formatSyncPath(dirIt->path()).c_str());
                ok = false;
                continue;
            }

            if (isPlaceholder) {
                if (isSyncing) {
                    // Force status to dehydrated
                    if (!_syncPal->vfsForceStatus(dirIt->path(), false, 0, false)) {
                        LOGW_SYNCPAL_WARN(_logger,
                                          L"Error in vfsForceStatus : " << Utility::formatSyncPath(dirIt->path()).c_str());
                        ok = false;
                        continue;
                    }
                    isHydrated = false;
                }

                // Fix pinstate if needed
                if (isHydrated && pinState != PinStateAlwaysLocal) {
                    if (!_syncPal->vfsSetPinState(dirIt->path(), PinStateAlwaysLocal)) {
                        LOGW_SYNCPAL_WARN(_logger,
                                          L"Error in vfsSetPinState : " << Utility::formatSyncPath(dirIt->path()).c_str());
                        ok = false;
                        continue;
                    }
                } else if (!isHydrated && pinState != PinStateOnlineOnly) {
                    if (!_syncPal->vfsSetPinState(dirIt->path(), PinStateOnlineOnly)) {
                        LOGW_SYNCPAL_WARN(_logger,
                                          L"Error in vfsSetPinState : " << Utility::formatSyncPath(dirIt->path()).c_str());
                        ok = false;
                        continue;
                    }
                }
            } else {
                if (pinState == PinStateAlwaysLocal || pinState == PinStateOnlineOnly) {
                    if (!_syncPal->vfsSetPinState(dirIt->path(), PinStateUnspecified)) {
                        LOGW_SYNCPAL_WARN(_logger,
                                          L"Error in vfsSetPinState : " << Utility::formatSyncPath(dirIt->path()).c_str());
                        ok = false;
                        continue;
                    }
                }
            }
        }
    } catch (std::filesystem::filesystem_error &e) {
        LOG_SYNCPAL_WARN(_logger, "Error caught in SyncPalWorker::resetVfsFilesStatus: " << e.code() << " - " << e.what());
        ok = false;
    } catch (...) {
        LOG_SYNCPAL_WARN(_logger, "Error caught in SyncPalWorker::resetVfsFilesStatus");
        ok = false;
    }

    return ok;
}

}  // namespace KDC
