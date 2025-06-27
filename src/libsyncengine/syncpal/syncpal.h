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

#pragma once

#include "syncenginelib.h"
#include "db/syncdb.h"
#include "progress/progressinfo.h"
#include "syncpal/conflictingfilescorrector.h"
#include "update_detection/file_system_observer/snapshot/livesnapshot.h"
#include "update_detection/file_system_observer/snapshot/snapshot.h"
#include "update_detection/file_system_observer/fsoperationset.h"
#include "update_detection/update_detector/updatetree.h"
#include "reconciliation/conflict_finder/conflict.h"
#include "reconciliation/syncoperation.h"
#include "libcommonserver/log/log.h"
#include "libcommon/utility/types.h"
#include "libcommonserver/vfs/vfs.h"
#include "libparms/db/parmsdb.h"

#include <memory>
#include <comm.h>

namespace KDC {

class BlacklistPropagator;
class ExcludeListPropagator;
class ConflictingFilesCorrector;

class SyncPalWorker;
class FileSystemObserverWorker;
class ComputeFSOperationWorker;
class UpdateTreeWorker;
class PlatformInconsistencyCheckerWorker;
class ConflictFinderWorker;
class ConflictResolverWorker;
class OperationGeneratorWorker;
class OperationSorterWorker;
class ExecutorWorker;

class TmpBlacklistManager;

class DownloadJob;
class GetSizeJob;

#define SYNCDBID this->syncDbId()

#define LOG_SYNCDBID std::string("*" + std::to_string(SYNCDBID) + "*")

#define LOG_SYNCPAL_DEBUG(logger, logEvent) LOG_DEBUG(logger, LOG_SYNCDBID << " " << logEvent)
#define LOGW_SYNCPAL_DEBUG(logger, logEvent) LOGW_DEBUG(logger, Utility::s2ws(LOG_SYNCDBID) << L" " << logEvent)

#define LOG_SYNCPAL_INFO(logger, logEvent) LOG_INFO(logger, LOG_SYNCDBID << " " << logEvent)
#define LOGW_SYNCPAL_INFO(logger, logEvent) LOGW_INFO(logger, Utility::s2ws(LOG_SYNCDBID) << L" " << logEvent)

#define LOG_SYNCPAL_WARN(logger, logEvent) LOG_WARN(logger, LOG_SYNCDBID << " " << logEvent)
#define LOGW_SYNCPAL_WARN(logger, logEvent) LOGW_WARN(logger, Utility::s2ws(LOG_SYNCDBID) << L" " << logEvent)

#define LOG_SYNCPAL_ERROR(logger, logEvent) LOG_ERROR(logger, LOG_SYNCDBID << " " << logEvent)
#define LOGW_SYNCPAL_ERROR(logger, logEvent) LOGW_ERROR(logger, Utility::s2ws(LOG_SYNCDBID) << L" " << logEvent)

#define LOG_SYNCPAL_FATAL(logger, logEvent) LOG_FATAL(logger, LOG_SYNCDBID << " " << logEvent)
#define LOGW_SYNCPAL_FATAL(logger, logEvent) LOGW_FATAL(logger, Utility::s2ws(LOG_SYNCDBID) << L" " << logEvent)

struct SyncPalInfo {
        SyncPalInfo() = default;
        SyncPalInfo(const int driveDbId_, const SyncPath &localPath_, const SyncPath targetPath_ = {}) :
            driveDbId(driveDbId_),
            localPath(localPath_),
            targetPath(targetPath_) {}

        int syncDbId{0};
        int driveDbId{0};
        int driveId{0};
        int accountDbId{0};
        int userDbId{0};
        int userId{0};
        std::string driveName;
        SyncPath localPath;
        NodeId localNodeId;
        SyncPath targetPath;
        VirtualFileMode vfsMode{VirtualFileMode::Off};
        bool restart{false};
        bool isPaused{false};
        bool syncHasFullyCompleted{false};

        // An advanced synchronisation targets a subdirectory of a remote drive
        bool isAdvancedSync() const { return !targetPath.empty(); }
};

struct SyncProgress {
        int64_t _currentFile{0};
        int64_t _totalFiles{0};
        int64_t _completedSize{0};
        int64_t _totalSize{0};
        int64_t _estimatedRemainingTime{0};

        bool operator==(const SyncProgress &other) const {
            return _currentFile == other._currentFile && _totalFiles == other._totalFiles &&
                   _completedSize == other._completedSize && _totalSize == other._totalSize &&
                   _estimatedRemainingTime == other._estimatedRemainingTime;
        }
};


class SYNCENGINE_EXPORT SyncPal : public std::enable_shared_from_this<SyncPal> {
    public:
        SyncPal(std::shared_ptr<Vfs> vfs, const SyncPath &syncDbPath, const std::string &version, const bool hasFullyCompleted);
        SyncPal(std::shared_ptr<Vfs> vfs, const int syncDbId, const std::string &version);
        virtual ~SyncPal();

        inline void setAddErrorCallback(const std::function<void(const Error &)> &addError) { _addError = addError; }
        inline void setAddCompletedItemCallback(const std::function<void(int, const SyncFileItem &, bool)> &addCompletedItem) {
            _addCompletedItem = addCompletedItem;
        }

        inline void setSendSignalCallback(const std::function<void(SignalNum, int, const SigValueType &)> &sendSignal) {
            _sendSignal = sendSignal;
        }

        void setVfs(std::shared_ptr<Vfs> vfs);
        inline std::shared_ptr<Vfs> vfs() { return _vfs; }

        // SyncPalInfo
        [[nodiscard]] inline std::shared_ptr<SyncDb> syncDb() const { return _syncDb; }
        inline const SyncPalInfo &syncInfo() const { return _syncInfo; }
        inline int syncDbId() const { return _syncInfo.syncDbId; }
        inline int driveDbId() const { return _syncInfo.driveDbId; }
        inline int driveId() const { return _syncInfo.driveId; }
        inline int accountDbId() const { return _syncInfo.accountDbId; }
        inline int userDbId() const { return _syncInfo.userDbId; }
        inline int userId() const { return _syncInfo.userId; }
        inline const std::string &driveName() const { return _syncInfo.driveName; }
        inline VirtualFileMode vfsMode() const { return _syncInfo.vfsMode; }
        inline SyncPath localPath() const { return _syncInfo.localPath; }
        inline const NodeId &localNodeId() const { return _syncInfo.localNodeId; }
        inline bool restart() const { return _syncInfo.restart; }
        inline bool isAdvancedSync() const { return _syncInfo.isAdvancedSync(); }

        void setLocalPath(const SyncPath &path) { _syncInfo.localPath = path; }
        ExitInfo isRootFolderValid();
        ExitInfo setLocalNodeId(const NodeId &localNodeId);
        void setSyncHasFullyCompleted(bool completed) { _syncInfo.syncHasFullyCompleted = completed; }
        void setRestart(bool shouldRestart) { _syncInfo.restart = shouldRestart; }
        void setVfsMode(const VirtualFileMode mode) { _syncInfo.vfsMode = mode; }
        void setIsPaused(const bool paused) { _syncInfo.isPaused = paused; }

        [[nodiscard]] const std::shared_ptr<SyncOperationList> &syncOps() const { return _syncOps; }
        [[nodiscard]] const std::shared_ptr<ConflictQueue> &conflictQueue() const { return _conflictQueue; }

        // TODO : not ideal, to be refactored
        bool checkIfExistsOnServer(const SyncPath &path, bool &exists) const;
        bool checkIfCanShareItem(const SyncPath &path, bool &canShare) const;

        ExitCode fileRemoteIdFromLocalPath(const SyncPath &path, NodeId &nodeId) const;
        ExitCode syncIdSet(SyncNodeType type, NodeSet &nodeIdSet);
        ExitCode setSyncIdSet(SyncNodeType type, const NodeSet &nodeIdSet);
        ExitCode syncListUpdated(bool restartSync);
        ExitCode excludeListUpdated();
        ExitCode fixConflictingFiles(bool keepLocalVersion, std::vector<Error> &errorList);
        ExitCode fixCorruptedFile(const std::unordered_map<NodeId, SyncPath> &localFileMap);
        ExitCode fileStatus(ReplicaSide side, const SyncPath &path, SyncFileStatus &status) const;
        ExitCode fileSyncing(ReplicaSide side, const SyncPath &path, bool &syncing) const;
        ExitCode setFileSyncing(ReplicaSide side, const SyncPath &path, bool syncing);
        ExitCode path(ReplicaSide side, const NodeId &nodeId, SyncPath &path);
        ExitCode clearNodes();

        void syncPalStartCallback(UniqueId jobId);

        //! Start SyncPal.
        /*!
         \param startDelay represents the time (expressed in seconds) the SyncPalWorker must wait before starting.
         */
        void start(const std::chrono::seconds &startDelay = std::chrono::seconds(0));
        void stop(bool pausedByUser = false, bool quit = false, bool clear = false);


        /* The synchronization will be paused once the ongoing sync reach the Idle state.
         * It will automatically restart after one minute (triggered by the appserver).
         *
         * /!\ This pause mechanism is intended for internal use only, such as handling network disconnections.
         * If the user pauses synchronization, use the stop() method instead.
         */
        void pause();
        void unpause();
        std::chrono::time_point<std::chrono::steady_clock> pauseTime() const;
        bool isPaused() const;
        bool pauseAsked() const;
        bool isIdle() const;
        bool isRunning() const;

        SyncStatus status() const;
        SyncStep step() const;

        void addError(const Error &error);
        void addCompletedItem(int syncDbId, const SyncFileItem &item);

        bool wipeVirtualFiles();
        bool wipeOldPlaceholders();

        void loadProgress(SyncProgress &syncProgress) const;
        [[nodiscard]] bool getSyncFileItem(const SyncPath &path, SyncFileItem &item);

        void resetSnapshotInvalidationCounters();

        ExitCode addDlDirectJob(const SyncPath &relativePath, const SyncPath &localPath);
        ExitCode cancelDlDirectJobs(const std::list<SyncPath> &fileList);
        ExitCode cancelAllDlDirectJobs(bool quit);
        ExitCode cleanOldUploadSessionTokens();
        bool isDownloadOngoing(const SyncPath &localPath);

        inline bool syncHasFullyCompleted() const { return _syncInfo.syncHasFullyCompleted; }

        void fixInconsistentFileNames();

        void fixNodeTableDeleteItemsWithNullParentNodeId();

        virtual void increaseErrorCount(const NodeId &nodeId, NodeType type, const SyncPath &relativePath, ReplicaSide side,
                                        ExitInfo exitInfo = ExitInfo());
        virtual void blacklistTemporarily(const NodeId &nodeId, const SyncPath &relativePath, ReplicaSide side);
        virtual bool isTmpBlacklisted(const SyncPath &relativePath, ReplicaSide side) const;
        virtual void refreshTmpBlacklist();
        virtual void removeItemFromTmpBlacklist(const NodeId &nodeId, ReplicaSide side);
        virtual void removeItemFromTmpBlacklist(const SyncPath &relativePath);
        //! Handle an access denied error on an item on the local side.
        /*!
         \param relativeLocalPath is the local path of the item.
         \param cause is an optional exit cause returned by the operation that has failed. Used only to generate an error message
         tailored to the context.
         \return The exit info of the function.
         */
        ExitInfo handleAccessDeniedItem(const SyncPath &relativeLocalPath, ExitCause cause = ExitCause::FileAccessError);
        ExitInfo handleAccessDeniedItem(const SyncPath &relativeLocalPath, std::shared_ptr<Node> &localBlacklistedNode,
                                        std::shared_ptr<Node> &remoteBlacklistedNode, ExitCause cause);

        //! Makes copies of real-time snapshots to be used by synchronization workers.
        void copySnapshots();
        void freeSnapshotsCopies();
        void tryToInvalidateSnapshots();
        void forceInvalidateSnapshots();

        // Workers
        std::shared_ptr<ComputeFSOperationWorker> computeFSOperationsWorker() const { return _computeFSOperationsWorker; }
        void setComputeFSOperationsWorker(std::shared_ptr<ComputeFSOperationWorker> worker) {
            _computeFSOperationsWorker = worker;
        }

        void createSharedObjects();
        void freeSharedObjects();
        void initSharedObjects();
        void resetSharedObjects();

        std::shared_ptr<UpdateTree> updateTree(ReplicaSide side) const;

        // Returns a snapshot of the filesystem state at the start of the ongoing sync.
        // This snapshot is immutable and remains consistent throughout the sync.
        // Returns nullptr if no sync is currently in progress.
        std::shared_ptr<ConstSnapshot> snapshot(ReplicaSide side) const;

        /* Returns a reference to the live snapshot, which reflects the real-time state of the filesystem.
         * Unlike the immutable snapshot(), this one is continuously updated as changes occur.
         *
         * /!\ Must not be called before SyncPal::createWorkers() or after SyncPal::freeWorkers(),
         * as the underlying data may not be initialized or may have already been released.
         *
         * The live snapshot is intended to be modified only by the FSO workers.
         * Workers should always retrieve information from a ConstSnapshot (see SyncPal::snapshot(ReplicaSide side))
         * There are a few exceptions where reading directly from the liveSnapshot is necessary:
         * - To check if the filesystem has changed (liveSnapshot().updated()).
         * - To create a ConstSnapshot (ConstSnapshot(liveSnapshot())).
         * - When outside of a sync process (i.e., not in a worker), since no ConstSnapshot is available.
         *      Ideally, we would query the filesystem directly in these situations. However, for optimization purposes,
         *      it may be preferable to read from the live snapshot, accepting the trade-off that it might lag slightly
         *      behind the actual state of the filesystem.
         */
        const LiveSnapshot &liveSnapshot(ReplicaSide side) const;

    protected:
        virtual void createWorkers(const std::chrono::seconds &startDelay = std::chrono::seconds(0));

        SyncPalInfo _syncInfo;

        std::shared_ptr<ExcludeListPropagator> _excludeListPropagator = nullptr;
        std::shared_ptr<BlacklistPropagator> _blacklistPropagator = nullptr;
        std::shared_ptr<ConflictingFilesCorrector> _conflictingFilesCorrector = nullptr;

        std::unordered_map<UniqueId, std::shared_ptr<DownloadJob>> _directDownloadJobsMap;
        std::unordered_map<SyncPath, UniqueId, PathHashFunction> _syncPathToDownloadJobMap;
        std::mutex _directDownloadJobsMapMutex;

        // Callbacks
        std::function<void(const Error &error)> _addError;
        std::function<void(int syncDbId, const SyncFileItem &item, bool notify)> _addCompletedItem;
        std::function<void(SignalNum sigId, int syncDbId, const SigValueType &val)> _sendSignal;
        std::shared_ptr<Vfs> _vfs;

        // DB
        std::shared_ptr<SyncDb> _syncDb{nullptr};

        // Shared objects
        std::shared_ptr<ConstSnapshot> _localSnapshot{nullptr}; // A copy of the real-time local snapshot taken at the start
                                                                // of each sync, used by synchronization workers
        std::shared_ptr<ConstSnapshot> _remoteSnapshot{nullptr}; // A copy of the real-time remote snapshot taken at the start
                                                                 // of each sync, used by synchronization workers
        std::shared_ptr<FSOperationSet> _localOperationSet{nullptr};
        std::shared_ptr<FSOperationSet> _remoteOperationSet{nullptr};
        std::shared_ptr<UpdateTree> _localUpdateTree{nullptr};
        std::shared_ptr<UpdateTree> _remoteUpdateTree{nullptr};
        std::shared_ptr<ConflictQueue> _conflictQueue{nullptr};
        std::shared_ptr<SyncOperationList> _syncOps{nullptr};
        std::shared_ptr<ProgressInfo> _progressInfo{nullptr};

        // Workers
        std::shared_ptr<SyncPalWorker> _syncPalWorker{nullptr};
        std::shared_ptr<FileSystemObserverWorker> _localFSObserverWorker{nullptr};
        std::shared_ptr<FileSystemObserverWorker> _remoteFSObserverWorker{nullptr};
        std::shared_ptr<ComputeFSOperationWorker> _computeFSOperationsWorker{nullptr};
        std::shared_ptr<UpdateTreeWorker> _localUpdateTreeWorker{nullptr};
        std::shared_ptr<UpdateTreeWorker> _remoteUpdateTreeWorker{nullptr};
        std::shared_ptr<PlatformInconsistencyCheckerWorker> _platformInconsistencyCheckerWorker{nullptr};
        std::shared_ptr<ConflictFinderWorker> _conflictFinderWorker{nullptr};
        std::shared_ptr<ConflictResolverWorker> _conflictResolverWorker{nullptr};
        std::shared_ptr<OperationGeneratorWorker> _operationsGeneratorWorker{nullptr};
        std::shared_ptr<OperationSorterWorker> _operationsSorterWorker{nullptr};
        std::shared_ptr<ExecutorWorker> _executorWorker{nullptr};

        std::shared_ptr<TmpBlacklistManager> _tmpBlacklistManager{nullptr};


        void freeWorkers();
        ExitCode setSyncPaused(bool value);
        bool createOrOpenDb(const SyncPath &syncDbPath, const std::string &version,
                            const std::string &targetNodeId = std::string());
        void setSyncHasFullyCompletedInParms(bool syncHasFullyCompleted);
        ExitInfo setListingCursor(const std::string &value, int64_t timestamp);
        ExitInfo listingCursor(std::string &value, int64_t &timestamp);
        ExitCode updateSyncNode(SyncNodeType syncNodeType);
        ExitCode updateSyncNode();
        std::shared_ptr<FSOperationSet> operationSet(ReplicaSide side) const;

        // Progress info management
        void createProgressInfo();
        void resetEstimateUpdates();
        void startEstimateUpdates();
        void stopEstimateUpdates();
        void updateEstimates();
        [[nodiscard]] bool initProgress(const SyncFileItem &item);
        [[nodiscard]] bool setProgress(const SyncPath &relativePath, int64_t current);
        [[nodiscard]] bool setProgressComplete(const SyncPath &relativeLocalPath, SyncFileStatus status,
                                               const NodeId &newRemoteNodeId = {});

        // Direct download callback
        void directDownloadCallback(UniqueId jobId);

    private:
        log4cplus::Logger _logger;

        // TODO : Refactor to not use friend classes (should be reserved for test purpose).
        friend class SyncPalWorker;
        friend class FileSystemObserverWorker;
        friend class LocalFileSystemObserverWorker;
        friend class LocalFileSystemObserverWorker_unix;
        friend class LocalFileSystemObserverWorker_win;
        friend class RemoteFileSystemObserverWorker;
        friend class ComputeFSOperationWorker;
        friend class UpdateTreeWorker;
        friend class PlatformInconsistencyCheckerWorker;
        friend class OperationProcessor;
        friend class ConflictFinderWorker;
        friend class OperationGeneratorWorker;
        friend class OperationSorterWorker;
        friend class ExecutorWorker;

        friend class BlacklistPropagator;
        friend class ExcludeListPropagator;
        friend class ConflictingFilesCorrector;
        friend class TmpBlacklistManager;

        friend class TestSyncPal;
        friend class TestOperationProcessor;
        friend class TestLocalFileSystemObserverWorker;
        friend class TestRemoteFileSystemObserverWorker;
        friend class TestComputeFSOperationWorker;
        friend class TestUpdateTreeWorker;
        friend class TestPlatformInconsistencyCheckerWorker;
        friend class TestConflictFinderWorker;
        friend class TestConflictResolverWorker;
        friend class TestOperationGeneratorWorker;
        friend class TestOperationSorterWorker;
        friend class TestExecutorWorker;
        friend class TestSnapshot;
        friend class TestLocalJobs;
        friend class TestIntegration;
        friend class TestWorkers;
        friend class TestAppServer;
        friend class MockSyncPal;
        friend class TestSituationGenerator;
        friend class TestFileRescuer;
};

} // namespace KDC
