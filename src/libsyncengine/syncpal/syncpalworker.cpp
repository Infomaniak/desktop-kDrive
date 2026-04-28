/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/io/filestat.h"
#include "libcommon/utility/utility.h"
#include "libcommon/log/sentry/ptraces.h"
#include "libcommon/log/sentry/utility.h"
#include <log4cplus/loggingmacros.h>

#define UPDATE_PROGRESS_DELAY 1
namespace KDC {

SyncPalWorker::SyncPalWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName,
                             const std::chrono::seconds &startDelay) :
    ISyncWorker(syncPal, name, shortName, startDelay) {}

namespace {

bool hasSuccessfullyFinished(const std::shared_ptr<ISyncWorker> w1, const std::shared_ptr<ISyncWorker> w2 = nullptr) {
    return (!w1 || w1->exitCode() == ExitCode::Ok) && (!w2 || w2->exitCode() == ExitCode::Ok);
}

bool shouldBeStoppedAndRestarted(const std::shared_ptr<ISyncWorker> w1, const std::shared_ptr<ISyncWorker> w2 = nullptr) {
    const auto dataError = (w1 && w1->exitCode() == ExitCode::DataError) || (w2 && w2->exitCode() == ExitCode::DataError);
    const auto backError = (w1 && w1->exitCode() == ExitCode::BackError) || (w2 && w2->exitCode() == ExitCode::BackError);
    const auto logicError = (w1 && w1->exitCode() == ExitCode::LogicError) || (w2 && w2->exitCode() == ExitCode::LogicError);
    return dataError || backError || logicError;
}

bool shouldBeStopped(const std::shared_ptr<ISyncWorker> w1, const std::shared_ptr<ISyncWorker> w2 = nullptr) {
    const auto dbError = (w1 && w1->exitCode() == ExitCode::DbError) || (w2 && w2->exitCode() == ExitCode::DbError);
    const auto systemError = (w1 && w1->exitCode() == ExitCode::SystemError) || (w2 && w2->exitCode() == ExitCode::SystemError);
    const auto updateRequired =
            (w1 && w1->exitCode() == ExitCode::UpdateRequired) || (w2 && w2->exitCode() == ExitCode::UpdateRequired);
    const auto invalidSyncError =
            (w1 && w1->exitCode() == ExitCode::InvalidSync) || (w2 && w2->exitCode() == ExitCode::InvalidSync);
    const auto invalidToken =
            (w1 && w1->exitCode() == ExitCode::InvalidToken) || (w2 && w2->exitCode() == ExitCode::InvalidToken);
    const auto driveNotFound = (w1 && w1->exitCode() == ExitCode::BackError && w1->exitCause() == ExitCause::DriveAccessError) ||
                               (w2 && w2->exitCode() == ExitCode::BackError && w2->exitCause() == ExitCause::DriveAccessError);

    return dbError || systemError || updateRequired || invalidSyncError || invalidToken || driveNotFound;
}

} // namespace

bool SyncPalWorker::shouldBePaused(const std::shared_ptr<ISyncWorker> w1, const std::shared_ptr<ISyncWorker> w2 /*= nullptr*/) {
    resetPauseDuration();

    const auto networkIssue =
            (w1 && w1->exitCode() == ExitCode::NetworkError) || (w2 && w2->exitCode() == ExitCode::NetworkError);
    const auto httpBlockingError =
            (w1 && w1->exitCode() == ExitCode::BackError &&
             (w1->exitCause() == ExitCause::Http5xx || w1->exitCause() == ExitCause::HttpErr ||
              w1->exitCause() == ExitCause::FullListParsingError || w1->exitCause() == ExitCause::MissingReplyData)) ||
            (w2 && w2->exitCode() == ExitCode::BackError &&
             (w2->exitCause() == ExitCause::Http5xx || w2->exitCause() == ExitCause::HttpErr ||
              w2->exitCause() == ExitCause::FullListParsingError || w2->exitCause() == ExitCause::MissingReplyData));
    const auto syncDirNotAccessible =
            (w1 && w1->exitCode() == ExitCode::SystemError &&
             (w1->exitCause() == ExitCause::SyncDirAccessError || w1->exitCause() == ExitCause::SyncDirDiskMissing)) ||
            (w2 && w2->exitCode() == ExitCode::SystemError &&
             (w2->exitCause() == ExitCause::SyncDirAccessError || w2->exitCause() == ExitCause::SyncDirDiskMissing));
    const auto invalidOperation =
            (w1 && w1->exitCode() == ExitCode::InvalidOperation) || (w2 && w2->exitCode() == ExitCode::InvalidOperation);

    if ((w1 && w1->exitCode() == ExitCode::RateLimited) || (w2 && w2->exitCode() == ExitCode::RateLimited)) {
        const auto newPauseDuration =
                std::max(w1 ? w1->pauseDuration() : defaultPauseDuration, w2 ? w2->pauseDuration() : defaultPauseDuration);
        if (newPauseDuration != pauseDuration()) {
            LOG_SYNCPAL_INFO(_logger, "Changing pause duration to " << newPauseDuration << " ms");
            setPauseDuration(newPauseDuration);
        }
        return true;
    }

    return networkIssue || httpBlockingError || syncDirNotAccessible || invalidOperation;
}

void SyncPalWorker::execute() {
    ExitCode exitCode(ExitCode::Unknown);
    LOG_SYNCPAL_INFO(_logger, "Worker " << name() << " started");
    if (_syncPal->vfsMode() != VirtualFileMode::Off) {
#if defined(KD_WINDOWS)
        auto resetFunc = std::function<void()>([this]() { resetVfsFilesStatus(); });
        _resetVfsFilesStatusThread = std::make_unique<StdLoggingThread>(resetFunc);
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
        bool syncDirChanged = false;
        // Check File System Observer workers status
        for (int index = 0; index < 2; index++) {
            if (fsoWorkers[index] && !fsoWorkers[index]->isRunning()) {
                if (isFSOInProgress[index]) {
                    LOG_SYNCPAL_DEBUG(_logger, "Stop FSO worker " << index);
                    isFSOInProgress[index] = false;
                    stopAndWaitForExitOfWorker(fsoWorkers[index]);
                    if (shouldBePaused(fsoWorkers[index], nullptr) && !pauseAsked()) {
                        pause();
                        continue;
                    }

                    syncDirChanged = fsoWorkers[index]->exitCode() == ExitCode::DataError &&
                                     fsoWorkers[index]->exitCause() == ExitCause::SyncDirChanged;
                    if (syncDirChanged) {
                        break;
                    }

                    if (shouldBeStopped(fsoWorkers[index], nullptr) && !stopAsked()) {
                        stop();
                    }
                } else if (!pauseAsked()) {
                    LOG_SYNCPAL_DEBUG(_logger, "Start FSO worker " << index);
                    isFSOInProgress[index] = true;
                    fsoWorkers[index]->start();
                }
            }
        }

        // Manage SyncDir change (might happen if the sync folder is deleted and recreated e.g migration from an other device)
        if (syncDirChanged && !tryToFixDbNodeIdsAfterSyncDirChange()) {
            LOG_SYNCPAL_INFO(_logger,
                             "Sync dir changed and we are unable to automaticaly fix syncDb, stopping all workers and exiting");
            stopAndWaitForExitOfAllWorkers(fsoWorkers, stepWorkers);
            exitCode = ExitCode::DataError;
            setExitCause(ExitCause::SyncDirChanged);
            break;
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
            if (hasSuccessfullyFinished(stepWorkers[0], stepWorkers[1])) {
                // Next step
                const SyncStep step = nextStep();
                if (step != _step) {
                    LOG_SYNCPAL_INFO(_logger, "***** Step " << stepName(_step) << " has finished");
                    waitForExitOfWorkers(stepWorkers);
                    initStep(step, stepWorkers, inputSharedObject);
                    isStepInProgress = false;
                }
            } else if (shouldBePaused(stepWorkers[0], stepWorkers[1])) {
                LOG_SYNCPAL_INFO(_logger, "***** Step " << stepName(_step) << " has aborted");

                // Stop the step workers and pause sync
                stopAndWaitForExitOfWorkers(stepWorkers);
                isStepInProgress = false;
                pause();
                continue;
            } else if (shouldBeStoppedAndRestarted(stepWorkers[0], stepWorkers[1])) {
                LOG_SYNCPAL_INFO(_logger, "***** Step " << stepName(_step) << " has aborted");

                // Stop the step workers and restart a full sync
                stopAndWaitForExitOfWorkers(stepWorkers);
                _syncPal->tryToInvalidateSnapshots();
                initStepFirst(stepWorkers, inputSharedObject, true);
                continue;
            } else if (shouldBeStopped(stepWorkers[0], stepWorkers[1])) {
                LOG_SYNCPAL_INFO(_logger, "***** Step " << stepName(_step) << " has aborted");

                // Stop all workers and exit
                stopAndWaitForExitOfAllWorkers(fsoWorkers, stepWorkers);
                if ((stepWorkers[0] && stepWorkers[0]->exitCode() == ExitCode::SystemError &&
                     (stepWorkers[0]->exitCause() == ExitCause::NotEnoughDiskSpace ||
                      stepWorkers[0]->exitCause() == ExitCause::FileAccessError ||
                      stepWorkers[0]->exitCause() == ExitCause::TmpDirAccessError ||
                      stepWorkers[0]->exitCause() == ExitCause::SyncDirAccessError ||
                      stepWorkers[0]->exitCause() == ExitCause::SyncDirDiskMissing)) ||
                    (stepWorkers[1] && stepWorkers[1]->exitCode() == ExitCode::SystemError &&
                     (stepWorkers[1]->exitCause() == ExitCause::NotEnoughDiskSpace ||
                      stepWorkers[1]->exitCause() == ExitCause::FileAccessError ||
                      stepWorkers[1]->exitCause() == ExitCause::TmpDirAccessError ||
                      stepWorkers[1]->exitCause() == ExitCause::SyncDirAccessError ||
                      stepWorkers[1]->exitCause() == ExitCause::SyncDirDiskMissing))) {
                    // Exit without error
                    exitCode = ExitCode::Ok;
                } else if ((stepWorkers[0] && stepWorkers[0]->exitCode() == ExitCode::UpdateRequired) ||
                           (stepWorkers[1] && stepWorkers[1]->exitCode() == ExitCode::UpdateRequired)) {
                    exitCode = ExitCode::UpdateRequired;
                } else {
                    exitCode = ExitCode::FatalError;
                    setExitCause(ExitCause::WorkerExited);
                }
                break;
            }
        } else {
            // Start workers
            LOG_SYNCPAL_INFO(_logger, "***** Step " << stepName(_step) << " start");
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

    LOG_SYNCPAL_INFO(_logger, "Worker " << name() << " stopped");
    setDone(exitCode);
}

void SyncPalWorker::stop() {
    _pauseAsked = false;
    _unpauseAsked = true;
    ISyncWorker::stop();
#if defined(KD_WINDOWS)
    if (_resetVfsFilesStatusThread && _resetVfsFilesStatusThread->joinable()) {
        _resetVfsFilesStatusThread->join();
    }
#endif
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
            if (!_syncPal->restart()) {
                _syncPal->resetSnapshotInvalidationCounters();
                _syncPal->setSyncHasFullyCompletedInParams(true);
            }
            if (_syncPal->updateTreesNeedToBeCleared()) {
                LOG_SYNCPAL_DEBUG(_logger, "Clearing update trees");
                _syncPal->_localUpdateTree->clear();
                _syncPal->_remoteUpdateTree->clear();
                _syncPal->setUpdateTreesNeedToBeCleared(false);
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
            const bool areLiveSnapshotsValid =
                    _syncPal->liveSnapshot(ReplicaSide::Local).isValid() && _syncPal->liveSnapshot(ReplicaSide::Remote).isValid();
            const bool areFSOWorkersRunning =
                    _syncPal->_localFSObserverWorker->isRunning() && _syncPal->_remoteFSObserverWorker->isRunning();
            const bool areFSOWorkersInitializing =
                    _syncPal->_localFSObserverWorker->initializing() || _syncPal->_remoteFSObserverWorker->initializing();
            const bool areFSOWorkersUpdating =
                    _syncPal->_localFSObserverWorker->updating() || _syncPal->_remoteFSObserverWorker->updating();
            const bool areLiveSnapshotsUpdated =
                    _syncPal->liveSnapshot(ReplicaSide::Local).updated() || _syncPal->liveSnapshot(ReplicaSide::Remote).updated();


            return areLiveSnapshotsValid && areFSOWorkersRunning && !areFSOWorkersInitializing && !areFSOWorkersUpdating &&
                                   (areLiveSnapshotsUpdated || _syncPal->restart())
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
bool SyncPalWorker::tryToFixDbNodeIdsAfterSyncDirChange() {
    LOG_SYNCPAL_INFO(_logger, "Trying to fix SyncDb node IDs after sync dir nodeId change...");

    NodeId newLocalRootNodeId;
    if (!IoHelper::getNodeId(_syncPal->localPath(), newLocalRootNodeId)) {
        LOGW_SYNCPAL_WARN(_logger,
                          L"Unable to get new local node ID for " << Utility::formatSyncPath(_syncPal->localPath()) << L".");
        sentry::Handler::instance()->captureMessage(KDC::sentry::Level::Warning, "Failed to get new local node ID for sync dir",
                                                    "Sync Dir migration failure");
        return false;
    }

    if (!_syncPal->syncDb()->tryToFixDbNodeIdsAfterSyncDirChange(_syncPal->localPath())) {
        LOGW_SYNCPAL_WARN(_logger, L"SyncDb could not be fixed after sync dir change.");
        sentry::Handler::instance()->captureMessage(
                KDC::sentry::Level::Warning, "Failed to fix SyncDb node IDs after sync dir change", "Sync Dir migration failure");
        return false;
    }

    if (const ExitInfo exitInfo = _syncPal->setLocalNodeId(newLocalRootNodeId); !exitInfo) {
        LOGW_SYNCPAL_WARN(_logger, L"Error in setLocalNodeId: " << exitInfo);
        sentry::Handler::instance()->captureMessage(KDC::sentry::Level::Warning,
                                                    "Failed to set new local node ID after sync dir change",
                                                    "Sync Dir migration failure");
        return false;
    }
 
    _syncPal->resolveSyncErrorsByExitCause(ExitCause::SyncDirChanged);

    LOG_SYNCPAL_INFO(_logger, "SyncDb successfully fixed after sync dir change, new local node ID is " << newLocalRootNodeId);
    sentry::Handler::instance()->captureMessage(KDC::sentry::Level::Info, "SyncDb successfully fixed after sync dir change",
                                                "Sync Dir migration success");
    return true;
}

bool SyncPalWorker::isLocalItemInSyncWithDb(const SyncPath &localAbsolutePath, std::optional<NodeId> &outLocalNodeId) {
    // Ideally, this logic should be shared with ComputeFSOperationWorker::inferChangeFromDbNode,
    // but for now it would require some refactoring to make it reusable, so we duplicate it here.
    outLocalNodeId.reset();
    FileStat fileStat;
    IoError ioError = IoError::Success;
    if (!IoHelper::getFileStat(localAbsolutePath, &fileStat, ioError, IoHelper::PathCheckOption::Insensitive)) {
        LOGW_SYNCPAL_WARN(_logger, L"Error in IoHelper::getFileStat: " << Utility::formatIoError(localAbsolutePath, ioError));
        return false;
    }
    if (ioError == IoError::NoSuchFileOrDirectory) {
        LOGW_SYNCPAL_WARN(_logger, L"Item does not exist anymore: " << Utility::formatSyncPath(localAbsolutePath));
        return false;
    } else if (ioError == IoError::AccessDenied) {
        LOGW_SYNCPAL_WARN(_logger, L"Item misses search permission: " << Utility::formatSyncPath(localAbsolutePath));
        return false;
    } else if (ioError != IoError::Success) {
        LOGW_SYNCPAL_WARN(_logger, L"Error accessing item: " << Utility::formatSyncPath(localAbsolutePath) << L": "
                                                             << Utility::formatIoError(ioError));
        return false;
    }

    outLocalNodeId = std::to_string(fileStat.inode);

    DbNode dbNode;
    bool found = false;
    if (!_syncPal->syncDb()->node(ReplicaSide::Local, std::to_string(fileStat.inode), dbNode, found)) {
        LOGW_SYNCPAL_WARN(_logger, L"Error in SyncDb::node for " << Utility::formatSyncPath(localAbsolutePath));
        return false;
    }

    if (!found) {
        return false;
    }

    if (dbNode.type() != fileStat.nodeType) {
        return false;
    }

    SyncPath localDbRelativePath;
    SyncPath remoteDbRelativePath;
    if (!_syncPal->syncDb()->path(dbNode.nodeId(), localDbRelativePath, remoteDbRelativePath, found)) {
        LOGW_SYNCPAL_WARN(_logger, L"Error in SyncDb::path for DbNodeID " << dbNode.nodeId());
        return false;
    }

    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "dbNodeId " << dbNode.nodeId() << " not found in SyncDb");
        return false;
    }

    const SyncPath localRelativePath = CommonUtility::relativePath(_syncPal->localPath(), localAbsolutePath);

    if (localDbRelativePath.lexically_normal() != localRelativePath.lexically_normal()) {
        return false;
    }

    if (fileStat.nodeType == NodeType::Directory) {
        return true;
    }

    if (dbNode.size() == fileStat.size && dbNode.lastModifiedLocal() == fileStat.modificationTime &&
        (!dbNode.created().has_value() || dbNode.created().value() == fileStat.creationTime)) {
        return true;
    }

    return false;
}

void SyncPalWorker::resetVfsFilesStatus() {
    if (_syncPal->vfsMode() == VirtualFileMode::Off) return;
    sentry::pTraces::scoped::ResetStatus perfMonitor1(syncDbId());

    bool ok = true;
    IoError ioError = IoError::Success;
    IoHelper::DirectoryIterator dirIt;
    bool endOfDir = false;
    DirectoryEntry entry;
    try {
        if (!IoHelper::recursiveDirectoryIterator(_syncPal->localPath(), dirIt)) {
            LOGW_WARN(_logger, L"Error in IoHelper::recursiveDirectoryIterator.");
            return;
        }

        while (dirIt.next(entry, endOfDir, ioError) && !endOfDir) {
            if (stopAsked()) {
                LOGW_SYNCPAL_DEBUG(_logger, L"Stop asked in resetVfsFilesStatus");
                return;
            }
            SyncPath absolutePath;
            try {
                absolutePath = entry.path();
                std::optional<NodeId> localNodeId;

                if (!entry.is_symlink() && entry.is_directory()) {
#ifdef KD_WINDOWS
                    if (isLocalItemInSyncWithDb(absolutePath, localNodeId)) {
                        // Fix directories sync status if needed to avoid having directories in incorrect Syncing status.
                        VfsStatus status;
                        status.isSyncing = false;
                        if (const ExitInfo exitInfo = _syncPal->vfs()->forceStatus(entry.path(), status); !exitInfo) {
                            LOGW_SYNCPAL_WARN(_logger, L"Error in vfsForceStatus : " << Utility::formatSyncPath(entry.path())
                                                                                     << L": " << exitInfo);
                        }
                    }
#endif // KD_WINDOWS
                    continue;
                }
            } catch (std::filesystem::filesystem_error &) {
                dirIt.disableRecursionPending();
                continue;
            }

            // Check if the directory entry is managed
            bool isManaged = true;
            auto managedEntryError = IoError::Success;
            if (!Utility::checkIfDirEntryIsManaged(entry, isManaged, managedEntryError)) {
                LOGW_SYNCPAL_WARN(_logger,
                                  L"Error in Utility::checkIfDirEntryIsManaged : " << Utility::formatSyncPath(absolutePath));
                ok = false;
                dirIt.disableRecursionPending();
                continue;
            }

            if (managedEntryError == IoError::NoSuchFileOrDirectory) {
                LOGW_SYNCPAL_DEBUG(_logger,
                                   L"Directory entry does not exist anymore : " << Utility::formatSyncPath(absolutePath));
                dirIt.disableRecursionPending();
                continue;
            }

            if (managedEntryError == IoError::AccessDenied) {
                LOGW_SYNCPAL_DEBUG(_logger, L"Directory misses search permission : " << Utility::formatSyncPath(absolutePath));
                dirIt.disableRecursionPending();
                continue;
            }

            if (!isManaged) {
                LOGW_SYNCPAL_DEBUG(_logger, L"Directory entry is not managed : " << Utility::formatSyncPath(absolutePath));
                dirIt.disableRecursionPending();
                continue;
            }

            VfsStatus vfsStatus;
            if (const auto exitInfo = _syncPal->vfs()->status(entry.path(), vfsStatus); !exitInfo) {
                LOGW_SYNCPAL_WARN(_logger,
                                  L"Error in vfsStatus : " << Utility::formatSyncPath(entry.path()) << L": " << exitInfo);
                ok = false;
                dirIt.disableRecursionPending();
                continue;
            }

            if (!vfsStatus.isPlaceholder) {
#if defined(KD_WINDOWS)
                // Due to a bug introduced in kDrive 3.8.2.5, some files may have been reverted to regular files
                // (i.e., no longer placeholders) while still being considered in sync with the database.
                // As a corrective measure, we reconvert such files to placeholders when necessary to avoid misleading syncing
                // status

                SyncFileItem syncItem;
                std::optional<NodeId> localNodeId;
                if (!entry.is_symlink() && isLocalItemInSyncWithDb(absolutePath, localNodeId) && localNodeId.has_value()) {
                    syncItem.setLocalNodeId(localNodeId.value());
                    if (ExitInfo exitInfo = _syncPal->vfs()->convertToPlaceholder(absolutePath, syncItem); !exitInfo) {
                        LOGW_SYNCPAL_WARN(_logger, L"Error in vfsConvertToPlaceholder : " << Utility::formatSyncPath(absolutePath)
                                                                                          << L": " << exitInfo);
                    }
                }
                continue;
#else
                continue;
#endif // KD_WINDOWS
            }

            const PinState pinState = _syncPal->vfs()->pinState(entry.path());
#ifndef KD_WINDOWS // Handle by the API on windows.
            if (vfsStatus.isSyncing) {
                // Force status to dehydrate
                if (const ExitInfo exitInfo = _syncPal->vfs()->forceStatus(entry.path(), VfsStatus()); !exitInfo) {
                    LOGW_SYNCPAL_WARN(
                            _logger, L"Error in vfsForceStatus : " << Utility::formatSyncPath(entry.path()) << L": " << exitInfo);
                    ok = false;
                    dirIt.disableRecursionPending();
                    continue;
                }
                vfsStatus.isHydrated = false;
            }

            bool hydrationOrDehydrationInProgress = false;
            const SyncPath relativePath =
                    CommonUtility::relativePath(_syncPal->localPath(), entry.path()); // Get the relative path of the file
            (void) _syncPal->fileSyncing(ReplicaSide::Local, relativePath, hydrationOrDehydrationInProgress);
            if (hydrationOrDehydrationInProgress) {
                _syncPal->vfs()->cancelHydrate(
                        entry.path()); // Cancel any (de)hydration that could still be in progress on the OS side.
            }
#endif
            // Fix hydration state if needed.
            if ((vfsStatus.isHydrated && pinState == PinState::OnlineOnly) ||
                (!vfsStatus.isHydrated && pinState == PinState::AlwaysLocal)) {
                if (!_syncPal->vfs()->fileStatusChanged(entry.path(), SyncFileStatus::Syncing)) {
                    LOGW_SYNCPAL_WARN(_logger, L"Error in fileStatusChanged: " << Utility::formatSyncPath(entry.path()));
                    ok = false;
                    dirIt.disableRecursionPending();
                    continue;
                }
            }
        }
    } catch (std::filesystem::filesystem_error &e) {
        LOG_SYNCPAL_WARN(_logger,
                         "Exception caught in SyncPalWorker::resetVfsFilesStatus: code=" << e.code() << " error=" << e.what());
        ok = false;
    } catch (...) {
        LOG_SYNCPAL_WARN(_logger, "Exception caught in SyncPalWorker::resetVfsFilesStatus");
        ok = false;
    }

    if (!endOfDir || ioError != IoError::Success) {
        LOGW_WARN(_logger, L"Error in IoHelper::DirectoryIterator causing early interruption: "
                                   << Utility::formatIoError(entry.path(), ioError));
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
}

} // namespace KDC
