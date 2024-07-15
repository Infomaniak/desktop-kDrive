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

#pragma once

#include "syncpal/operationprocessor.h"
#include "syncpal/syncpal.h"
#include "reconciliation/syncoperation.h"
#include "jobs/abstractjob.h"

#include <queue>
#include <unordered_map>

namespace KDC {

class AbstractNetworkJob;
class UpdateTree;
class FSOperationSet;
class SyncDb;

/**
 * A thread safe implementation of the terminated jobs queue.
 * In the context of `ExecutorWorker`, the terminated jobs queue is the only container that can be accessed from multiple threads, namely, the job threads.
 * Therefore, it is the only container that requires to be thread safe.
 */
class TerminatedJobsQueue {
    public:
        void push(const UniqueId id) {
            const std::scoped_lock lock(mutex);
            terminatedJobs.push(id);
        }
        void pop() {
            const std::scoped_lock lock(mutex);
            terminatedJobs.pop();
        }
        [[nodiscard]] UniqueId front() const { return terminatedJobs.front(); }
        [[nodiscard]] bool empty() const { return terminatedJobs.empty(); }

    private:
        std::queue<UniqueId> _terminatedJobs;
        std::mutex _mutex;
};

class ExecutorWorker : public OperationProcessor {
    public:
        ExecutorWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName);

        void executorCallback(UniqueId jobId);

    protected:
        void execute() override;

    private:
        void initProgressManager();
        bool initSyncFileItem(SyncOpPtr syncOp, SyncFileItem &syncItem);

        void handleCreateOp(SyncOpPtr syncOp, std::shared_ptr<AbstractJob> &job, bool &hasError);
        void checkAlreadyExcluded(const SyncPath &absolutePath, const NodeId &parentId);
        bool generateCreateJob(SyncOpPtr syncOp, std::shared_ptr<AbstractJob> &job) noexcept;
        bool checkLiteSyncInfoForCreate(SyncOpPtr syncOp, SyncPath &path, bool &isDehydratedPlaceholder);
        bool createPlaceholder(const SyncPath &relativeLocalPath);
        bool convertToPlaceholder(const SyncPath &relativeLocalPath, bool hydrated, bool &needRestart);

        void handleEditOp(SyncOpPtr syncOp, std::shared_ptr<AbstractJob> &job, bool &hasError);
        bool generateEditJob(SyncOpPtr syncOp, std::shared_ptr<AbstractJob> &job);
        bool checkLiteSyncInfoForEdit(SyncOpPtr syncOp, SyncPath &absolutePath, bool &ignoreItem,
                                      bool &isSyncing);  // TODO : is called "check..." but perform some actions. Wording not
                                                         // good, function probably does too much

        void handleMoveOp(SyncOpPtr syncOp, bool &hasError);
        bool generateMoveJob(SyncOpPtr syncOp);

        void handleDeleteOp(SyncOpPtr syncOp, bool &hasError);
        bool generateDeleteJob(SyncOpPtr syncOp);

        bool hasRight(SyncOpPtr syncOp, bool &exists);
        bool enoughLocalSpace(SyncOpPtr syncOp);

        void waitForAllJobsToFinish(bool &hasError);
        bool deleteFinishedAsyncJobs();
        bool handleManagedBackError(ExitCause jobExitCause, SyncOpPtr syncOp, bool isInconsistencyIssue, bool downloadImpossible);
        bool handleFinishedJob(std::shared_ptr<AbstractJob> job, SyncOpPtr syncOp, const SyncPath &relativeLocalPath);
        void handleForbiddenAction(SyncOpPtr syncOp, const SyncPath &relativeLocalPath);
        void sendProgress();

        bool propagateConflictToDbAndTree(SyncOpPtr syncOp, bool &propagateChange);
        bool propagateChangeToDbAndTree(SyncOpPtr syncOp, std::shared_ptr<AbstractJob> job, std::shared_ptr<Node> &node);
        bool propagateCreateToDbAndTree(SyncOpPtr syncOp, const NodeId &newNodeId, std::optional<SyncTime> newLastModTime,
                                        std::shared_ptr<Node> &node);
        bool propagateEditToDbAndTree(SyncOpPtr syncOp, const NodeId &newNodeId, std::optional<SyncTime> newLastModTime,
                                      std::shared_ptr<Node> &node);
        bool propagateMoveToDbAndTree(SyncOpPtr syncOp);
        bool propagateDeleteToDbAndTree(SyncOpPtr syncOp);
        bool deleteFromDb(std::shared_ptr<Node> node);

        bool runCreateDirJob(SyncOpPtr syncOp, std::shared_ptr<AbstractJob> job);

        void cancelAllOngoingJobs(bool reschedule = false);

        void manageJobDependencies(SyncOpPtr syncOp, std::shared_ptr<AbstractJob> job);

        inline bool isLiteSyncActivated() { return _syncPal->_vfsMode != VirtualFileModeOff; }

        inline std::shared_ptr<UpdateTree> affectedUpdateTree(SyncOpPtr syncOp) {
            return _syncPal->updateTree(otherSide(syncOp->targetSide()));
        }
        inline std::shared_ptr<UpdateTree> targetUpdateTree(SyncOpPtr syncOp) {
            return _syncPal->updateTree(syncOp->targetSide());
        }

        void increaseErrorCount(SyncOpPtr syncOp);

        std::unordered_map<UniqueId, std::shared_ptr<AbstractJob>> _ongoingJobs;
        TerminatedJobsQueue _terminatedJobs;
        std::unordered_map<UniqueId, SyncOpPtr> _jobToSyncOpMap;
        std::unordered_map<UniqueId, UniqueId> _syncOpToJobMap;
        std::list<UniqueId> _opList;

        ExitCode _executorExitCode = ExitCodeUnknown;
        ExitCause _executorExitCause = ExitCauseUnknown;

        std::chrono::steady_clock::time_point _fileProgressTimer = std::chrono::steady_clock::now();

        bool _snapshotToInvalidate = false;

        friend class TestExecutorWorker;
};

}  // namespace KDC
