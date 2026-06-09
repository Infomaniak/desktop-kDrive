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
#include "update_detection/file_system_observer/remotefilesystemobserverworker.h"
#include "update_detection/file_system_observer/computefsoperationworker.h"
#include "update_detection/update_detector/updatetreeworker.h"
#include "reconciliation/platform_inconsistency_checker/platforminconsistencycheckerworker.h"
#include "reconciliation/conflict_finder/conflictfinderworker.h"
#include "reconciliation/conflict_resolver/conflictresolverworker.h"
#include "reconciliation/operation_generator/operationgeneratorworker.h"
#include "propagation/operation_sorter/operationsorterworker.h"
#include "propagation/executor/executorworker.h"
#include "requests/syncnodecache.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/io/filestat.h"
#include "libcommon/utility/utility.h"
#include "libcommon/log/sentry/ptraces.h"
#include "libcommon/log/sentry/utility.h"

#include <log4cplus/loggingmacros.h>

#include <cmath>
#include "useractionscopedlock.h"

#define UPDATE_PROGRESS_DELAY 1

namespace KDC {

static constexpr auto snapshotMinSizeForDeleteAlert = 100; // 100 items

SyncPalWorker::SyncPalWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName,
                             const std::chrono::seconds &startDelay) :
    ISyncWorker(syncPal, name, shortName, startDelay) {}

namespace {

bool hasSuccessfullyFinished(const SyncPalWorker::ReplicaWorkers &workers) {
    for (const auto side: {ReplicaSide::Local, ReplicaSide::Remote, ReplicaSide::Both}) {
        if (workers.contains(side) && workers.at(side).worker && workers.at(side).worker->exitCode() != ExitCode::Ok)
            return false;
    }

    return true;
}

bool shouldBeStoppedAndRestarted(const SyncPalWorker::ReplicaWorkers &workers) {
    const std::unordered_set<ExitCode> interruptingExitCodes = {ExitCode::DataError, ExitCode::BackError, ExitCode::LogicError};

    for (const auto side: {ReplicaSide::Local, ReplicaSide::Remote, ReplicaSide::Both}) {
        if (workers.contains(side) && workers.at(side).worker &&
            interruptingExitCodes.contains(workers.at(side).worker->exitCode())) {
            return true;
        }
    }

    return false;
}

bool shouldBeStopped(const SyncPalWorker::ReplicaWorkers &workers) {
    const std::unordered_set<ExitCode> stoppingExitCodes = {ExitCode::DbError, ExitCode::UpdateRequired, ExitCode::InvalidSync,
                                                            ExitCode::InvalidToken};

    for (const auto side: {ReplicaSide::Local, ReplicaSide::Remote, ReplicaSide::Both}) {
        const bool hasWorker = workers.contains(side) && workers.at(side).worker;

        if (hasWorker && stoppingExitCodes.contains(workers.at(side).worker->exitCode())) return true;

        if (hasWorker && workers.at(side).worker->exitCode() == ExitCode::BackError &&
            workers.at(side).worker->exitCause() == ExitCause::DriveAccessError) {
            return true;
        }
    }

    return false;
}

bool shouldExitWithoutError(const SyncPalWorker::ReplicaWorkers &workers) {
    const std::unordered_set<ExitCause> exitCausesWithoutConsequences = {
            ExitCause::NotEnoughDiskSpace, ExitCause::FileAccessError, ExitCause::TmpDirAccessError,
            ExitCause::SyncDirAccessError, ExitCause::SyncDirDiskMissing};

    for (const auto side: {ReplicaSide::Local, ReplicaSide::Remote, ReplicaSide::Both}) {
        if (workers.contains(side) && workers.at(side).worker && workers.at(side).worker->exitCode() == ExitCode::SystemError &&
            exitCausesWithoutConsequences.contains(workers.at(side).worker->exitCause())) {
            return true;
        }
    }

    return false;
}

} // namespace

bool SyncPalWorker::shouldBePaused(const SyncPalWorker::ReplicaWorkers &workers) {
    const std::unordered_set<ExitCause> blockingExitCause = {ExitCause::Http5xx, ExitCause::HttpErr, ExitCause::MissingReplyData};
    const std::unordered_set<ExitCause> syncDirNotAccessibleExitCauses = {ExitCause::SyncDirAccessError,
                                                                          ExitCause::SyncDirDiskMissing};
    resetPauseDuration();

    if (handleRateLimited(workers)) return true;

    bool networkIssue = false;
    bool httpBlockingError = false;
    bool syncDirNotAccessible = false;
    bool invalidOperation = false;
    bool rfsoError = false;

    for (const auto side: {ReplicaSide::Local, ReplicaSide::Remote, ReplicaSide::Both}) {
        const bool hasWorker = workers.contains(side) && workers.at(side).worker;
        if (!hasWorker) continue;

        auto worker = workers.at(side).worker;

        if (worker->exitCode() == ExitCode::NetworkError) networkIssue = true;
        if (worker->exitCode() == ExitCode::BackError && blockingExitCause.contains(worker->exitCause())) {
            httpBlockingError = true;
        }

        if (worker->exitCode() == ExitCode::SystemError &&
            syncDirNotAccessibleExitCauses.contains(workers.at(side).worker->exitCause())) {
            syncDirNotAccessible = true;
        }

        if (worker->exitCode() == ExitCode::InvalidOperation) invalidOperation = true;

        if (const auto rfsoWorker = std::dynamic_pointer_cast<RemoteFileSystemObserverWorker>(worker);
            rfsoWorker && rfsoWorker->exitCode() != ExitCode::Ok) {
            rfsoError = true;
        }
    }

    if (httpBlockingError || rfsoError) {
        handleBackError();
        return true;
    }

    return networkIssue || syncDirNotAccessible || invalidOperation;
}

bool SyncPalWorker::handleRateLimited(const SyncPalWorker::ReplicaWorkers &workers) {
    auto newPauseDuration = pauseDuration();
    bool shouldHandleRateLimit = false;

    for (const auto side: {ReplicaSide::Local, ReplicaSide::Remote, ReplicaSide::Both}) {
        if (const bool hasWorker = workers.contains(side) && workers.at(side).worker; !hasWorker) continue;

        if (workers.at(side).worker->exitCode() == ExitCode::RateLimited) shouldHandleRateLimit = true;

        newPauseDuration = std::max(newPauseDuration, workers.at(side).worker->pauseDuration());
    }

    if (shouldHandleRateLimit && newPauseDuration != pauseDuration()) {
        LOG_SYNCPAL_INFO(_logger, "Changing pause duration to " << newPauseDuration << " ms due to rate limiting");
        setPauseDuration(newPauseDuration);
    }

    return shouldHandleRateLimit;
}

bool shouldRequireUpdate(const SyncPalWorker::ReplicaWorkers &workers) {
    for (const auto side: {ReplicaSide::Local, ReplicaSide::Remote, ReplicaSide::Both}) {
        if (workers.contains(side) && workers.at(side).worker &&
            workers.at(side).worker->exitCode() == ExitCode::UpdateRequired) {
            return true;
        }
    }

    return false;
}


void SyncPalWorker::handleBackError() {
    auto computedDelay = static_cast<int64_t>(
            backoffVariable::baseDelay *
            std::pow(backoffVariable::multiplicativeFactor, std::min(_syncPal->consecutiveBackErrors(), (int64_t) 10)));
    _syncPal->incrementConsecutiveBackErrors();

    const double jitterFactor = jitter(); // 40% of the computed delay
    const int64_t newPauseDuration =
            std::min(static_cast<int64_t>(static_cast<double>(computedDelay) * jitterFactor), backoffVariable::maxDelay);
    LOG_SYNCPAL_INFO(_logger, "Changing pause duration to " << newPauseDuration << " ms");
    setPauseDuration(newPauseDuration);
}

double SyncPalWorker::jitter() const {
    return static_cast<double>(CommonUtility::generateRandomNumber(1000, 1400)) / 1000.0;
}

void SyncPalWorker::checkForMassDeletions() const {
    const auto nbOfLocalDeleteOpsToPropagate = _syncPal->_syncOps->countOps(ReplicaSide::Local, OperationType::Delete);
    if (!nbOfLocalDeleteOpsToPropagate) return;

    // Cumulative count of local deletions that were propagated across successive synchronizations, none of which reached the idle
    // state.
    const auto totalCountOfLocalDeleteOps = _syncPal->nbOfPropagatedLocalDeleteOps() + nbOfLocalDeleteOpsToPropagate;

    // Approximation of the local snapshot size before deletions
    const auto totalCountOfLocalSnapshotItems = _syncPal->snapshot(ReplicaSide::Local)->nbItems() + totalCountOfLocalDeleteOps;

    // Calculate alert threshold = size / ln(size)
    Count alertThreshold = 0;
    if (totalCountOfLocalSnapshotItems > snapshotMinSizeForDeleteAlert) {
        static_assert(snapshotMinSizeForDeleteAlert > 1);
        alertThreshold = static_cast<Count>(static_cast<double>(totalCountOfLocalSnapshotItems) /
                                            log(static_cast<double>(totalCountOfLocalSnapshotItems)));
    }

    // Check for mass deletions
    if (alertThreshold && totalCountOfLocalDeleteOps >= alertThreshold) {
        LOG_SYNCPAL_WARN(_logger,
                         "Mass deletions detected: " << totalCountOfLocalDeleteOps << "/" << totalCountOfLocalSnapshotItems);
        sentry::Handler::captureMessage(sentry::Level::Warning, "SyncPalWorker::checkForMassDeletions",
                                        "Mass deletions detected: " + std::to_string(totalCountOfLocalDeleteOps) + "/" +
                                                std::to_string(totalCountOfLocalSnapshotItems));
    }
}

ExitInfo SyncPalWorker::ensureBlackListIsPropagated() {
    // Check if any of the Blacklisted directory still in the db, if yes, restart the blacklist propagator.
    // It might happen if it as previously been stopped or encountered a locked directory / file.
    NodeSet blacklistedNodes;
    if (ExitInfo exitInfo = SyncNodeCache::instance()->syncNodes(_syncPal->syncDbId(), SyncNodeType::BlackList, blacklistedNodes);
        !exitInfo) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncNodeCache::syncNodes");
        return exitInfo;
    }

    std::function<ExitInfo(const NodeSet &, bool &)> areBlacklistedNodesStillInDb = [this](const NodeSet &nodes, bool &found) {
        found = false;
        for (const auto &nodeId: nodes) {
            DbNodeId dbNodeId = 0;
            if (!_syncPal->syncDb()->dbId(ReplicaSide::Remote, nodeId, dbNodeId, found)) {
                LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbId");
                return ExitInfo(ExitCode::DbError, ExitCause::DbAccessError);
            }

            if (found) return ExitInfo(ExitCode::Ok);
        }
        return ExitInfo(ExitCode::Ok);
    };

    bool found = false;
    if (ExitInfo exitInfo = areBlacklistedNodesStillInDb(blacklistedNodes, found); !exitInfo) {
        LOG_SYNCPAL_WARN(_logger, "Error while checking if blacklisted nodes still exist in SyncDb");
        return exitInfo;
    }

    if (!found) return ExitCode::Ok;

    LOG_WARN(_logger, "Blacklisted nodes still exist in SyncDb, restarting blacklist propagator");

    {
        UserActionScopedLock lock;
        const std::chrono::milliseconds timeout(5000);
        if (!lock.tryLock(_syncPal, timeout)) {
            LOG_SYNCPAL_WARN(Log::instance()->getLogger(),
                             "Could not acquire user action lock for propagateSyncIdSetChange. Another user action is "
                             "running, aborting.");
            return {ExitCode::DataError, ExitCause::BlackListPropagationError};
        }


        if (ExitInfo exitInfo = _syncPal->propagateSyncIdSetChange(false); !exitInfo) {
            LOG_SYNCPAL_WARN(_logger, "Error propagating blacklist changes");
            return exitInfo;
        }
    }

    if (ExitInfo exitInfo = areBlacklistedNodesStillInDb(blacklistedNodes, found); !exitInfo) {
        LOG_SYNCPAL_WARN(_logger, "Error while checking if blacklisted nodes still exist in SyncDb");
        return exitInfo;
    }

    if (found) {
        LOG_SYNCPAL_WARN(_logger, "Blacklisted nodes still exist after retry, giving up");
        return {ExitCode::DataError, ExitCause::BlackListPropagationError};
    }

    return ExitCode::Ok;
}

void SyncPalWorker::adaptFsoWorkerActivityToCurrentState(Worker &fsoWorker, bool &syncDirChanged) {
    auto worker = fsoWorker.worker;

    if (worker == nullptr || worker->isRunning()) return;

    if (fsoWorker.isInProgress) {
        LOG_SYNCPAL_DEBUG(_logger, "Stop FSO worker " << fsoWorker.worker->name());
        fsoWorker.isInProgress = false;
        stopAndWaitForExitOfWorker(fsoWorker.worker);
        if (shouldBePaused({{fsoWorker.side, fsoWorker}}) && !pauseAsked()) {
            pause();
            return;
        }

        syncDirChanged = worker->exitCode() == ExitCode::DataError && worker->exitCause() == ExitCause::SyncDirChanged;
        if (syncDirChanged) return;

        if (shouldBeStopped({{fsoWorker.side, fsoWorker}}) && !stopAsked()) stop();

    } else if (!pauseAsked()) {
        LOG_SYNCPAL_DEBUG(_logger, "Start FSO worker " << worker->name());
        fsoWorker.isInProgress = true;
        worker->start();
    }
}

void SyncPalWorker::execute() {
    ExitCode exitCode(ExitCode::Unknown);
    LOG_SYNCPAL_INFO(_logger, "Worker " << name() << " started");

    // Ensure the pin states are coherent with hydration states.
    if (_syncPal->vfsMode() != VirtualFileMode::Off) {
#if defined(KD_WINDOWS)
        auto resetFunc = std::function<void()>([this]() { resetVfsFilesStatus(); });
        _resetVfsFilesStatusThread = std::make_unique<StdLoggingThread>(resetFunc);
#else
        resetVfsFilesStatus();
#endif
    }

    // Ensure blacklist is propagated
    if (ExitInfo exitInfo = ensureBlackListIsPropagated(); !exitInfo) {
        LOG_SYNCPAL_INFO(_logger, "Worker " << name() << " stopped");
        setExitCause(exitInfo.cause());
        setDone(exitInfo.code());
        return;
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
    ReplicaWorkers fsoWorkers = {
            {ReplicaSide::Local, {.worker = _syncPal->_localFSObserverWorker, .side = ReplicaSide::Local}},
            {ReplicaSide::Remote, {.worker = _syncPal->_remoteFSObserverWorker, .side = ReplicaSide::Remote}}};
    bool isStepInProgress = false;
    ReplicaWorkers stepWorkers = {{ReplicaSide::Local, {.side = ReplicaSide::Local}},
                                  {ReplicaSide::Remote, {.side = ReplicaSide::Remote}}};

    ReplicaInputSharedObjects inputSharedObject = {{ReplicaSide::Local, nullptr}, {ReplicaSide::Remote, nullptr}};
    time_t lastEstimateUpdate = 0;

    bool syncDirChanged = false;
    adaptFsoWorkerActivityToCurrentState(fsoWorkers[ReplicaSide::Remote], syncDirChanged);

    for (;;) {
        // Check File System Observer workers status
        syncDirChanged = false;
        adaptFsoWorkerActivityToCurrentState(fsoWorkers[ReplicaSide::Local], syncDirChanged);

        // Manage SyncDir change (might happen if the sync folder is deleted and recreated e.g migration from an other device)
        if (syncDirChanged && !tryToFixDbNodeIdsAfterSyncDirChange()) {
            LOG_SYNCPAL_INFO(_logger,
                             "Sync dir changed and we are unable to automatically fix syncDb, stopping all workers and exiting");
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
            if (hasSuccessfullyFinished(stepWorkers)) {
                // Next step
                const SyncStep step = nextStep();
                if (step != _step) {
                    LOG_SYNCPAL_INFO(_logger, "***** Step " << stepName(_step) << " has finished");
                    waitForExitOfWorkers(stepWorkers);
                    initStep(step, stepWorkers, inputSharedObject);
                    isStepInProgress = false;
                }
            } else if (shouldBePaused(stepWorkers)) {
                LOG_SYNCPAL_INFO(_logger, "***** Step " << stepName(_step) << " has aborted");

                // Stop the step workers and pause sync
                stopAndWaitForExitOfWorkers(stepWorkers);
                isStepInProgress = false;
                pause();
                continue;
            } else if (shouldBeStoppedAndRestarted(stepWorkers)) {
                LOG_SYNCPAL_INFO(_logger, "***** Step " << stepName(_step) << " has aborted");

                // Stop the step workers and restart a full sync
                stopAndWaitForExitOfWorkers(stepWorkers);
                _syncPal->tryToInvalidateSnapshots();
                initStepFirst(stepWorkers, inputSharedObject, true);
                continue;
            } else if (shouldBeStopped(stepWorkers)) {
                LOG_SYNCPAL_INFO(_logger, "***** Step " << stepName(_step) << " has aborted");

                stopAndWaitForExitOfAllWorkers(fsoWorkers, stepWorkers);

                if (shouldExitWithoutError(stepWorkers)) {
                    exitCode = ExitCode::Ok;
                } else if (shouldRequireUpdate(stepWorkers)) {
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
            for (const auto side: {ReplicaSide::Local, ReplicaSide::Remote}) {
                if (inputSharedObject[side]) inputSharedObject[side]->startRead();
                if (stepWorkers[side].worker) stepWorkers[side].worker->start();
            }

            if (_step == SyncStep::UpdateDetection1) {
                LOG_DEBUG(_logger, "Stopping RFSO as long as the synchronization does not reach the Done state.");
                stopAndWaitForExitOfWorker(_syncPal->_remoteFSObserverWorker);
                fsoWorkers[ReplicaSide::Remote].isInProgress = false;
            } else if (_step == SyncStep::Done) {
                LOG_DEBUG(_logger, "Restarting RFSO until the synchronization leaves the Idle state.");
                _syncPal->_remoteFSObserverWorker->resume();
                fsoWorkers[ReplicaSide::Remote].isInProgress = true;
            }
        }

        if (exitCode != ExitCode::Unknown) break;

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

void SyncPalWorker::initStep(SyncStep step, ReplicaWorkers &workers, ReplicaInputSharedObjects &inputSharedObjects) {
    if (step == SyncStep::UpdateDetection1) {
        sentry::pTraces::basic::Sync(syncDbId()).start();
    }
    sentry::syncStepToPTrace(_step, syncDbId())->stop();
    sentry::syncStepToPTrace(step, syncDbId())->start();

    _step = step;

    workers = {{ReplicaSide::Local, {.side = ReplicaSide::Local}},
               {ReplicaSide::Remote, {.side = ReplicaSide::Remote}},
               {ReplicaSide::Both, {.side = ReplicaSide::Both}}};
    inputSharedObjects = {{ReplicaSide::Local, nullptr}, {ReplicaSide::Remote, nullptr}};

    switch (step) {
        case SyncStep::Idle:
            _syncPal->resetEstimateUpdates();
            _syncPal->refreshTmpBlacklist();
            _syncPal->freeSnapshotsCopies();
            _syncPal->syncDb()->cache().clear();
            _syncPal->resetConsecutiveBackErrors();
            break;
        case SyncStep::UpdateDetection1:
            workers[ReplicaSide::Local].worker = _syncPal->computeFSOperationsWorker();
            _syncPal->copySnapshots();
            LOG_IF_FAIL(_syncPal->syncDb()->cache().reloadIfNeeded());
            _syncPal->setRestart(false);
            break;
        case SyncStep::UpdateDetection2:
            workers[ReplicaSide::Local].worker = _syncPal->_localUpdateTreeWorker;
            workers[ReplicaSide::Remote].worker = _syncPal->_remoteUpdateTreeWorker;
            inputSharedObjects[ReplicaSide::Local] = _syncPal->operationSet(ReplicaSide::Local);
            inputSharedObjects[ReplicaSide::Remote] = _syncPal->operationSet(ReplicaSide::Remote);
            break;
        case SyncStep::Reconciliation1:
            workers[ReplicaSide::Both].worker = _syncPal->_platformInconsistencyCheckerWorker;
            inputSharedObjects[ReplicaSide::Remote] = _syncPal->updateTree(ReplicaSide::Remote);
            break;
        case SyncStep::Reconciliation2:
            workers[ReplicaSide::Both].worker = _syncPal->_conflictFinderWorker;
            break;
        case SyncStep::Reconciliation3:
            workers[ReplicaSide::Both].worker = _syncPal->_conflictResolverWorker;
            break;
        case SyncStep::Reconciliation4:
            workers[ReplicaSide::Both].worker = _syncPal->_operationsGeneratorWorker;
            break;
        case SyncStep::Propagation1:
            workers[ReplicaSide::Both].worker = _syncPal->_operationsSorterWorker;
            _syncPal->startEstimateUpdates();
            break;
        case SyncStep::Propagation2:
            workers[ReplicaSide::Both].worker = _syncPal->_executorWorker;
            _syncPal->syncDb()->cache().clear(); // Cache is not needed anymore, free resources
            break;
        case SyncStep::Done:
            _syncPal->stopEstimateUpdates();
            if (!_syncPal->restart()) {
                _syncPal->resetSnapshotInvalidationCounters();
                _syncPal->setSyncHasFullyCompletedInParams(true);
            }
            if (_syncPal->updateTreesNeedToBeCleared()) {
                LOG_SYNCPAL_DEBUG(_logger, "Clearing update trees");
                _syncPal->updateTree(ReplicaSide::Local)->clear();
                _syncPal->updateTree(ReplicaSide::Remote)->clear();
                _syncPal->setUpdateTreesNeedToBeCleared(false);
            }
            sentry::pTraces::basic::Sync(syncDbId()).stop();
            break;
        default:
            LOG_SYNCPAL_WARN(_logger, "Invalid status");
            break;
    }
}

void SyncPalWorker::initStepFirst(ReplicaWorkers &workers, ReplicaInputSharedObjects &inputSharedObjects, bool reset) {
    LOG_SYNCPAL_DEBUG(_logger, "Restart sync");

    if (reset) _syncPal->resetSharedObjects();

    initStep(SyncStep::Idle, workers, inputSharedObjects);
}

SyncStep SyncPalWorker::nextStep() const {
    switch (_step) {
        case SyncStep::Idle: {
            const bool areLiveSnapshotsValid =
                    _syncPal->liveSnapshot(ReplicaSide::Local).isValid() && _syncPal->liveSnapshot(ReplicaSide::Remote).isValid();
            const bool areWorkersRunning =
                    _syncPal->_localFSObserverWorker->isRunning() && _syncPal->_remoteFSObserverWorker->isRunning();
            const bool areWorkersInitializing =
                    _syncPal->_localFSObserverWorker->initializing() || _syncPal->_remoteFSObserverWorker->initializing();
            const bool areWorkersUpdating =
                    _syncPal->_localFSObserverWorker->updating() || _syncPal->_remoteFSObserverWorker->updating();
            const bool areLiveSnapshotsUpdated =
                    _syncPal->liveSnapshot(ReplicaSide::Local).updated() || _syncPal->liveSnapshot(ReplicaSide::Remote).updated();

            if (areLiveSnapshotsValid && areWorkersRunning && !areWorkersInitializing && !areWorkersUpdating &&
                (areLiveSnapshotsUpdated || _syncPal->restart()))
                return SyncStep::UpdateDetection1;
            else {
                _syncPal->resetNbOfPropagatedLocalDeleteOps();
                return SyncStep::Idle;
            }
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
            checkForMassDeletions();
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

void SyncPalWorker::stopWorkers(ReplicaWorkers &workers) {
    for (const auto side: {ReplicaSide::Local, ReplicaSide::Remote, ReplicaSide::Both}) {
        if (workers.contains(side) && workers[side].worker) workers[side].worker->stop();
    }
}

void SyncPalWorker::waitForExitOfWorkers(ReplicaWorkers &workers) {
    for (const auto side: {ReplicaSide::Local, ReplicaSide::Remote, ReplicaSide::Both}) {
        if (workers.contains(side) && workers[side].worker) workers[side].worker->waitForExit();
    }
}

void SyncPalWorker::stopAndWaitForExitOfWorkers(ReplicaWorkers &workers) {
    stopWorkers(workers);
    waitForExitOfWorkers(workers);
}

void SyncPalWorker::stopAndWaitForExitOfAllWorkers(ReplicaWorkers &fsoWorkers, ReplicaWorkers &stepWorkers) {
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
