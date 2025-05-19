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
#include "libcommon/utility/utility.h"
#include "libcommon/log/sentry/ptraces.h"
#include "libcommon/log/sentry/utility.h"
#include <log4cplus/loggingmacros.h>

#define UPDATE_PROGRESS_DELAY 1

namespace KDC {

SyncPalWorker::SyncPalWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName,
                             const std::chrono::seconds &startDelay) :
    ISyncWorker(syncPal, name, shortName, startDelay) {}

void SyncPalWorker::execute() {
    ExitCode exitCode(ExitCode::Unknown);
    LOG_SYNCPAL_INFO(_logger, "Worker " << name().c_str() << " started");
    if (_syncPal->vfsMode() != VirtualFileMode::Off) {
#ifdef _WIN32
        auto resetFunc = std::function<void()>([this]() { resetVfsFilesStatus(); });
        _resetVfsFilesStatusThread = StdLoggingThread(resetFunc);
#else
        resetVfsFilesStatus();
#endif
    }

    // Wait before really starting
    bool awakenByStop = false;
    sleepUntilStartDelay(awakenByStop);
    if (awakenByStop) {
        // Exit
        exitCode = ExitCode::Ok;
        setDone(exitCode);
        return;
    }

    // Sync loop
    LOG_SYNCPAL_DEBUG(_logger, "Start sync loop");
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
                    LOG_SYNCPAL_DEBUG(_logger, "Stop FSO worker " << index);
                    isFSOInProgress[index] = false;
                    stopAndWaitForExitOfWorker(fsoWorkers[index]);
                    bool shouldPause = fsoWorkers[index]->exitCode() == ExitCode::NetworkError ||
                                       (fsoWorkers[index]->exitCode() == ExitCode::BackError &&
                                        fsoWorkers[index]->exitCause() == ExitCause::ServiceUnavailable);
                    if (shouldPause && !pauseAsked()) {
                        pause();
                    }
                } else if (!pauseAsked()) {
                    LOG_SYNCPAL_DEBUG(_logger, "Start FSO worker " << index);
                    isFSOInProgress[index] = true;
                    fsoWorkers[index]->start();
                }
            }
        }

        // Manage stop
        if (stopAsked()) {
            // Stop all workers
            stopAndWaitForExitOfAllWorkers(fsoWorkers, stepWorkers);

            // Exit
            exitCode = ExitCode::Ok;
            break;
        }

        // Manage pause
        while ((_pauseAsked || _isPaused) &&
               (_step == SyncStep::Idle ||
                _step == SyncStep::Propagation2)) { // Pause only if we are idle or in Propagation2 (because it needs network)
            if (!_isPaused) {
                // Pause workers
                _pauseAsked = false;
                _isPaused = true;
                LOG_SYNCPAL_INFO(_logger, "***** Pause");
            }

            Utility::msleep(LOOP_PAUSE_SLEEP_PERIOD);

            // Manage unpause
            if (_unpauseAsked) {
                _isPaused = false;
                _unpauseAsked = false;
                LOG_SYNCPAL_INFO(_logger, "***** Resume");
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
                workersExitCode[index] = (stepWorkers[index] && !stepWorkers[index]->isRunning() ? stepWorkers[index]->exitCode()
                                                                                                 : ExitCode::Unknown);
            }

            if ((!stepWorkers[0] || workersExitCode[0] == ExitCode::Ok) &&
                (!stepWorkers[1] || workersExitCode[1] == ExitCode::Ok)) {
                // Next step
                SyncStep step = nextStep();
                if (step != _step) {
                    LOG_SYNCPAL_INFO(_logger, "***** Step " << stepName(_step).c_str() << " has finished");
                    waitForExitOfWorkers(stepWorkers);
                    initStep(step, stepWorkers, inputSharedObject);
                    isStepInProgress = false;
                }
            } else if ((stepWorkers[0] && workersExitCode[0] == ExitCode::NetworkError) ||
                       (stepWorkers[1] && workersExitCode[1] == ExitCode::NetworkError)) {
                LOG_SYNCPAL_INFO(_logger, "***** Step " << stepName(_step).c_str() << " has aborted");

                // Stop the step workers and pause sync
                stopAndWaitForExitOfWorkers(stepWorkers);
                isStepInProgress = false;
                pause();
                continue;
            } else if ((stepWorkers[0] && workersExitCode[0] == ExitCode::DataError) ||
                       (stepWorkers[1] && workersExitCode[1] == ExitCode::DataError) ||
                       (stepWorkers[0] && workersExitCode[0] == ExitCode::BackError) ||
                       (stepWorkers[1] && workersExitCode[1] == ExitCode::BackError) ||
                       (stepWorkers[0] && workersExitCode[0] == ExitCode::LogicError) ||
                       (stepWorkers[1] && workersExitCode[1] == ExitCode::LogicError)) {
                LOG_SYNCPAL_INFO(_logger, "***** Step " << stepName(_step).c_str() << " has aborted");

                // Stop the step workers and restart a full sync
                stopAndWaitForExitOfWorkers(stepWorkers);
                _syncPal->invalideSnapshots();
                initStepFirst(stepWorkers, inputSharedObject, true);
                continue;
            } else if ((stepWorkers[0] && workersExitCode[0] == ExitCode::DbError) ||
                       (stepWorkers[1] && workersExitCode[1] == ExitCode::DbError) ||
                       (stepWorkers[0] && workersExitCode[0] == ExitCode::SystemError) ||
                       (stepWorkers[1] && workersExitCode[1] == ExitCode::SystemError) ||
                       (stepWorkers[0] && workersExitCode[0] == ExitCode::UpdateRequired) ||
                       (stepWorkers[1] && workersExitCode[1] == ExitCode::UpdateRequired)) {
                LOG_SYNCPAL_INFO(_logger, "***** Step " << stepName(_step).c_str() << " has aborted");

                // Stop all workers and exit
                stopAndWaitForExitOfAllWorkers(fsoWorkers, stepWorkers);
                if ((stepWorkers[0] && workersExitCode[0] == ExitCode::SystemError) ||
                    (stepWorkers[1] && workersExitCode[1] == ExitCode::SystemError)) {
                    const auto exitCause = stepWorkers[0] ? stepWorkers[0]->exitCause() : stepWorkers[1]->exitCause();
                    if (exitCause == ExitCause::NotEnoughDiskSpace || exitCause == ExitCause::FileAccessError ||
                        exitCause == ExitCause::SyncDirAccesError || exitCause == ExitCause::SyncDirDoesntExist) {
                        // Exit without error
                        exitCode = ExitCode::Ok;
                        break;
                    }
                }
                if ((stepWorkers[0] && workersExitCode[0] == ExitCode::UpdateRequired) ||
                    (stepWorkers[1] && workersExitCode[1] == ExitCode::UpdateRequired)) {
                    exitCode = ExitCode::UpdateRequired;
                    break;
                }

                exitCode = ExitCode::FatalError;
                setExitCause(ExitCause::WorkerExited);
                break;
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

        if (exitCode != ExitCode::Unknown) {
            break;
        }

        Utility::msleep(LOOP_EXEC_SLEEP_PERIOD);
    }

    LOG_SYNCPAL_INFO(_logger, "Worker " << name().c_str() << " stoped");
    setDone(exitCode);
}
void SyncPalWorker::stop() {
    _pauseAsked = false;
    _unpauseAsked = true;
    ISyncWorker::stop();
    if (_resetVfsFilesStatusThread.joinable()) {
        _resetVfsFilesStatusThread.join();
    }
}

void SyncPalWorker::pause() {
    if (!isRunning()) {
        LOG_SYNCPAL_DEBUG(_logger, "Worker " << name() << " is not running");
        return;
    }
    _pauseTime = std::chrono::steady_clock::now();

    if (_isPaused || _pauseAsked) {
        LOG_SYNCPAL_DEBUG(_logger, "Worker " << name() << " is already paused");
        return;
    }

    LOG_SYNCPAL_DEBUG(_logger, "Worker " << name() << " pause");
    _pauseAsked = true;
    _unpauseAsked = false;
}

void SyncPalWorker::unpause() {
    if (!isRunning()) {
        LOG_SYNCPAL_DEBUG(_logger, "Worker " << name() << " is not running");
        return;
    }

    if ((!_isPaused && !_pauseAsked) || _unpauseAsked) {
        LOG_SYNCPAL_DEBUG(_logger, "Worker " << name() << " is already unpaused");
        return;
    }

    LOG_SYNCPAL_DEBUG(_logger, "Worker " << name() << " unpause");

    _unpauseAsked = true;
    _pauseAsked = false;
    _syncPal->setRestart(true);
}

std::string SyncPalWorker::stepName(SyncStep step) {
    return "<" + toString(step) + ">";
}

void SyncPalWorker::initStep(SyncStep step, std::shared_ptr<ISyncWorker> (&workers)[2],
                             std::shared_ptr<SharedObject> (&inputSharedObject)[2]) {
    if (step == SyncStep::UpdateDetection1) {
        sentry::pTraces::basic::Sync(syncDbId()).start();
    }
    sentry::syncStepToPTrace(_step, syncDbId())->stop();
    sentry::syncStepToPTrace(step, syncDbId())->start();

    _step = step;
    switch (step) {
        case SyncStep::Idle:
            workers[0] = nullptr;
            workers[1] = nullptr;
            inputSharedObject[0] = nullptr;
            inputSharedObject[1] = nullptr;
            _syncPal->resetEstimateUpdates();
            _syncPal->refreshTmpBlacklist();
            _syncPal->freeSnapshotsCopies();
            _syncPal->syncDb()->cache().clear();
            break;
        case SyncStep::UpdateDetection1:
            workers[0] = _syncPal->computeFSOperationsWorker();
            workers[1] = nullptr;
            _syncPal->copySnapshots();
            LOG_IF_FAIL(_syncPal->syncDb()->cache().reloadIfNeeded());
            assert(_syncPal->snapshotCopy(ReplicaSide::Local)->checkIntegrityRecursively() &&
                   "Local snapshot is corrupted, see logs for details");
            assert(_syncPal->snapshotCopy(ReplicaSide::Remote)->checkIntegrityRecursively() &&
                   "Remote snapshot is corrupted, see logs for details");
            inputSharedObject[0] = nullptr;
            inputSharedObject[1] = nullptr;
            _syncPal->setRestart(false);
            break;
        case SyncStep::UpdateDetection2:
            workers[0] = _syncPal->_localUpdateTreeWorker;
            workers[1] = _syncPal->_remoteUpdateTreeWorker;
            inputSharedObject[0] = _syncPal->operationSet(ReplicaSide::Local);
            inputSharedObject[1] = _syncPal->operationSet(ReplicaSide::Remote);
            break;
        case SyncStep::Reconciliation1:
            workers[0] = _syncPal->_platformInconsistencyCheckerWorker;
            workers[1] = nullptr;
            inputSharedObject[0] = _syncPal->updateTree(ReplicaSide::Remote);
            inputSharedObject[1] = nullptr;
            break;
        case SyncStep::Reconciliation2:
            workers[0] = _syncPal->_conflictFinderWorker;
            workers[1] = nullptr;
            inputSharedObject[0] = nullptr;
            inputSharedObject[1] = nullptr;
            break;
        case SyncStep::Reconciliation3:
            workers[0] = _syncPal->_conflictResolverWorker;
            workers[1] = nullptr;
            inputSharedObject[0] = nullptr;
            inputSharedObject[1] = nullptr;
            break;
        case SyncStep::Reconciliation4:
            workers[0] = _syncPal->_operationsGeneratorWorker;
            workers[1] = nullptr;
            inputSharedObject[0] = nullptr;
            inputSharedObject[1] = nullptr;
            break;
        case SyncStep::Propagation1:
            workers[0] = _syncPal->_operationsSorterWorker;
            workers[1] = nullptr;
            inputSharedObject[0] = nullptr;
            inputSharedObject[1] = nullptr;
            _syncPal->startEstimateUpdates();
            break;
        case SyncStep::Propagation2:
            workers[0] = _syncPal->_executorWorker;
            workers[1] = nullptr;
            inputSharedObject[0] = nullptr;
            inputSharedObject[1] = nullptr;
            _syncPal->syncDb()->cache().clear(); // Cache is not needed anymore, free resources
            break;
        case SyncStep::Done:
            workers[0] = nullptr;
            workers[1] = nullptr;
            inputSharedObject[0] = nullptr;
            inputSharedObject[1] = nullptr;
            _syncPal->stopEstimateUpdates();
            _syncPal->resetSnapshotInvalidationCounters();
            if (!_syncPal->restart()) {
                _syncPal->setSyncHasFullyCompletedInParms(true);
            }
            sentry::pTraces::basic::Sync(syncDbId()).stop();
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

    initStep(SyncStep::Idle, workers, inputSharedObject);
}

SyncStep SyncPalWorker::nextStep() const {
    switch (_step) {
        case SyncStep::Idle: {
            const bool areSnapshotsValid =
                    _syncPal->isSnapshotValid(ReplicaSide::Local) && _syncPal->isSnapshotValid(ReplicaSide::Remote);
            const bool areFSOWorkersRunning =
                    _syncPal->_localFSObserverWorker->isRunning() && _syncPal->_remoteFSObserverWorker->isRunning();
            const bool areFSOWorkersInitializing =
                    _syncPal->_localFSObserverWorker->initializing() || _syncPal->_remoteFSObserverWorker->initializing();
            const bool areFSOWorkersUpdating =
                    _syncPal->_localFSObserverWorker->updating() || _syncPal->_remoteFSObserverWorker->updating();
            const bool areSnapshotsUpdated =
                    _syncPal->snapshot(ReplicaSide::Local)->updated() || _syncPal->snapshot(ReplicaSide::Remote)->updated();


            return areSnapshotsValid && areFSOWorkersRunning && !areFSOWorkersInitializing && !areFSOWorkersUpdating &&
                                   (areSnapshotsUpdated || _syncPal->restart())
                           ? SyncStep::UpdateDetection1
                           : SyncStep::Idle;
        }
        case SyncStep::UpdateDetection1: {
            auto logNbOps = [this](const ReplicaSide side) {
                const auto opsSet = _syncPal->operationSet(side);
                LOG_SYNCPAL_DEBUG(_logger, opsSet->nbOps()
                                                   << " " << side << " operations detected (# CREATE: "
                                                   << opsSet->nbOpsByType(OperationType::Create)
                                                   << ", # EDIT: " << opsSet->nbOpsByType(OperationType::Edit)
                                                   << ", # MOVE: " << opsSet->nbOpsByType(OperationType::Move)
                                                   << ", # DELETE: " << opsSet->nbOpsByType(OperationType::Delete) << ")");
            };
            logNbOps(ReplicaSide::Local);
            logNbOps(ReplicaSide::Remote);

            if (CommonUtility::isFileSizeMismatchDetectionEnabled() &&
                !_syncPal->computeFSOperationsWorker()->getFileSizeMismatchMap().empty()) {
                _syncPal->fixCorruptedFile(_syncPal->computeFSOperationsWorker()->getFileSizeMismatchMap());
                _syncPal->setRestart(true);
                return SyncStep::Idle;
            }

            return (_syncPal->operationSet(ReplicaSide::Local)->updated() ||
                    _syncPal->operationSet(ReplicaSide::Remote)->updated())
                           ? SyncStep::UpdateDetection2
                           : SyncStep::Done;
        }
        case SyncStep::UpdateDetection2:
            return (_syncPal->updateTree(ReplicaSide::Local)->updated() || _syncPal->updateTree(ReplicaSide::Remote)->updated())
                           ? SyncStep::Reconciliation1
                           : SyncStep::Done;
        case SyncStep::Reconciliation1:
            return SyncStep::Reconciliation2;
        case SyncStep::Reconciliation2:
            LOG_SYNCPAL_DEBUG(_logger, _syncPal->_conflictQueue->size() << " conflicts found")
            return _syncPal->_conflictQueue->empty() ? SyncStep::Reconciliation4 : SyncStep::Reconciliation3;
        case SyncStep::Reconciliation3:
            return SyncStep::Propagation2; // Go directly to the Executor step
        case SyncStep::Reconciliation4:
            LOG_SYNCPAL_DEBUG(_logger, _syncPal->_syncOps->size() << " operations generated")
            return SyncStep::Propagation1;
        case SyncStep::Propagation1:
            return SyncStep::Propagation2;
        case SyncStep::Propagation2:
            return SyncStep::Done;
        case SyncStep::Done:
            return SyncStep::Idle;
        default:
            LOG_SYNCPAL_WARN(_logger, "Invalid status")
            return SyncStep::Idle;
    }
}

void SyncPalWorker::stopAndWaitForExitOfWorker(std::shared_ptr<ISyncWorker> worker) {
    worker->stop();
    worker->waitForExit();
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

void SyncPalWorker::resetVfsFilesStatus() {
    if (_syncPal->vfsMode() == VirtualFileMode::Off) return;
    sentry::pTraces::scoped::ResetStatus perfMonitor1(syncDbId());

    bool ok = true;
    try {
        std::error_code ec;
        auto dirIt = std::filesystem::recursive_directory_iterator(
                _syncPal->localPath(), std::filesystem::directory_options::skip_permission_denied, ec);
        if (ec) {
            LOGW_SYNCPAL_WARN(_logger, L"Error in resetVfsFilesStatus: " << Utility::formatStdError(ec).c_str());
            return;
        }
        for (; dirIt != std::filesystem::recursive_directory_iterator(); ++dirIt) {
            if (stopAsked()) {
                LOGW_SYNCPAL_DEBUG(_logger, L"Stop asked in resetVfsFilesStatus");
                return;
            }
            SyncPath absolutePath;
            try {
                if (dirIt->is_directory()) {
                    continue;
                }
                absolutePath = dirIt->path();
            } catch (std::filesystem::filesystem_error &) {
                dirIt.disable_recursion_pending();
                continue;
            }

            // Check if the directory entry is managed
            bool isManaged = true;
            IoError ioError = IoError::Success;
            if (!Utility::checkIfDirEntryIsManaged(*dirIt, isManaged, ioError)) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in Utility::checkIfDirEntryIsManaged : "
                                                   << Utility::formatSyncPath(absolutePath).c_str());
                ok = false;
                dirIt.disable_recursion_pending();
                continue;
            }

            if (ioError == IoError::NoSuchFileOrDirectory) {
                LOGW_SYNCPAL_DEBUG(_logger,
                                   L"Directory entry does not exist anymore : " << Utility::formatSyncPath(absolutePath).c_str());
                dirIt.disable_recursion_pending();
                continue;
            }

            if (ioError == IoError::AccessDenied) {
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

            VfsStatus vfsStatus;
            if (ExitInfo exitInfo = _syncPal->vfs()->status(dirIt->path(), vfsStatus); !exitInfo) {
                LOGW_SYNCPAL_WARN(_logger,
                                  L"Error in vfsStatus : " << Utility::formatSyncPath(dirIt->path()) << L": " << exitInfo);
                ok = false;
                dirIt.disable_recursion_pending();
                continue;
            }

            if (!vfsStatus.isPlaceholder) continue;

            const PinState pinState = _syncPal->vfs()->pinState(dirIt->path());
#ifndef _WIN32 // Handle by the API on windows.
            if (vfsStatus.isSyncing) {
                // Force status to dehydrate
                if (const ExitInfo exitInfo = _syncPal->vfs()->forceStatus(dirIt->path(), VfsStatus()); !exitInfo) {
                    LOGW_SYNCPAL_WARN(_logger, L"Error in vfsForceStatus : " << Utility::formatSyncPath(dirIt->path()) << L": "
                                                                             << exitInfo);
                    ok = false;
                    dirIt.disable_recursion_pending();
                    continue;
                }
                vfsStatus.isHydrated = false;
            }

            bool hydrationOrDehydrationInProgress = false;
            const SyncPath relativePath =
                    CommonUtility::relativePath(_syncPal->localPath(), dirIt->path()); // Get the relative path of the file
            (void) _syncPal->fileSyncing(ReplicaSide::Local, relativePath, hydrationOrDehydrationInProgress);
            if (hydrationOrDehydrationInProgress) {
                _syncPal->vfs()->cancelHydrate(
                        dirIt->path()); // Cancel any (de)hydration that could still be in progress on the OS side.
            }
#endif
            // Fix hydration state if needed.
            if ((vfsStatus.isHydrated && pinState == PinState::OnlineOnly) ||
                (!vfsStatus.isHydrated && pinState == PinState::AlwaysLocal)) {
                if (!_syncPal->vfs()->fileStatusChanged(dirIt->path(), SyncFileStatus::Syncing)) {
                    LOGW_SYNCPAL_WARN(_logger, L"Error in vfsSetPinState : " << Utility::formatSyncPath(dirIt->path()).c_str());
                    ok = false;
                    dirIt.disable_recursion_pending();
                    continue;
                }
            }
        }
    } catch (std::filesystem::filesystem_error &e) {
        LOG_SYNCPAL_WARN(_logger,
                         "Error caught in SyncPalWorker::resetVfsFilesStatus: code=" << e.code() << " error=" << e.what());
        ok = false;
    } catch (...) {
        LOG_SYNCPAL_WARN(_logger, "Error caught in SyncPalWorker::resetVfsFilesStatus");
        ok = false;
    }

    if (ok) {
        if (!_syncPal->syncDb()->updateNodesSyncing(false)) {
            LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::updateNodesSyncing for syncDbId=" << _syncPal->syncDbId());
        }
        LOG_SYNCPAL_DEBUG(_logger, "VFS files status reset");
        perfMonitor1.stop();
    } else {
        LOG_SYNCPAL_WARN(_logger, "Error in resetVfsFilesStatus");
    }
    return;
}

} // namespace KDC
