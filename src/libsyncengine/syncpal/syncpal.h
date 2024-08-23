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

#include "syncenginelib.h"
#include "db/syncdb.h"
#include "progress/progressinfo.h"
#include "syncpal/conflictingfilescorrector.h"
#include "update_detection/file_system_observer/snapshot/snapshot.h"
#include "update_detection/file_system_observer/fsoperationset.h"
#include "update_detection/update_detector/updatetree.h"
#include "reconciliation/conflict_finder/conflict.h"
#include "reconciliation/syncoperation.h"
#include "libcommonserver/log/log.h"
#include "libcommon/utility/types.h"
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

#define LOG_SYNCDBID "*" << SYNCDBID << "*"

#define LOG_SYNCPAL_DEBUG(logger, logEvent) LOG_DEBUG(logger, LOG_SYNCDBID << " " << logEvent)
#define LOGW_SYNCPAL_DEBUG(logger, logEvent) LOGW_DEBUG(logger, LOG_SYNCDBID << " " << logEvent)

#define LOG_SYNCPAL_INFO(logger, logEvent) LOG_INFO(logger, LOG_SYNCDBID << " " << logEvent)
#define LOGW_SYNCPAL_INFO(logger, logEvent) LOGW_INFO(logger, LOG_SYNCDBID << " " << logEvent)

#define LOG_SYNCPAL_WARN(logger, logEvent) LOG_WARN(logger, LOG_SYNCDBID << " " << logEvent)
#define LOGW_SYNCPAL_WARN(logger, logEvent) LOGW_WARN(logger, LOG_SYNCDBID << " " << logEvent)

#define LOG_SYNCPAL_ERROR(logger, logEvent) LOG_ERROR(logger, LOG_SYNCDBID << " " << logEvent)
#define LOGW_SYNCPAL_ERROR(logger, logEvent) LOGW_ERROR(logger, LOG_SYNCDBID << " " << logEvent)

#define LOG_SYNCPAL_FATAL(logger, logEvent) LOG_FATAL(logger, LOG_SYNCDBID << " " << logEvent)
#define LOGW_SYNCPAL_FATAL(logger, logEvent) LOGW_FATAL(logger, LOG_SYNCDBID << " " << logEvent)

struct SyncPalInfo {
        SyncPalInfo() = default;
        SyncPalInfo(const int driveDbId_, const SyncPath &localPath_, const SyncPath targetPath_ = {})
            : driveDbId(driveDbId_), localPath(localPath_), targetPath(targetPath_) {}

        int syncDbId{0};
        int driveDbId{0};
        int driveId{0};
        int accountDbId{0};
        int userDbId{0};
        int userId{0};
        std::string driveName;
        SyncPath localPath;
        SyncPath targetPath;
        VirtualFileMode vfsMode{VirtualFileMode::Off};
        bool restart{false};
        bool isPaused{false};
        bool syncHasFullyCompleted;

        bool isAdvancedSync() const { return !targetPath.empty(); };
};


class SYNCENGINE_EXPORT SyncPal : public std::enable_shared_from_this<SyncPal> {
    public:
        SyncPal(const SyncPath &syncDbPath, const std::string &version, const bool hasFullyCompleted);
        SyncPal(const int syncDbId, const std::string &version);
        virtual ~SyncPal();

        ExitCode setTargetNodeId(const std::string &targetNodeId);

        inline void setAddErrorCallback(void (*addError)(const Error &)) { _addError = addError; }
        inline void setAddCompletedItemCallback(void (*addCompletedItem)(int, const SyncFileItem &, bool)) {
            _addCompletedItem = addCompletedItem;
        }
        inline void setSendSignalCallback(void (*sendSignal)(SignalNum, int, const SigValueType &)) { _sendSignal = sendSignal; }

        inline void setVfsIsExcludedCallback(bool (*vfsIsExcluded)(int, const SyncPath &, bool &)) {
            _vfsIsExcluded = vfsIsExcluded;
        }
        inline void setVfsExcludeCallback(bool (*vfsExclude)(int, const SyncPath &)) { _vfsExclude = vfsExclude; }
        inline void setVfsPinStateCallback(bool (*vfsPinState)(int, const SyncPath &, PinState &)) { _vfsPinState = vfsPinState; }
        inline void setVfsSetPinStateCallback(bool (*vfsSetPinState)(int, const SyncPath &, PinState)) {
            _vfsSetPinState = vfsSetPinState;
        }
        inline void setVfsStatusCallback(bool (*vfsStatus)(int, const SyncPath &, bool &, bool &, bool &, int &)) {
            _vfsStatus = vfsStatus;
        }
        inline void setVfsCreatePlaceholderCallback(bool (*vfsCreatePlaceholder)(int, const SyncPath &, const SyncFileItem &)) {
            _vfsCreatePlaceholder = vfsCreatePlaceholder;
        }
        inline void setVfsConvertToPlaceholderCallback(bool (*vfsConvertToPlaceholder)(int, const SyncPath &,
                                                                                       const SyncFileItem &, bool &)) {
            _vfsConvertToPlaceholder = vfsConvertToPlaceholder;
        }
        inline void setVfsUpdateMetadataCallback(bool (*vfsUpdateMetadata)(int, const SyncPath &, const SyncTime &,
                                                                           const SyncTime &, const int64_t, const NodeId &,
                                                                           std::string &)) {
            _vfsUpdateMetadata = vfsUpdateMetadata;
        }
        inline void setVfsUpdateFetchStatusCallback(bool (*vfsUpdateFetchStatus)(int, const SyncPath &, const SyncPath &, int64_t,
                                                                                 bool &, bool &)) {
            _vfsUpdateFetchStatus = vfsUpdateFetchStatus;
        }
        inline void setVfsFileStatusChangedCallback(bool (*vfsFileStatusChanged)(int, const SyncPath &, SyncFileStatus)) {
            _vfsFileStatusChanged = vfsFileStatusChanged;
        }
        inline void setVfsForceStatusCallback(bool (*vfsForceStatus)(int, const SyncPath &, bool, int, bool)) {
            _vfsForceStatus = vfsForceStatus;
        }
        inline void setVfsCleanUpStatusesCallback(bool (*vfsCleanUpStatuses)(int)) { _vfsCleanUpStatuses = vfsCleanUpStatuses; }
        inline void setVfsClearFileAttributesCallback(bool (*vfsClearFileAttributes)(int, const SyncPath &)) {
            _vfsClearFileAttributes = vfsClearFileAttributes;
        }
        inline void setVfsCancelHydrateCallback(bool (*vfsCancelHydrate)(int, const SyncPath &)) {
            _vfsCancelHydrate = vfsCancelHydrate;
        }

        [[nodiscard]] inline std::shared_ptr<SyncDb> syncDb() const { return _syncDb; }
        inline const SyncPalInfo &syncInfo() const { return _syncInfo; };
        inline int syncDbId() const { return _syncInfo.syncDbId; }
        inline int driveDbId() const { return _syncInfo.driveDbId; }
        inline int driveId() const { return _syncInfo.driveId; }
        inline int accountDbId() const { return _syncInfo.accountDbId; }
        inline int userDbId() const { return _syncInfo.userDbId; }
        inline int userId() const { return _syncInfo.userId; }
        inline const std::string &driveName() const { return _syncInfo.driveName; }
        inline VirtualFileMode vfsMode() const { return _syncInfo.vfsMode; }
        inline SyncPath localPath() const { return _syncInfo.localPath; }
        inline bool restart() const { return _syncInfo.restart; };
        inline bool isAdvancedSync() const { return _syncInfo.isAdvancedSync(); }

        // TODO : not ideal, to be refactored
        bool existOnServer(const SyncPath &path) const;
        bool canShareItem(const SyncPath &path) const;

        ExitCode fileRemoteIdFromLocalPath(const SyncPath &path, NodeId &nodeId) const;
        ExitCode syncIdSet(SyncNodeType type, std::unordered_set<NodeId> &nodeIdSet);
        ExitCode setSyncIdSet(SyncNodeType type, const std::unordered_set<NodeId> &nodeIdSet);
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

        void start();
        void stop(bool pausedByUser = false, bool quit = false, bool clear = false);
        void pause();
        void unpause();

        bool isPaused(std::chrono::time_point<std::chrono::system_clock> &pauseTime) const;
        bool isIdle() const;
        bool isRunning() const;

        SyncStatus status() const;
        SyncStep step() const;

        void addError(const Error &error);
        void addCompletedItem(int syncDbId, const SyncFileItem &item);

        bool vfsIsExcluded(const SyncPath &itemPath, bool &isExcluded);
        bool vfsExclude(const SyncPath &itemPath);
        bool vfsPinState(const SyncPath &itemPath, PinState &pinState);
        bool vfsSetPinState(const SyncPath &itemPath, PinState pinState);
        bool vfsStatus(const SyncPath &itemPath, bool &isPlaceholder, bool &isHydrated, bool &isSyncing, int &progress);
        bool vfsCreatePlaceholder(const SyncPath &relativeLocalPath, const SyncFileItem &item);
        bool vfsConvertToPlaceholder(const SyncPath &path, const SyncFileItem &item, bool &needRestart);
        bool vfsUpdateMetadata(const SyncPath &path, const SyncTime &creationTime, const SyncTime &modtime, const int64_t size,
                               const NodeId &id, std::string &error);
        bool vfsUpdateFetchStatus(const SyncPath &tmpPath, const SyncPath &path, int64_t received, bool &canceled,
                                  bool &finished);
        bool vfsFileStatusChanged(const SyncPath &path, SyncFileStatus status);
        bool vfsForceStatus(const SyncPath &path, bool isSyncing, int progress, bool isHydrated = false);
        bool vfsCleanUpStatuses();
        bool vfsClearFileAttributes(const SyncPath &path);
        bool vfsCancelHydrate(const SyncPath &path);

        bool wipeVirtualFiles();
        bool wipeOldPlaceholders();

        void loadProgress(int64_t &currentFile, int64_t &totalFiles, int64_t &_completedSize, int64_t &_totalSize,
                          int64_t &estimatedRemainingTime) const;
        bool getSyncFileItem(const SyncPath &path, SyncFileItem &item);

        bool isSnapshotValid(ReplicaSide side);

        ExitCode addDlDirectJob(const SyncPath &relativePath, const SyncPath &localPath);
        ExitCode cancelDlDirectJobs(const std::list<SyncPath> &fileList);
        ExitCode cancelAllDlDirectJobs(bool quit);
        ExitCode cleanOldUploadSessionTokens();
        bool isDownloadOngoing(const SyncPath &localPath);

        inline bool syncHasFullyCompleted() const { return _syncInfo.syncHasFullyCompleted; }

        void fixInconsistentFileNames();

        void fixNodeTableDeleteItemsWithNullParentNodeId();

        virtual void increaseErrorCount(const NodeId &nodeId, NodeType type, const SyncPath &relativePath, ReplicaSide side);
        virtual int getErrorCount(const NodeId &nodeId, ReplicaSide side) const noexcept;
        virtual void blacklistTemporarily(const NodeId &nodeId, const SyncPath &relativePath, ReplicaSide side);
        virtual void refreshTmpBlacklist();
        virtual void removeItemFromTmpBlacklist(const NodeId &nodeId, ReplicaSide side);
        //! Makes copies of real-time snapshots to be used by synchronization workers.
        void copySnapshots();

        void setLocalPath(const SyncPath &path) { _syncInfo.localPath = path; };
        void setSyncHasFullyCompeleted(bool completed) { _syncInfo.syncHasFullyCompleted = completed; };
        void setRestart(bool shouldRestart) { _syncInfo.restart = shouldRestart; };
        void setVfsMode(const VirtualFileMode mode) { _syncInfo.vfsMode = mode; };
        void setIsPaused(const bool paused) { _syncInfo.isPaused = paused; }

    private:
        log4cplus::Logger _logger;
        SyncPalInfo _syncInfo;

        std::shared_ptr<ExcludeListPropagator> _excludeListPropagator = nullptr;
        std::shared_ptr<BlacklistPropagator> _blacklistPropagator = nullptr;
        std::shared_ptr<ConflictingFilesCorrector> _conflictingFilesCorrector = nullptr;

        std::unordered_map<UniqueId, std::shared_ptr<DownloadJob>> _directDownloadJobsMap;
        std::unordered_map<SyncPath, UniqueId, hashPathFunction> _syncPathToDownloadJobMap;
        std::mutex _directDownloadJobsMapMutex;

        // Callbacks
        void (*_addError)(const Error &error){nullptr};
        void (*_addCompletedItem)(int syncDbId, const SyncFileItem &item, bool notify){nullptr};
        void (*_sendSignal)(SignalNum sigId, int syncDbId, const SigValueType &val){nullptr};

        bool (*_vfsIsExcluded)(int syncDbId, const SyncPath &itemPath, bool &isExcluded){nullptr};
        bool (*_vfsExclude)(int syncDbId, const SyncPath &itemPath){nullptr};
        bool (*_vfsPinState)(int syncDbId, const SyncPath &itemPath, PinState &pinState){nullptr};
        bool (*_vfsSetPinState)(int syncDbId, const SyncPath &itemPath, PinState pinState){nullptr};
        bool (*_vfsStatus)(int syncDbId, const SyncPath &itemPath, bool &isPlaceholder, bool &isHydrated, bool &isSyncing,
                           int &progress){nullptr};
        bool (*_vfsCreatePlaceholder)(int syncDbId, const SyncPath &relativeLocalPath, const SyncFileItem &item){nullptr};
        bool (*_vfsConvertToPlaceholder)(int syncDbId, const SyncPath &path, const SyncFileItem &item,
                                         bool &needRestart){nullptr};
        bool (*_vfsUpdateMetadata)(int syncDbId, const SyncPath &path, const SyncTime &creationTime, const SyncTime &modtime,
                                   const int64_t size, const NodeId &id, std::string &error){nullptr};
        bool (*_vfsUpdateFetchStatus)(int syncDbId, const SyncPath &tmpPath, const SyncPath &path, int64_t received,
                                      bool &canceled, bool &finished){nullptr};
        bool (*_vfsFileStatusChanged)(int syncDbId, const SyncPath &path, SyncFileStatus status){nullptr};
        bool (*_vfsForceStatus)(int syncDbId, const SyncPath &path, bool isSyncing, int progress, bool isHydrated){nullptr};
        bool (*_vfsCleanUpStatuses)(int syncDbId){nullptr};
        bool (*_vfsClearFileAttributes)(int syncDbId, const SyncPath &path){nullptr};
        bool (*_vfsCancelHydrate)(int syncDbId, const SyncPath &path){nullptr};

        // DB
        std::shared_ptr<SyncDb> _syncDb{nullptr};

        // Shared objects
        std::shared_ptr<bool> _interruptSync{new bool(false)};
        std::shared_ptr<Snapshot> _localSnapshot{nullptr};   // Real time local snapshot
        std::shared_ptr<Snapshot> _remoteSnapshot{nullptr};  // Real time remote snapshot
        std::shared_ptr<Snapshot> _localSnapshotCopy{
            nullptr};  // Copy of the real time local snapshot that is used by synchronization workers
        std::shared_ptr<Snapshot> _remoteSnapshotCopy{
            nullptr};  // Copy of the real time remote snapshot that is used by synchronization workers
        std::shared_ptr<FSOperationSet> _localOperationSet{nullptr};
        std::shared_ptr<FSOperationSet> _remoteOperationSet{nullptr};
        std::shared_ptr<UpdateTree> _localUpdateTree{nullptr};
        std::shared_ptr<UpdateTree> _remoteUpdateTree{nullptr};
        std::shared_ptr<ConflictQueue> _conflictQueue{nullptr};
        std::shared_ptr<SyncOperationList> _syncOps{nullptr};

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

        std::shared_ptr<ProgressInfo> _progressInfo{nullptr};

        std::shared_ptr<TmpBlacklistManager> _tmpBlacklistManager{nullptr};

        void createSharedObjects();
        void resetSharedObjects();
        void createWorkers();
        void free();
        ExitCode setSyncPaused(bool value);
        bool createOrOpenDb(const SyncPath &syncDbPath, const std::string &version,
                            const std::string &targetNodeId = std::string());
        void setSyncHasFullyCompleted(bool syncHasFullyCompleted);
        inline bool interruptSync() const { return *_interruptSync; }
        ExitCode setListingCursor(const std::string &value, int64_t timestamp);
        ExitCode listingCursor(std::string &value, int64_t &timestamp);
        ExitCode updateSyncNode(SyncNodeType syncNodeType);
        ExitCode updateSyncNode();
        std::shared_ptr<Snapshot> snapshot(ReplicaSide side, bool copy = false);
        std::shared_ptr<FSOperationSet> operationSet(ReplicaSide side);
        std::shared_ptr<UpdateTree> updateTree(ReplicaSide side);

        // Progress info management
        void resetEstimateUpdates();
        void startEstimateUpdates();
        void stopEstimateUpdates();
        void updateEstimates();
        void initProgress(const SyncFileItem &item);
        void setProgress(const SyncPath &relativePath, int64_t current);
        void setProgressComplete(const SyncPath &relativeLocalPath, SyncFileStatus status);

        // Direct download callback
        void directDownloadCallback(UniqueId jobId);

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
        friend class ConflictResolverWorker;
        friend class OperationGeneratorWorker;
        friend class OperationSorterWorker;
        friend class ExecutorWorker;

        friend class BlacklistPropagator;
        friend class ExcludeListPropagator;
        friend class ConflictingFilesCorrector;
        friend class TmpBlacklistManager;

        friend class TestSyncPal;
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
};

}  // namespace KDC
