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
 * In the context of `ExecutorWorker`, the terminated jobs queue is the only container that can be accessed from multiple threads,
 * namely, the job threads. Therefore, it is the only container that requires to be thread safe.
 */
class TerminatedJobsQueue {
    public:
        void push(const UniqueId id) {
            const std::scoped_lock lock(_mutex);
            _terminatedJobs.push(id);
        }
        void pop() {
            const std::scoped_lock lock(_mutex);
            _terminatedJobs.pop();
        }
        [[nodiscard]] UniqueId front() const { return _terminatedJobs.front(); }
        [[nodiscard]] bool empty() const { return _terminatedJobs.empty(); }

    private:
        std::queue<UniqueId> _terminatedJobs;
        std::mutex _mutex;
};

class ExecutorWorker : public OperationProcessor {
    public:
        ExecutorWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName);

        void executorCallback(UniqueId jobId);

    protected:
        /// @note _executorExitCode and _executorExitCause must be set when the function returns
        void execute() override;

    private:
        void initProgressManager();
        void initSyncFileItem(SyncOpPtr syncOp, SyncFileItem &syncItem);

        ExitInfo handleCreateOp(SyncOpPtr syncOp, std::shared_ptr<AbstractJob> &job, bool &ignored);
        ExitInfo checkAlreadyExcluded(const SyncPath &absolutePath, const NodeId &parentId);
        ExitInfo generateCreateJob(SyncOpPtr syncOp, std::shared_ptr<AbstractJob> &job) noexcept;
        ExitInfo checkLiteSyncInfoForCreate(SyncOpPtr syncOp, const SyncPath &path, bool &isDehydratedPlaceholder);
        ExitInfo createPlaceholder(const SyncPath &relativeLocalPath);
        ExitInfo convertToPlaceholder(const SyncPath &relativeLocalPath, bool hydrated);
        ExitInfo processCreateOrConvertToPlaceholderError(const SyncPath &relativeLocalPath, bool create);
        ExitInfo handleEditOp(SyncOpPtr syncOp, std::shared_ptr<AbstractJob> &job, bool &ignored);
        ExitInfo generateEditJob(SyncOpPtr syncOp, std::shared_ptr<AbstractJob> &job);

        /**
         * This method aims to fix the last modification date of a local file using the date stored in DB. This allows us to fix
         * wrong EDIT operations generated on dehydrated placeholders.
         * @param syncOp : the operation to propagate.
         * @param absolutePath : absolute local path of the affected file.
         * @return `true` if the date is modified successfully.
         * @note _executorExitCode and _executorExitCause must be set when the function returns false
         */
        ExitInfo fixModificationDate(SyncOpPtr syncOp, const SyncPath &absolutePath);
        ExitInfo checkLiteSyncInfoForEdit(SyncOpPtr syncOp, const SyncPath &absolutePath, bool &ignoreItem,
                                          bool &isSyncing); // TODO : is called "check..." but perform some actions. Wording not
                                                            // good, function probably does too much

        ExitInfo handleMoveOp(SyncOpPtr syncOp, bool &ignored, bool &bypassProgressComplete);
        ExitInfo generateMoveJob(SyncOpPtr syncOp, bool &ignored, bool &bypassProgressComplete);

        ExitInfo handleDeleteOp(SyncOpPtr syncOp, bool &ignored, bool &bypassProgressComplete);
        ExitInfo generateDeleteJob(SyncOpPtr syncOp, bool &ignored, bool &bypassProgressComplete);

        /// @note _executorExitCode and _executorExitCause must be set when the function returns with hasError == true
        void waitForAllJobsToFinish(bool &hasError);

        ExitInfo waitForAllJobsToFinish();
        ExitInfo deleteFinishedAsyncJobs();
        ExitInfo handleManagedBackError(ExitCause jobExitCause, SyncOpPtr syncOp, bool isInconsistencyIssue,
                                        bool downloadImpossible);
        ExitInfo handleFinishedJob(std::shared_ptr<AbstractJob> job, SyncOpPtr syncOp, const SyncPath &relativeLocalPath,
                                   bool &ignored, bool &bypassProgressComplete);
        ExitInfo handleForbiddenAction(SyncOpPtr syncOp, const SyncPath &relativeLocalPath, bool &ignored);
        void sendProgress();
        bool isValidDestination(SyncOpPtr syncOp);
        bool enoughLocalSpace(SyncOpPtr syncOp);

        ExitInfo propagateConflictToDbAndTree(SyncOpPtr syncOp, bool &propagateChange);
        ExitInfo propagateChangeToDbAndTree(SyncOpPtr syncOp, std::shared_ptr<AbstractJob> job, std::shared_ptr<Node> &node);
        ExitInfo propagateCreateToDbAndTree(SyncOpPtr syncOp, const NodeId &newNodeId, std::optional<SyncTime> newLastModTime,
                                            std::shared_ptr<Node> &node);
        ExitInfo propagateEditToDbAndTree(SyncOpPtr syncOp, const NodeId &newNodeId, std::optional<SyncTime> newLastModTime,
                                          std::shared_ptr<Node> &node);
        ExitInfo propagateMoveToDbAndTree(SyncOpPtr syncOp);
        ExitInfo propagateDeleteToDbAndTree(SyncOpPtr syncOp);
        ExitInfo deleteFromDb(std::shared_ptr<Node> node);

        ExitInfo runCreateDirJob(SyncOpPtr syncOp, std::shared_ptr<AbstractJob> job);
        void cancelAllOngoingJobs(bool reschedule = false);
        void manageJobDependencies(SyncOpPtr syncOp, std::shared_ptr<AbstractJob> job);

        inline bool isLiteSyncActivated() { return _syncPal->vfsMode() != VirtualFileMode::Off; }

        inline std::shared_ptr<UpdateTree> affectedUpdateTree(SyncOpPtr syncOp) {
            return _syncPal->updateTree(otherSide(syncOp->targetSide()));
        }
        inline std::shared_ptr<UpdateTree> targetUpdateTree(SyncOpPtr syncOp) {
            return _syncPal->updateTree(syncOp->targetSide());
        }

        void increaseErrorCount(SyncOpPtr syncOp);

        ExitInfo getFileSize(const SyncPath &path, uint64_t &size);
        void logCorrespondingNodeErrorMsg(const SyncOpPtr syncOp);

        void setProgressComplete(const SyncOpPtr syncOp, SyncFileStatus status);

        // This methode will return ExitCode::Ok if the error is safely managed and the executor can continue. Else, it will
        // return opsExitInfo.
        ExitInfo handleExecutorError(SyncOpPtr syncOp, ExitInfo opsExitInfo);
        ExitInfo handleOpsFileAccessError(SyncOpPtr syncOp, ExitInfo opsExitInfo);
        ExitInfo handleOpsAlreadyExistError(SyncOpPtr syncOp, ExitInfo opsExitInfo);

        ExitInfo removeDependentOps(SyncOpPtr syncOp);

        std::unordered_map<UniqueId, std::shared_ptr<AbstractJob>> _ongoingJobs;
        TerminatedJobsQueue _terminatedJobs;
        std::unordered_map<UniqueId, SyncOpPtr> _jobToSyncOpMap;
        std::unordered_map<UniqueId, UniqueId> _syncOpToJobMap;
        std::list<UniqueId> _opList;
        std::chrono::steady_clock::time_point _fileProgressTimer = std::chrono::steady_clock::now();

        bool _snapshotToInvalidate = false;

        friend class TestExecutorWorker;
        friend class TestWorkers;
};

} // namespace KDC
