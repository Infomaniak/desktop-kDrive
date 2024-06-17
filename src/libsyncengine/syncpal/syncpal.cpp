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

#include "syncpal.h"
#include "comm.h"
#include "syncpal/virtualfilescleaner.h"
#include "syncpalworker.h"
#include "utility/asserts.h"
#include "syncpal/excludelistpropagator.h"
#include "syncpal/conflictingfilescorrector.h"
#include "update_detection/file_system_observer/filesystemobserverworker.h"
#ifdef _WIN32
#include "update_detection/file_system_observer/localfilesystemobserverworker_win.h"
#else
#include "update_detection/file_system_observer/localfilesystemobserverworker_unix.h"
#endif
#include "update_detection/file_system_observer/remotefilesystemobserverworker.h"
#include "update_detection/blacklist_changes_propagator/blacklistpropagator.h"
#include "update_detection/file_system_observer/computefsoperationworker.h"
#include "update_detection/update_detector/updatetreeworker.h"
#include "reconciliation/platform_inconsistency_checker/platforminconsistencycheckerworker.h"
#include "reconciliation/platform_inconsistency_checker/platforminconsistencycheckerutility.h"
#include "reconciliation/conflict_finder/conflictfinderworker.h"
#include "reconciliation/conflict_resolver/conflictresolverworker.h"
#include "reconciliation/operation_generator/operationgeneratorworker.h"
#include "propagation/operation_sorter/operationsorterworker.h"
#include "propagation/executor/executorworker.h"
#include "requests/syncnodecache.h"
#include "requests/parameterscache.h"
#include "jobs/network/downloadjob.h"
#include "jobs/network/upload_session/uploadsessioncanceljob.h"
#include "jobs/local/localmovejob.h"
#include "jobs/local/localdeletejob.h"
#include "jobs/jobmanager.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/utility/utility.h"
#include "tmpblacklistmanager.h"

#define SYNCPAL_NEW_ERROR_MSG "Failed to create SyncPal instance!"

namespace KDC {

#define SYNCDB _syncDbId

SyncPal::SyncPal(const SyncPath &syncDbPath, const std::string &version, bool hasFullyCompleted)
    : _logger(Log::instance()->getLogger()), _syncHasFullyCompleted(hasFullyCompleted) {
    LOGW_SYNCPAL_DEBUG(_logger, L"SyncPal init : " << Utility::formatSyncPath(syncDbPath).c_str());

    if (!createOrOpenDb(syncDbPath, version)) {
        throw std::runtime_error(SYNCPAL_NEW_ERROR_MSG);
    }

    createSharedObjects();
}

SyncPal::SyncPal(int syncDbId, const std::string &version) : _logger(Log::instance()->getLogger()), _syncDbId(syncDbId) {
    LOG_SYNCPAL_DEBUG(_logger, "SyncPal init");

    // Get sync
    Sync sync;
    bool found;
    if (!ParmsDb::instance()->selectSync(syncDbId, sync, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in ParmsDb::selectSync");
        throw std::runtime_error(SYNCPAL_NEW_ERROR_MSG);
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "Sync not found");
        throw std::runtime_error(SYNCPAL_NEW_ERROR_MSG);
    }
    _driveDbId = sync.driveDbId();
    _localPath = sync.localPath();
    _localPath.make_preferred();
    _targetPath = sync.targetPath();
    _targetPath.make_preferred();

    // Get drive
    Drive drive;
    if (!ParmsDb::instance()->selectDrive(_driveDbId, drive, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in ParmsDb::selectDrive");
        throw std::runtime_error(SYNCPAL_NEW_ERROR_MSG);
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "Drive not found for driveDbId=" << _driveDbId);
        throw std::runtime_error(SYNCPAL_NEW_ERROR_MSG);
    }
    _driveId = drive.driveId();
    _accountDbId = drive.accountDbId();
    _driveName = drive.name();

    // Get account
    Account account;
    if (!ParmsDb::instance()->selectAccount(_accountDbId, account, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in ParmsDb::selectAccount");
        throw std::runtime_error(SYNCPAL_NEW_ERROR_MSG);
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "Account not found for accountDbId=" << _accountDbId);
        throw std::runtime_error(SYNCPAL_NEW_ERROR_MSG);
    }
    _userDbId = account.userDbId();

    // Get user
    User user;
    if (!ParmsDb::instance()->selectUser(_userDbId, user, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in ParmsDb::selectUser");
        throw std::runtime_error(SYNCPAL_NEW_ERROR_MSG);
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "User not found for userDbId=" << _userDbId);
        throw std::runtime_error(SYNCPAL_NEW_ERROR_MSG);
    }
    _userId = user.userId();

    if (sync.dbPath().empty()) {
        // Set Db path
        bool alreadyExist;
        SyncPath dbPath = Db::makeDbName(user.userId(), account.accountId(), drive.driveId(), _syncDbId, alreadyExist);
        if (dbPath.empty()) {
            LOG_SYNCPAL_WARN(_logger, "Error in Db::makeDbName");
            throw std::runtime_error(SYNCPAL_NEW_ERROR_MSG);
        }

        if (alreadyExist) {
            // Old DB => delete it
            std::error_code ec;
            std::filesystem::remove(dbPath, ec);

            SyncPath dbPathShm(dbPath);
            dbPathShm.replace_filename(dbPathShm.filename().native() + Str("-shm"));
            std::filesystem::remove(dbPathShm, ec);

            SyncPath dbPathWal(dbPath);
            dbPathWal.replace_filename(dbPathWal.filename().native() + Str("-wal"));
            std::filesystem::remove(dbPathWal, ec);
        }

        sync.setDbPath(dbPath);

        if (!ParmsDb::instance()->updateSync(sync, found)) {
            LOG_SYNCPAL_WARN(_logger, "Error in ParmsDb::updateSync");
            throw std::runtime_error(SYNCPAL_NEW_ERROR_MSG);
        }
        if (!found) {
            LOG_SYNCPAL_WARN(_logger, "Sync not found");
            throw std::runtime_error(SYNCPAL_NEW_ERROR_MSG);
        }
    }

    if (!createOrOpenDb(sync.dbPath(), version, sync.targetNodeId())) {
        throw std::runtime_error(SYNCPAL_NEW_ERROR_MSG);
    }

#ifdef __APPLE__
    fixFileNamesWithColon(_syncDb, _localPath);
#endif
    fixInconsistentFileNames(_syncDb, _localPath);
    fixNodeTableDeleteItemsWithNullParentNodeId();

    createSharedObjects();
}

SyncPal::~SyncPal() {
    SyncNodeCache::instance()->clearCache(_syncDbId);
    LOG_SYNCPAL_DEBUG(_logger, "~SyncPal");
}

ExitCode SyncPal::setTargetNodeId(const std::string &targetNodeId) {
    bool found = false;
    if (!_syncDb->setTargetNodeId(targetNodeId, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::setTargetNodeId");
        return ExitCode::DbError;
    }
    if (!found) {
        LOGW_SYNCPAL_WARN(_logger, L"Root node not found in node table");
        return ExitCode::DataError;
    }

    _remoteSnapshot->setRootFolderId(targetNodeId);
    _remoteUpdateTree->setRootFolderId(targetNodeId);

    return ExitCode::Ok;
}

bool SyncPal::isRunning() const {
    return (_syncPalWorker && _syncPalWorker->isRunning());
}

SyncStatus SyncPal::status() const {
    if (!_syncPalWorker) {
        // Not started
        return SyncStatus::Stoped;
    } else {
        // Has started
        if (_syncPalWorker->isRunning()) {
            // Still running
            if (_syncPalWorker->pauseAsked()) {
                // Auto pausing after a NON fatal error (network, back...)
                return SyncStatus::PauseAsked;
            } else if (_syncPalWorker->isPaused()) {
                // Auto paused after a NON fatal error
                return SyncStatus::Paused;
            } else if (_syncPalWorker->stopAsked()) {
                // Stopping at the request of the user
                return SyncStatus::StopAsked;
            } else if (_syncPalWorker->step() == SyncStepIdle && !_restart && !_localSnapshot->updated() &&
                       !_remoteSnapshot->updated()) {
                // Sync pending
                return SyncStatus::Idle;
            } else {
                // Syncing
                return SyncStatus::Running;
            }
        } else {
            // Starting or exited
            if (_syncPalWorker->exitCode() == ExitCode::Unknown) {
                // Starting
                return SyncStatus::Starting;
            } else if (_syncPalWorker->exitCode() == ExitCode::Ok) {
                // Stopped at the request of the user
                return SyncStatus::Stoped;
            } else {
                // Stopped after a fatal error (DB access, system...)
                return SyncStatus::Error;
            }
        }
    }
}

SyncStep SyncPal::step() const {
    return (_syncPalWorker ? _syncPalWorker->step() : SyncStepNone);
}

ExitCode SyncPal::fileStatus(ReplicaSide side, const SyncPath &path, SyncFileStatus &status) const {
    if (_tmpBlacklistManager && _tmpBlacklistManager->isTmpBlacklisted(path, side)) {
        if (path == Utility::sharedFolderName()) {
            status = SyncFileStatusSuccess;
        } else {
            status = SyncFileStatusIgnored;
        }
        return ExitCode::Ok;
    }

    bool found;
    if (!_syncDb->status(side, path, status, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::status");
        return ExitCode::DbError;
    }
    if (!found) {
        status = SyncFileStatusUnknown;
        return ExitCode::Ok;
    }

    return ExitCode::Ok;
}

ExitCode SyncPal::fileSyncing(ReplicaSide side, const SyncPath &path, bool &syncing) const {
    bool found;
    if (!_syncDb->syncing(side, path, syncing, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::syncing");
        return ExitCode::DbError;
    }
    if (!found) {
        LOGW_SYNCPAL_WARN(_logger, L"Node not found in node table : " << Utility::formatSyncPath(path).c_str());
        return ExitCode::DataError;
    }

    return ExitCode::Ok;
}

ExitCode SyncPal::setFileSyncing(ReplicaSide side, const SyncPath &path, bool syncing) {
    bool found;
    if (!_syncDb->setSyncing(side, path, syncing, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::setSyncing");
        return ExitCode::DbError;
    }
    if (!found) {
        LOGW_SYNCPAL_WARN(_logger, L"Node not found in node table : " << Utility::formatSyncPath(path).c_str());
        return ExitCode::DataError;
    }

    return ExitCode::Ok;
}

ExitCode SyncPal::path(ReplicaSide side, const NodeId &nodeId, SyncPath &path) {
    bool found;
    if (!_syncDb->path(side, nodeId, path, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::path");
        return ExitCode::DbError;
    }

    if (!found) {
        LOGW_SYNCPAL_WARN(_logger, L"Node not found in node table : " << Utility::formatSyncPath(path).c_str());
        return ExitCode::DataError;
    }

    return ExitCode::Ok;
}

ExitCode SyncPal::clearNodes() {
    if (!_syncDb->clearNodes()) {
        return ExitCode::DbError;
    }
    return ExitCode::Ok;
}

void SyncPal::syncPalStartCallback([[maybe_unused]] UniqueId jobId) {
    auto jobPtr = JobManager::instance()->getJob(jobId);
    if (jobPtr) {
        if (jobPtr->exitCode() != ExitCode::Ok) {
            LOG_SYNCPAL_WARN(_logger, "Error in PropagatorJob");
            addError(Error(_syncDbId, ERRID, jobPtr->exitCode(), ExitCause::Unknown));
            return;
        }

        if (jobPtr->isAborted()) {
            LOG_SYNCPAL_INFO(_logger, "Job aborted, not restarting SyncPal");
            return;
        }

        std::shared_ptr<AbstractPropagatorJob> abstractJob = std::dynamic_pointer_cast<AbstractPropagatorJob>(jobPtr);
        if (abstractJob && abstractJob->restartSyncPal()) {
            LOG_SYNCPAL_INFO(_logger, "Restarting SyncPal");
            start();
        }

        if (std::dynamic_pointer_cast<BlacklistPropagator>(jobPtr)) {
            _blacklistPropagator = nullptr;
        } else if (std::dynamic_pointer_cast<ExcludeListPropagator>(jobPtr)) {
            _excludeListPropagator = nullptr;
        } else if (std::dynamic_pointer_cast<ConflictingFilesCorrector>(jobPtr)) {
            _sendSignal(SIGNAL_NUM_NODE_FIX_CONFLICTED_FILES_COMPLETED, _syncDbId, _conflictingFilesCorrector->nbErrors());
            _conflictingFilesCorrector = nullptr;
        }
    }
}

void SyncPal::addError(const Error &error) {
    if (_addError) {
        _addError(error);
    }
}

void SyncPal::addCompletedItem(int syncDbId, const SyncFileItem &item) {
    if (_addCompletedItem) {
        _addCompletedItem(syncDbId, item, syncHasFullyCompleted());
    }
}

bool SyncPal::vfsIsExcluded(const SyncPath &itemPath, bool &isExcluded) {
    if (!_vfsIsExcluded) {
        return false;
    }

    return _vfsIsExcluded(_syncDbId, itemPath, isExcluded);
}

bool SyncPal::vfsExclude(const SyncPath &itemPath) {
    if (!_vfsExclude) {
        return false;
    }

    return _vfsExclude(_syncDbId, itemPath);
}

bool SyncPal::vfsPinState(const SyncPath &itemPath, PinState &pinState) {
    if (!_vfsPinState) {
        return false;
    }

    return _vfsPinState(_syncDbId, itemPath, pinState);
}

bool SyncPal::vfsSetPinState(const SyncPath &itemPath, PinState pinState) {
    if (!_vfsPinState) {
        return false;
    }

    return _vfsSetPinState(_syncDbId, itemPath, pinState);
}

bool SyncPal::vfsStatus(const SyncPath &itemPath, bool &isPlaceholder, bool &isHydrated, bool &isSyncing, int &progress) {
    if (!_vfsStatus) {
        return false;
    }

    return _vfsStatus(_syncDbId, itemPath, isPlaceholder, isHydrated, isSyncing, progress);
}

bool SyncPal::vfsCreatePlaceholder(const SyncPath &relativeLocalPath, const SyncFileItem &item) {
    if (!_vfsCreatePlaceholder) {
        return false;
    }

    return _vfsCreatePlaceholder(_syncDbId, relativeLocalPath, item);
}

bool SyncPal::vfsConvertToPlaceholder(const SyncPath &path, const SyncFileItem &item, bool &needRestart) {
    if (!_vfsConvertToPlaceholder) {
        return false;
    }

    return _vfsConvertToPlaceholder(_syncDbId, path, item, needRestart);
}

bool SyncPal::vfsUpdateMetadata(const SyncPath &path, const SyncTime &creationTime, const SyncTime &modtime, const int64_t size,
                                const NodeId &id, std::string &error) {
    if (!_vfsUpdateMetadata) {
        return false;
    }

    return _vfsUpdateMetadata(_syncDbId, path, creationTime, modtime, size, id, error);
}

bool SyncPal::vfsUpdateFetchStatus(const SyncPath &tmpPath, const SyncPath &path, int64_t received, bool &canceled,
                                   bool &finished) {
    if (ParametersCache::isExtendedLogEnabled()) {
        LOGW_SYNCPAL_DEBUG(_logger,
                           L"vfsUpdateFetchStatus : " << Utility::formatSyncPath(path).c_str() << L" received=" << received);
    }

    if (!_vfsUpdateFetchStatus) {
        return false;
    }

    return _vfsUpdateFetchStatus(_syncDbId, tmpPath, path, received, canceled, finished);
}

bool SyncPal::vfsFileStatusChanged(const SyncPath &path, SyncFileStatus status) {
    if (!_vfsFileStatusChanged) {
        return false;
    }

    return _vfsFileStatusChanged(_syncDbId, path, status);
}

bool SyncPal::vfsForceStatus(const SyncPath &path, bool isSyncing, int progress, bool isHydrated) {
    if (!_vfsForceStatus) {
        return false;
    }

    return _vfsForceStatus(_syncDbId, path, isSyncing, progress, isHydrated);
}

bool SyncPal::vfsCleanUpStatuses() {
    if (!_vfsCleanUpStatuses) {
        return false;
    }

    return _vfsCleanUpStatuses(_syncDbId);
}

bool SyncPal::vfsClearFileAttributes(const SyncPath &path) {
    if (!_vfsClearFileAttributes) {
        return false;
    }

    return _vfsClearFileAttributes(_syncDbId, path);
}

bool SyncPal::vfsCancelHydrate(const SyncPath &path) {
    if (!_vfsCancelHydrate) {
        return false;
    }

    return _vfsCancelHydrate(_syncDbId, path);
}

bool SyncPal::wipeVirtualFiles() {
    LOG_SYNCPAL_INFO(_logger, "Wiping virtual files");
    VirtualFilesCleaner virtualFileCleaner(_localPath, _syncDbId, _syncDb, _vfsStatus, _vfsClearFileAttributes);
    if (!virtualFileCleaner.run()) {
        LOG_SYNCPAL_WARN(_logger, "Error in VirtualFilesCleaner::run");
        addError(Error(_syncDbId, ERRID, virtualFileCleaner.exitCode(), virtualFileCleaner.exitCause()));
        return false;
    }
    return true;
}

bool SyncPal::wipeOldPlaceholders() {
    LOG_SYNCPAL_INFO(_logger, "Wiping old placeholders files");
    VirtualFilesCleaner virtualFileCleaner(_localPath);
    std::vector<SyncPath> failedToRemovePlaceholders;
    if (!virtualFileCleaner.removeDehydratedPlaceholders(failedToRemovePlaceholders)) {
        LOG_SYNCPAL_WARN(_logger, "Error in VirtualFilesCleaner::removeDehydratedPlaceholders");
        for (auto &failedItem : failedToRemovePlaceholders) {
            addError(Error(_syncDbId, "", "", NodeType::File, failedItem, ConflictType::None, InconsistencyTypeNone, CancelType::None,
                           "", virtualFileCleaner.exitCode(), virtualFileCleaner.exitCause()));
        }
        return false;
    }
    return true;
}

void SyncPal::loadProgress(int64_t &currentFile, int64_t &totalFiles, int64_t &completedSize, int64_t &totalSize,
                           int64_t &estimatedRemainingTime) const {
    currentFile = _progressInfo->completedFiles();
    totalFiles = std::max(_progressInfo->completedFiles(), _progressInfo->totalFiles());
    completedSize = _progressInfo->completedSize();
    totalSize = std::max(_progressInfo->completedSize(), _progressInfo->totalSize());
    estimatedRemainingTime = _progressInfo->totalProgress().estimatedEta();
}

void SyncPal::createSharedObjects() {
    // Create shared objects
    _localSnapshot = std::shared_ptr<Snapshot>(new Snapshot(ReplicaSide::Local, _syncDb->rootNode()));
    _remoteSnapshot = std::shared_ptr<Snapshot>(new Snapshot(ReplicaSide::Remote, _syncDb->rootNode()));
    _localSnapshotCopy = std::shared_ptr<Snapshot>(new Snapshot(ReplicaSide::Local, _syncDb->rootNode()));
    _remoteSnapshotCopy = std::shared_ptr<Snapshot>(new Snapshot(ReplicaSide::Remote, _syncDb->rootNode()));
    _localOperationSet = std::shared_ptr<FSOperationSet>(new FSOperationSet());
    _remoteOperationSet = std::shared_ptr<FSOperationSet>(new FSOperationSet());
    _localUpdateTree = std::shared_ptr<UpdateTree>(new UpdateTree(ReplicaSide::Local, _syncDb->rootNode()));
    _remoteUpdateTree = std::shared_ptr<UpdateTree>(new UpdateTree(ReplicaSide::Remote, _syncDb->rootNode()));
    _conflictQueue = std::shared_ptr<ConflictQueue>(new ConflictQueue(_localUpdateTree, _remoteUpdateTree));
    _syncOps = std::shared_ptr<SyncOperationList>(new SyncOperationList());

    // Init SyncNode table cache
    SyncNodeCache::instance()->initCache(_syncDbId, _syncDb);
}

void SyncPal::resetSharedObjects() {
    LOG_SYNCPAL_DEBUG(_logger, "Reset shared objects");

    _localOperationSet->clear();
    _remoteOperationSet->clear();
    _localUpdateTree->init();
    _remoteUpdateTree->init();
    _conflictQueue->clear();
    _syncOps->clear();
    _syncHasFullyCompleted = false;

    LOG_SYNCPAL_DEBUG(_logger, "Reset shared objects done");
}

void SyncPal::createWorkers() {
#if defined(_WIN32)
    _localFSObserverWorker = std::shared_ptr<FileSystemObserverWorker>(
        new LocalFileSystemObserverWorker_win(shared_from_this(), "Local File System Observer", "LFSO"));
#else
    _localFSObserverWorker = std::shared_ptr<FileSystemObserverWorker>(
        new LocalFileSystemObserverWorker_unix(shared_from_this(), "Local File System Observer", "LFSO"));
#endif
    _remoteFSObserverWorker = std::shared_ptr<FileSystemObserverWorker>(
        new RemoteFileSystemObserverWorker(shared_from_this(), "Remote File System Observer", "RFSO"));
    _computeFSOperationsWorker = std::shared_ptr<ComputeFSOperationWorker>(
        new ComputeFSOperationWorker(shared_from_this(), "Compute FS Operations", "COOP"));
    _localUpdateTreeWorker = std::shared_ptr<UpdateTreeWorker>(
        new UpdateTreeWorker(shared_from_this(), "Local Tree Updater", "LTRU", ReplicaSide::Local));
    _remoteUpdateTreeWorker = std::shared_ptr<UpdateTreeWorker>(
        new UpdateTreeWorker(shared_from_this(), "Remote Tree Updater", "RTRU", ReplicaSide::Remote));
    _platformInconsistencyCheckerWorker = std::shared_ptr<PlatformInconsistencyCheckerWorker>(
        new PlatformInconsistencyCheckerWorker(shared_from_this(), "Platform Inconsistency Checker", "PICH"));
    _conflictFinderWorker =
        std::shared_ptr<ConflictFinderWorker>(new ConflictFinderWorker(shared_from_this(), "Conflict Finder", "COFD"));
    _conflictResolverWorker =
        std::shared_ptr<ConflictResolverWorker>(new ConflictResolverWorker(shared_from_this(), "Conflict Resolver", "CORE"));
    _operationsGeneratorWorker = std::shared_ptr<OperationGeneratorWorker>(
        new OperationGeneratorWorker(shared_from_this(), "Operation Generator", "OPGE"));
    _operationsSorterWorker =
        std::shared_ptr<OperationSorterWorker>(new OperationSorterWorker(shared_from_this(), "Operation Sorter", "OPSO"));
    _executorWorker = std::shared_ptr<ExecutorWorker>(new ExecutorWorker(shared_from_this(), "Executor", "EXEC"));
    _syncPalWorker = std::shared_ptr<SyncPalWorker>(new SyncPalWorker(shared_from_this(), "Main", "MAIN"));

    _tmpBlacklistManager = std::shared_ptr<TmpBlacklistManager>(new TmpBlacklistManager(shared_from_this()));
}

void SyncPal::free() {
    _localFSObserverWorker.reset();
    _remoteFSObserverWorker.reset();
    _computeFSOperationsWorker.reset();
    _localUpdateTreeWorker.reset();
    _remoteUpdateTreeWorker.reset();
    _platformInconsistencyCheckerWorker.reset();
    _conflictFinderWorker.reset();
    _conflictResolverWorker.reset();
    _operationsGeneratorWorker.reset();
    _operationsSorterWorker.reset();
    _executorWorker.reset();
    _syncPalWorker.reset();
    _tmpBlacklistManager.reset();

    _interruptSync.reset();
    _localSnapshot.reset();
    _remoteSnapshot.reset();
    _localSnapshotCopy.reset();
    _remoteSnapshotCopy.reset();
    _localOperationSet.reset();
    _remoteOperationSet.reset();
    _localUpdateTree.reset();
    _remoteUpdateTree.reset();
    _conflictQueue.reset();
    _syncOps.reset();

    // Check that there is no memory leak
    ASSERT(_interruptSync.use_count() == 0);
    ASSERT(_localSnapshot.use_count() == 0);
    ASSERT(_remoteSnapshot.use_count() == 0);
    ASSERT(_localSnapshotCopy.use_count() == 0);
    ASSERT(_remoteSnapshotCopy.use_count() == 0);
    ASSERT(_localOperationSet.use_count() == 0);
    ASSERT(_remoteOperationSet.use_count() == 0);
    ASSERT(_localUpdateTree.use_count() == 0);
    ASSERT(_remoteUpdateTree.use_count() == 0);
    ASSERT(_conflictQueue.use_count() == 0);
    ASSERT(_syncOps.use_count() == 0);
}

ExitCode SyncPal::setSyncPaused(bool value) {
    bool found;
    if (!ParmsDb::instance()->setSyncPaused(_syncDbId, value, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in ParmsDb::setSyncPaused");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "Sync not found");
        return ExitCode::DataError;
    }

    return ExitCode::Ok;
}

bool SyncPal::createOrOpenDb(const SyncPath &syncDbPath, const std::string &version, const std::string &targetNodeId) {
    // Create/open sync DB
    try {
        _syncDb = std::shared_ptr<SyncDb>(new SyncDb(syncDbPath.string(), version, targetNodeId));
    } catch (std::exception const &) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::SyncDb");
        return false;
    }

    return true;
}

void SyncPal::resetEstimateUpdates() {
    _progressInfo->reset();
}

void SyncPal::startEstimateUpdates() {
    _progressInfo->setUpdate(true);
}

void SyncPal::stopEstimateUpdates() {
    _progressInfo->setUpdate(false);
}

void SyncPal::updateEstimates() {
    _progressInfo->updateEstimates();
}

void SyncPal::initProgress(const SyncFileItem &item) {
    _progressInfo->initProgress(item);
}

void SyncPal::setProgress(const SyncPath &relativePath, int64_t current) {
    _progressInfo->setProgress(relativePath, current);

    bool found;
    if (!_syncDb->setStatus(ReplicaSide::Remote, relativePath, SyncFileStatusSyncing, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::setStatus");
        return;
    }
    if (!found) {
        SyncFileItem item;
        if (!getSyncFileItem(relativePath, item)) {
            LOG_SYNCPAL_WARN(_logger, "Error in SyncPal::getSyncFileItem");
            return;
        }
        if (item.instruction() != SyncFileInstructionGet && item.instruction() != SyncFileInstructionPut) {
            LOGW_SYNCPAL_WARN(_logger, L"Node not found : " << Utility::formatSyncPath(relativePath).c_str());
            return;
        }
    }
}

void SyncPal::setProgressComplete(const SyncPath &relativeLocalPath, SyncFileStatus status) {
    _progressInfo->setProgressComplete(relativeLocalPath, status);
    vfsFileStatusChanged(_localPath / relativeLocalPath, status);

    bool found;
    if (!_syncDb->setStatus(ReplicaSide::Local, relativeLocalPath, status, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::setStatus");
        return;
    }
    if (!found) {
        // Can happen for a dehydrated placeholder
        LOGW_SYNCPAL_DEBUG(_logger, L"Node not found : " << Utility::formatSyncPath(relativeLocalPath).c_str());
        return;
    }
}

void SyncPal::directDownloadCallback(UniqueId jobId) {
    const std::lock_guard<std::mutex> lock(_directDownloadJobsMapMutex);
    auto directDownloadJobsMapIt = _directDownloadJobsMap.find(jobId);
    if (directDownloadJobsMapIt == _directDownloadJobsMap.end()) {
        // No need to send a warning, the job might have been cancelled and therefor not in the map anymore
        return;
    }

    if (directDownloadJobsMapIt->second->getStatusCode() == Poco::Net::HTTPResponse::HTTP_NOT_FOUND) {
        Error error;
        error.setLevel(ErrorLevelNode);
        error.setSyncDbId(syncDbId());
        error.setRemoteNodeId(directDownloadJobsMapIt->second->remoteNodeId());
        error.setPath(directDownloadJobsMapIt->second->localPath());
        error.setExitCode(ExitCode::BackError);
        error.setExitCause(ExitCause::NotFound);
        addError(error);

        vfsCancelHydrate(directDownloadJobsMapIt->second->localPath());
    }

    _syncPathToDownloadJobMap.erase(directDownloadJobsMapIt->second->affectedFilePath());
    _directDownloadJobsMap.erase(directDownloadJobsMapIt);
}

bool SyncPal::getSyncFileItem(const SyncPath &path, SyncFileItem &item) {
    return _progressInfo->getSyncFileItem(path, item);
}

bool SyncPal::isSnapshotValid(ReplicaSide side) {
    return side == ReplicaSide::Local ? _localSnapshot->isValid() : _remoteSnapshot->isValid();
}

ExitCode SyncPal::addDlDirectJob(const SyncPath &relativePath, const SyncPath &localPath) {
    std::optional<NodeId> localNodeId = std::nullopt;
    bool found = false;
    if (!_syncDb->id(ReplicaSide::Local, relativePath, localNodeId, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::id");
        return ExitCode::DbError;
    }
    if (!found) {
        LOGW_SYNCPAL_WARN(_logger, L"Node not found in node table : " << Utility::formatSyncPath(relativePath).c_str());
        return ExitCode::DataError;
    }

    NodeId remoteNodeId;
    if (!_syncDb->correspondingNodeId(ReplicaSide::Local, *localNodeId, remoteNodeId, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::correspondingNodeId");
        return ExitCode::DbError;
    }
    if (!found) {
        LOGW_SYNCPAL_WARN(_logger, L"Node not found in node table for localNodeId=" << Utility::s2ws(*localNodeId).c_str());
        return ExitCode::DataError;
    }

    int64_t expectedSize = -1;
    if (!_syncDb->size(ReplicaSide::Local, *localNodeId, expectedSize, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::size");
        return ExitCode::DbError;
    }
    if (!found) {
        LOGW_SYNCPAL_WARN(_logger, L"Node not found in node table for localNodeId=" << Utility::s2ws(*localNodeId).c_str());
        return ExitCode::DataError;
    }

    // Hydration job
    std::shared_ptr<DownloadJob> job = nullptr;
    try {
        job = std::make_shared<DownloadJob>(_driveDbId, remoteNodeId, localPath, expectedSize);
        if (!job) {
            LOG_SYNCPAL_WARN(_logger, "Memory allocation error");
            return ExitCode::SystemError;
        }
        job->setAffectedFilePath(localPath);
    } catch (std::exception const &) {
        LOG_SYNCPAL_WARN(Log::instance()->getLogger(), "Error in DownloadJob::DownloadJob");
        addError(Error(_syncDbId, ERRID, ExitCode::Unknown, ExitCause::Unknown));
        return ExitCode::Unknown;
    }

    if (_vfsMode == VirtualFileModeMac || _vfsMode == VirtualFileModeWin) {
        // Set callbacks
        std::function<bool(const SyncPath &, const SyncPath &, int64_t, bool &, bool &)> vfsUpdateFetchStatusCallback =
            std::bind(&SyncPal::vfsUpdateFetchStatus, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                      std::placeholders::_4, std::placeholders::_5);
        job->setVfsUpdateFetchStatusCallback(vfsUpdateFetchStatusCallback);

#ifdef __APPLE__
        // Not done in Windows case: the pin state and the status must not be set by the download job because hydration could be
        // asked for a move and so, the file place will change just after the dl.
        std::function<bool(const SyncPath &, PinState)> vfsSetPinStateCallback =
            std::bind(&SyncPal::vfsSetPinState, this, std::placeholders::_1, std::placeholders::_2);
        job->setVfsSetPinStateCallback(vfsSetPinStateCallback);

        std::function<bool(const SyncPath &, bool, int, bool)> vfsForceStatusCallback =
            std::bind(&SyncPal::vfsForceStatus, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                      std::placeholders::_4);
        job->setVfsForceStatusCallback(vfsForceStatusCallback);
#endif

        std::function<bool(const SyncPath &, const SyncTime &, const SyncTime &, const int64_t, const NodeId &, std::string &)>
            vfsUpdateMetadataCallback =
                std::bind(&SyncPal::vfsUpdateMetadata, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                          std::placeholders::_4, std::placeholders::_5, std::placeholders::_6);
        job->setVfsUpdateMetadataCallback(vfsUpdateMetadataCallback);

        std::function<bool(const SyncPath &)> vfsCancelHydrateCallback =
            std::bind(&SyncPal::vfsCancelHydrate, this, std::placeholders::_1);
        job->setVfsCancelHydrateCallback(vfsCancelHydrateCallback);
    }

    // Queue job
    std::function<void(UniqueId)> callback = std::bind(&SyncPal::directDownloadCallback, this, std::placeholders::_1);
    JobManager::instance()->queueAsyncJob(job, Poco::Thread::PRIO_HIGH, callback);

    _directDownloadJobsMapMutex.lock();
    _directDownloadJobsMap.insert({job->jobId(), job});
    _syncPathToDownloadJobMap.insert({localPath, job->jobId()});
    _directDownloadJobsMapMutex.unlock();

    return ExitCode::Ok;
}

ExitCode SyncPal::cancelDlDirectJobs(const std::list<SyncPath> &fileList) {
    for (const auto &filePath : fileList) {
        const std::lock_guard<std::mutex> lock(_directDownloadJobsMapMutex);
        auto itId = _syncPathToDownloadJobMap.find(filePath);
        if (itId != _syncPathToDownloadJobMap.end()) {
            auto itJob = _directDownloadJobsMap.find(itId->second);
            if (itJob != _directDownloadJobsMap.end()) {
                itJob->second->abort();
                _directDownloadJobsMap.erase(itJob);
            }
            _syncPathToDownloadJobMap.erase(itId);
        }
    }

    return ExitCode::Ok;
}

ExitCode SyncPal::cancelAllDlDirectJobs(bool quit) {
    LOG_SYNCPAL_INFO(_logger, "Cancelling all direct download jobs");

    const std::lock_guard<std::mutex> lock(_directDownloadJobsMapMutex);
    for (auto &directDownloadJobsMapElt : _directDownloadJobsMap) {
        LOG_SYNCPAL_DEBUG(_logger, "Cancelling download job " << directDownloadJobsMapElt.first);
        if (quit) {
            // Reset callbacks
            directDownloadJobsMapElt.second->setVfsUpdateFetchStatusCallback(nullptr);
            directDownloadJobsMapElt.second->setVfsSetPinStateCallback(nullptr);
            directDownloadJobsMapElt.second->setVfsForceStatusCallback(nullptr);
            directDownloadJobsMapElt.second->setVfsUpdateMetadataCallback(nullptr);
            directDownloadJobsMapElt.second->setVfsCancelHydrateCallback(nullptr);
            directDownloadJobsMapElt.second->setAdditionalCallback(nullptr);
        }
        directDownloadJobsMapElt.second->abort();
    }

    _directDownloadJobsMap.clear();
    _syncPathToDownloadJobMap.clear();

    LOG_SYNCPAL_INFO(_logger, "Cancelling all direct download jobs done");

    return ExitCode::Ok;
}

void SyncPal::setSyncHasFullyCompleted(bool syncHasFullyCompleted) {
    _syncHasFullyCompleted = syncHasFullyCompleted;

    bool found;
    if (!ParmsDb::instance()->setSyncHasFullyCompleted(_syncDbId, syncHasFullyCompleted, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in ParmsDb::setSyncHasFullyCompleted");
        return;
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "Sync not found");
        return;
    }
}

ExitCode SyncPal::setListingCursor(const std::string &value, int64_t timestamp) {
    Sync sync;
    bool found;
    if (!ParmsDb::instance()->selectSync(_syncDbId, sync, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in ParmsDb::selectSync");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "Sync not found");
        return ExitCode::DataError;
    }

    sync.setListingCursor(value, timestamp);
    if (!ParmsDb::instance()->updateSync(sync, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in ParmsDb::updateSync");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "Sync not found");
        return ExitCode::DataError;
    }

    return ExitCode::Ok;
}

ExitCode SyncPal::listingCursor(std::string &value, int64_t &timestamp) {
    Sync sync;
    bool found;
    if (!ParmsDb::instance()->selectSync(_syncDbId, sync, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in ParmsDb::selectSync");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "Sync not found");
        return ExitCode::DataError;
    }

    sync.listingCursor(value, timestamp);
    return ExitCode::Ok;
}

ExitCode SyncPal::updateSyncNode(SyncNodeType syncNodeType) {
    // Remove deleted nodes from sync_node table & cache
    std::unordered_set<NodeId> nodeIdSet;
    ExitCode exitCode = SyncNodeCache::instance()->syncNodes(_syncDbId, syncNodeType, nodeIdSet);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Error in SyncNodeCache::syncNodes");
        return exitCode;
    }

    auto nodeIdIt = nodeIdSet.begin();
    while (nodeIdIt != nodeIdSet.end()) {
        const bool ok = syncNodeType == SyncNodeType::TmpLocalBlacklist ? _localSnapshotCopy->exists(*nodeIdIt)
                                                                      : _remoteSnapshotCopy->exists(*nodeIdIt);
        if (!ok) {
            nodeIdIt = nodeIdSet.erase(nodeIdIt);
        } else {
            nodeIdIt++;
        }
    }

    exitCode = SyncNodeCache::instance()->update(_syncDbId, syncNodeType, nodeIdSet);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Error in SyncNodeCache::update");
        return exitCode;
    }

    return ExitCode::Ok;
}

ExitCode SyncPal::updateSyncNode() {
    for (int SyncNodeTypeIdx = enumClassToInt(SyncNodeType::WhiteList);
         SyncNodeTypeIdx <= enumClassToInt(SyncNodeType::UndecidedList); SyncNodeTypeIdx++) {
        auto syncNodeType = static_cast<SyncNodeType>(SyncNodeTypeIdx);

        ExitCode exitCode = updateSyncNode(syncNodeType);
        if (exitCode != ExitCode::Ok) {
            LOG_WARN(Log::instance()->getLogger(),
                     "Error in SyncPal::updateSyncNode for syncNodeType=" << enumClassToInt(syncNodeType));
            return exitCode;
        }
    }

    return ExitCode::Ok;
}

std::shared_ptr<Snapshot> SyncPal::snapshot(ReplicaSide side, bool copy) {
    if (copy) {
        return (side == ReplicaSide::Local ? _localSnapshotCopy : _remoteSnapshotCopy);
    } else {
        return (side == ReplicaSide::Local ? _localSnapshot : _remoteSnapshot);
    }
}

std::shared_ptr<FSOperationSet> SyncPal::operationSet(ReplicaSide side) {
    return (side == ReplicaSide::Local ? _localOperationSet : _remoteOperationSet);
}

std::shared_ptr<UpdateTree> SyncPal::updateTree(ReplicaSide side) {
    return (side == ReplicaSide::Local ? _localUpdateTree : _remoteUpdateTree);
}

ExitCode SyncPal::fileRemoteIdFromLocalPath(const SyncPath &path, NodeId &nodeId) const {
    DbNodeId dbNodeId = -1;
    bool found = false;
    if (!_syncDb->dbId(ReplicaSide::Local, path, dbNodeId, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbId");
        return ExitCode::DbError;
    }
    if (!found) {
        LOGW_SYNCPAL_WARN(_logger, L"Node not found in node table : " << Utility::formatSyncPath(path).c_str());
        return ExitCode::DataError;
    }

    if (!_syncDb->id(ReplicaSide::Remote, dbNodeId, nodeId, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::id");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "Node not found in node table for ID=" << dbNodeId);
        return ExitCode::DataError;
    }

    return ExitCode::Ok;
}

bool SyncPal::existOnServer(const SyncPath &path) const {
    NodeId nodeId = _remoteSnapshot->itemId(path);
    return !nodeId.empty();
}

bool SyncPal::canShareItem(const SyncPath &path) const {
    NodeId nodeId = _remoteSnapshot->itemId(path);
    if (!nodeId.empty()) {
        return _remoteSnapshot->canShare(nodeId);
    }

    return false;
}

ExitCode SyncPal::syncIdSet(SyncNodeType type, std::unordered_set<NodeId> &nodeIdSet) {
    ExitCode exitCode = SyncNodeCache::instance()->syncNodes(_syncDbId, type, nodeIdSet);
    if (exitCode != ExitCode::Ok) {
        LOG_SYNCPAL_WARN(Log::instance()->getLogger(), "Error in SyncNodeCache::syncNodes");
        return exitCode;
    }

    return ExitCode::Ok;
}

ExitCode SyncPal::setSyncIdSet(SyncNodeType type, const std::unordered_set<NodeId> &nodeIdSet) {
    ExitCode exitCode = SyncNodeCache::instance()->update(_syncDbId, type, nodeIdSet);
    if (exitCode != ExitCode::Ok) {
        LOG_SYNCPAL_WARN(Log::instance()->getLogger(), "Error in SyncNodeCache::syncNodes");
        return exitCode;
    }

    return ExitCode::Ok;
}

ExitCode SyncPal::syncListUpdated(bool restartSync) {
    restartSync &= isRunning();
    if (restartSync) {
        stop();
    }

    if (_blacklistPropagator) {
        LOG_SYNCPAL_INFO(_logger, "BlacklistPropagator is already running, aborting the current process");
        _blacklistPropagator->abort();
        restartSync = _blacklistPropagator->restartSyncPal();
    }

    _blacklistPropagator.reset(new BlacklistPropagator(shared_from_this()));
    _blacklistPropagator->setRestartSyncPal(restartSync);
    std::function<void(UniqueId)> callback = std::bind(&SyncPal::syncPalStartCallback, this, std::placeholders::_1);
    JobManager::instance()->queueAsyncJob(_blacklistPropagator, Poco::Thread::PRIO_HIGHEST, callback);

    return ExitCode::Ok;
}

ExitCode SyncPal::excludeListUpdated() {
    bool restartSync = isRunning();
    if (restartSync) {
        stop();
    }

    if (_excludeListPropagator) {
        LOG_SYNCPAL_INFO(_logger, "ExcludeListPropagator is already running, aborting the current process");
        _excludeListPropagator->abort();
        restartSync = _excludeListPropagator->restartSyncPal();
    }

    _excludeListPropagator.reset(new ExcludeListPropagator(shared_from_this()));
    _excludeListPropagator->setRestartSyncPal(restartSync);
    std::function<void(UniqueId)> callback = std::bind(&SyncPal::syncPalStartCallback, this, std::placeholders::_1);
    JobManager::instance()->queueAsyncJob(_excludeListPropagator, Poco::Thread::PRIO_HIGHEST, callback);

    return ExitCode::Ok;
}

ExitCode SyncPal::fixConflictingFiles(bool keepLocalVersion, std::vector<Error> &errorList) {
    bool restartSync = isRunning();
    if (restartSync) {
        stop();
    }

    if (_conflictingFilesCorrector) {
        LOG_SYNCPAL_INFO(_logger, "ConflictingFilesCorrector is already running, aborting the current process");
        _conflictingFilesCorrector->abort();
        restartSync = _conflictingFilesCorrector->restartSyncPal();
    }

    _conflictingFilesCorrector.reset(new ConflictingFilesCorrector(shared_from_this(), keepLocalVersion, errorList));
    _conflictingFilesCorrector->setRestartSyncPal(restartSync);
    std::function<void(UniqueId)> callback = std::bind(&SyncPal::syncPalStartCallback, this, std::placeholders::_1);
    JobManager::instance()->queueAsyncJob(_conflictingFilesCorrector, Poco::Thread::PRIO_HIGHEST, callback);

    return ExitCode::Ok;
}

ExitCode SyncPal::fixCorruptedFile(const std::unordered_map<NodeId, SyncPath> &localFileMap) {
    for (const auto &localFileInfo : localFileMap) {
        SyncPath destPath;
        if (ExitCode exitCode = PlatformInconsistencyCheckerUtility::renameLocalFile(
                localFileInfo.second, PlatformInconsistencyCheckerUtility::SuffixTypeConflict, &destPath);
            exitCode != ExitCode::Ok) {
            LOGW_SYNCPAL_WARN(_logger, L"Fail to rename " << Path2WStr(localFileInfo.second).c_str() << L" into "
                                                          << Path2WStr(destPath).c_str());

            return exitCode;
        }

        DbNodeId dbId = -1;
        bool found = false;
        if (!_syncDb->dbId(ReplicaSide::Local, localFileInfo.first, dbId, found)) {
            LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbId for nodeId=" << localFileInfo.first.c_str());
            return ExitCode::DbError;
        }
        if (found) {
            if (!_syncDb->deleteNode(dbId, found)) {
                LOG_SYNCPAL_ERROR(_logger, "Error in SyncDb::deleteNode for DB node ID=" << dbId);
                return ExitCode::DbError;
            }
        }

        // Ok if not found, we do not want this node in the DB anymore
    }

    return ExitCode::Ok;
}

void SyncPal::start() {
    LOG_SYNCPAL_DEBUG(_logger, "SyncPal start");

    // Load VFS mode
    Sync sync;
    bool found;
    if (!ParmsDb::instance()->selectSync(_syncDbId, sync, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in ParmsDb::selectSync");
        addError(Error(_syncDbId, ERRID, ExitCode::DbError, ExitCause::Unknown));
        return;
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "Sync not found in sync table for syncDbId=" << _syncDbId);
        addError(Error(_syncDbId, ERRID, ExitCode::DataError, ExitCause::Unknown));
        return;
    }
    _vfsMode = sync.virtualFileMode();

    // Reset shared objects
    resetSharedObjects();

    // Clear tmp blacklist
    SyncNodeCache::instance()->update(_syncDbId, SyncNodeType::TmpRemoteBlacklist, std::unordered_set<NodeId>());
    SyncNodeCache::instance()->update(_syncDbId, SyncNodeType::TmpLocalBlacklist, std::unordered_set<NodeId>());

    // Create ProgressInfo
    _progressInfo = std::shared_ptr<ProgressInfo>(new ProgressInfo(shared_from_this()));

    // Create workers
    createWorkers();

    // Reset paused flag
    ExitCode exitCode = setSyncPaused(false);
    if (exitCode != ExitCode::Ok) {
        LOG_SYNCPAL_DEBUG(_logger, "Error in SyncPal::setSyncPaused");
        addError(Error(_syncDbId, ERRID, exitCode, ExitCause::Unknown));
        return;
    }

    exitCode = cleanOldUploadSessionTokens();
    if (exitCode != ExitCode::Ok) {
        LOG_SYNCPAL_DEBUG(_logger, "Error in SyncPal::cleanOldUploadSessionTokens");
        addError(Error(_syncDbId, ERRID, exitCode, ExitCause::Unknown));
    }

    // Start main worker
    _syncPalWorker->start();

    LOG_SYNCPAL_DEBUG(_logger, "SyncPal started");
}

void SyncPal::pause() {
    if (_syncPalWorker) {
        if (!_syncPalWorker->isRunning()) {
            LOG_SYNCPAL_DEBUG(_logger, "SyncPal is not running");
            return;
        }

        // Pause main worker
        _syncPalWorker->pause();

        _isPaused = true;
    }
}

void SyncPal::unpause() {
    if (_syncPalWorker) {
        if (!_syncPalWorker->isRunning()) {
            LOG_SYNCPAL_DEBUG(_logger, "SyncPal is not running");
            return;
        }

        ExitCode exitCode = cleanOldUploadSessionTokens();
        if (exitCode != ExitCode::Ok) {
            LOG_SYNCPAL_DEBUG(_logger, "Error in SyncPal::cleanOldUploadSessionTokens");
            addError(Error(_syncDbId, ERRID, exitCode, ExitCause::Unknown));
        }

        // Unpause main worker
        _syncPalWorker->unpause();

        _isPaused = false;
    }
}

void SyncPal::stop(bool pausedByUser, bool quit, bool clear) {
    if (_syncPalWorker) {
        if (_syncPalWorker->isRunning()) {
            // Stop main worker
            _syncPalWorker->stop();
            _syncPalWorker->waitForExit();
        }
    }

    // Stop direct download jobs
    cancelAllDlDirectJobs(quit);

    if (pausedByUser) {
        // Set paused flag
        ExitCode exitCode = setSyncPaused(true);
        if (exitCode != ExitCode::Ok) {
            LOG_SYNCPAL_DEBUG(_logger, "Error in SyncPal::setSyncPaused");
            addError(Error(_syncDbId, ERRID, exitCode, ExitCause::Unknown));
        }
    }

    if (quit) {
        // Free workers
        free();

        // Free progressInfo
        _progressInfo.reset();
    }

    _syncDb->setAutoDelete(clear);
}

bool SyncPal::isPaused(std::chrono::time_point<std::chrono::system_clock> &pauseTime) const {
    if (_syncPalWorker && _syncPalWorker->isPaused()) {
        pauseTime = _syncPalWorker->pauseTime();
        return true;
    }

    return false;
}

bool SyncPal::isIdle() const {
    if (_syncPalWorker) {
        return (_syncPalWorker->step() == SyncStepIdle && _restart == false);
    }
    return false;
}

ExitCode SyncPal::cleanOldUploadSessionTokens() {
    std::vector<UploadSessionToken> uploadSessionTokenList;
    if (!_syncDb->selectAllUploadSessionTokens(uploadSessionTokenList)) {
        LOG_WARN(_logger, "Error in SyncDb::selectAllUploadSessionTokens");
        return ExitCode::DbError;
    }

    for (auto &uploadSessionToken : uploadSessionTokenList) {
        try {
            auto job = std::make_shared<UploadSessionCancelJob>(_driveDbId, "", uploadSessionToken.token());
            ExitCode exitCode = job->runSynchronously();
            if (exitCode != ExitCode::Ok) {
                LOG_SYNCPAL_WARN(_logger, "Error in UploadSessionCancelJob::runSynchronously : " << enumClassToInt(exitCode));
                if (exitCode == ExitCode::NetworkError) {
                    return exitCode;
                }
            }

            if (job->hasHttpError()) {
                LOGW_SYNCPAL_WARN(_logger, L"Upload Session Token: " << uploadSessionToken.token().c_str()
                                                                     << " has already been canceled or has expired.");
            }
        } catch (std::exception const &e) {
            LOG_WARN(_logger, "Error in UploadSessionCancelJob: " << e.what());
            return ExitCode::BackError;
        }
    }

    if (!_syncDb->deleteAllUploadSessionToken()) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::selectAllUploadSessionTokens");
        return ExitCode::DbError;
    }

    return ExitCode::Ok;
}

bool SyncPal::isDownloadOngoing(const SyncPath &localPath) {
    if (_syncPathToDownloadJobMap.find(localPath) != _syncPathToDownloadJobMap.end()) {
        return true;
    }

    for (const auto &[path, _] : _syncPathToDownloadJobMap) {
        if (CommonUtility::isSubDir(localPath, path)) {
            return true;
        }
    }

    return false;
}

#ifdef __APPLE__
void SyncPal::fixFileNamesWithColon(std::shared_ptr<SyncDb> syncDb, const SyncPath &localPath) {
    if (syncDb->fromVersion().empty()) {
        // New DB
        return;
    }

    // Fix file names with ':' character
    std::string dbFromVersionNumber = CommonUtility::dbVersionNumber(syncDb->fromVersion());
    if (CommonUtility::isVersionLower(dbFromVersionNumber, "3.4.2")) {
        LOG_DEBUG(KDC::Log::instance()->getLogger(), "Fix file names with colon");

        std::vector<DbNode> dbNodeList;
        if (!syncDb->selectAllRenamedNodes(dbNodeList, true)) {
            LOG_WARN(KDC::Log::instance()->getLogger(), "Error in SyncDb::selectAllRenamedNodes");
            return;
        }

        LOG_DEBUG(KDC::Log::instance()->getLogger(), "Rename " << dbNodeList.size() << " files");
        for (DbNode &dbNode : dbNodeList) {
            // Get old local path
            SyncPath oldLocalPath;
            SyncPath remotePath;
            bool found;
            if (!syncDb->path(dbNode.nodeId(), oldLocalPath, remotePath, found)) {
                LOG_WARN(KDC::Log::instance()->getLogger(), "Error in SyncDb::path");
                continue;
            }
            if (!found) {
                LOG_WARN(KDC::Log::instance()->getLogger(), "Node not found for id=" << dbNode.nodeId());
                continue;
            }

            // Get new local name
            SyncName newLocalName;
            if (!PlatformInconsistencyCheckerUtility::instance()->fixNameForbiddenChars(dbNode.nameRemote(), newLocalName)) {
                // No fix done
                newLocalName = dbNode.nameRemote();
            }

            // Update local name in DB
            dbNode.setNameLocal(newLocalName);
            if (!syncDb->updateNode(dbNode, found)) {
                LOG_WARN(KDC::Log::instance()->getLogger(), "Error in SyncDb::updateNode");
                continue;
            }
            if (!found) {
                LOG_WARN(KDC::Log::instance()->getLogger(), "Node not found for id=" << dbNode.nodeId());
                continue;
            }

            // Get new local path
            SyncPath newLocalPath;
            if (!syncDb->path(dbNode.nodeId(), newLocalPath, remotePath, found)) {
                LOG_WARN(KDC::Log::instance()->getLogger(), "Error in SyncDb::path");
                continue;
            }
            if (!found) {
                LOG_WARN(KDC::Log::instance()->getLogger(), "Node not found for id=" << dbNode.nodeId());
                continue;
            }

            // Rename file
            LOGW_DEBUG(KDC::Log::instance()->getLogger(),
                       L"Rename " << Utility::formatSyncPath(localPath / oldLocalPath).c_str() << L" to "
                                  << Utility::formatSyncPath(localPath / newLocalPath).c_str());
            LocalMoveJob moveJob(localPath / oldLocalPath, localPath / newLocalPath);
            moveJob.runSynchronously();
        }
    }
}
#endif

void SyncPal::fixInconsistentFileNames(std::shared_ptr<SyncDb> syncDb, const SyncPath &path) {
    if (syncDb->fromVersion().empty()) {
        // New DB
        return;
    }

    std::string dbFromVersionNumber = CommonUtility::dbVersionNumber(syncDb->fromVersion());
    if (CommonUtility::isVersionLower(dbFromVersionNumber, "3.4.9")) {
        LOG_DEBUG(KDC::Log::instance()->getLogger(), "Fix inconsistent file names");

        std::vector<DbNode> dbNodeList;
        if (!syncDb->selectAllRenamedNodes(dbNodeList, false)) {
            LOG_WARN(KDC::Log::instance()->getLogger(), "Error in SyncDb::selectAllRenamedNodes");
            return;
        }

        LOG_DEBUG(KDC::Log::instance()->getLogger(), "Delete " << dbNodeList.size() << " files");
        for (DbNode &dbNode : dbNodeList) {
            SyncPath oldLocalPath;
            SyncPath remotePath;
            bool found;
            if (!syncDb->path(dbNode.nodeId(), oldLocalPath, remotePath, found)) {
                LOG_WARN(KDC::Log::instance()->getLogger(), "Error in SyncDb::path");
                continue;
            }
            if (!found) {
                LOG_WARN(KDC::Log::instance()->getLogger(), "Node not found for id=" << dbNode.nodeId());
                continue;
            }

            found = false;
            if (!syncDb->deleteNode(dbNode.nodeId(), found)) {
                LOG_WARN(KDC::Log::instance()->getLogger(), "Node not found for id=" << dbNode.nodeId());
                continue;
            }
            if (!found) {
                LOG_WARN(KDC::Log::instance()->getLogger(), "Node not found for id=" << dbNode.nodeId());
                continue;
            }

            LocalDeleteJob deleteJob(_driveDbId, path, oldLocalPath, false, dbNode.nodeIdRemote().value(), true);
            deleteJob.setBypassCheck(true);
            deleteJob.runSynchronously();
        }
    }
}

void SyncPal::fixNodeTableDeleteItemsWithNullParentNodeId() {
    // Delete nodes with parentNodeId == NULL
    LOG_DEBUG(KDC::Log::instance()->getLogger(), "Delete nodes with parentNodeId == NULL");

    if (!_syncDb->deleteNodesWithNullParentNodeId()) {
        LOG_WARN(KDC::Log::instance()->getLogger(), "Error in SyncDb::deleteNodesWithNullParentNodeId");
        return;
    }
}

void SyncPal::increaseErrorCount(const NodeId &nodeId, NodeType type, const SyncPath &relativePath, ReplicaSide side) {
    _tmpBlacklistManager->increaseErrorCount(nodeId, type, relativePath, side);
}

int SyncPal::getErrorCount(const NodeId &nodeId, ReplicaSide side) const noexcept {
    return _tmpBlacklistManager->getErrorCount(nodeId, side);
}

void SyncPal::blacklistTemporarily(const NodeId &nodeId, const SyncPath &relativePath, ReplicaSide side) {
    _tmpBlacklistManager->blacklistItem(nodeId, relativePath, side);
}

void SyncPal::refreshTmpBlacklist() {
    _tmpBlacklistManager->refreshBlacklist();
}

void SyncPal::removeItemFromTmpBlacklist(const NodeId &nodeId, ReplicaSide side) {
    _tmpBlacklistManager->removeItemFromTmpBlacklist(nodeId, side);
}

void SyncPal::copySnapshots() {
    _localSnapshotCopy = _localSnapshot;
    _remoteSnapshotCopy = _remoteSnapshot;
}

}  // namespace KDC
