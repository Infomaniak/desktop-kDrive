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

#include "syncpal.h"
#include "comm.h"
#include "syncpal/virtualfilescleaner.h"
#include "syncpalworker.h"
#include "libcommon/utility/logiffail.h"
#include "syncpal/excludelistpropagator.h"
#include "syncpal/conflictingfilescorrector.h"
#include "update_detection/file_system_observer/filesystemobserverworker.h"
#if defined(KD_WINDOWS)
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
#include "jobs/network/kDrive_API/downloadjob.h"
#include "jobs/local/localdeletejob.h"
#include "jobs/jobmanager.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/io/iohelper.h"
#include "tmpblacklistmanager.h"
#include "jobs/network/kDrive_API/upload/upload_session/uploadsessioncanceljob.h"

#define SYNCPAL_NEW_ERROR_MSG "Failed to create SyncPal instance!"

namespace KDC {

SyncPal::SyncPal(std::shared_ptr<Vfs> vfs, const SyncPath &syncDbPath, const std::string &version, const bool hasFullyCompleted) :
    _vfs(vfs),
    _logger(Log::instance()->getLogger()) {
    _syncInfo.syncHasFullyCompleted = hasFullyCompleted;
    LOGW_SYNCPAL_DEBUG(_logger, L"SyncPal init: " << Utility::formatSyncPath(syncDbPath));
    assert(_vfs);

    if (!createOrOpenDb(syncDbPath, version)) {
        throw std::runtime_error(SYNCPAL_NEW_ERROR_MSG);
    }
}

SyncPal::SyncPal(std::shared_ptr<Vfs> vfs, const int syncDbId_, const std::string &version) :
    _vfs(vfs),
    _logger(Log::instance()->getLogger()) {
    LOG_SYNCPAL_DEBUG(_logger, "SyncPal init");
    assert(_vfs);

    // Get sync
    Sync sync;
    bool found;
    if (!ParmsDb::instance()->selectSync(syncDbId_, sync, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in ParmsDb::selectSync");
        throw std::runtime_error(SYNCPAL_NEW_ERROR_MSG);
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "Sync not found");
        throw std::runtime_error(SYNCPAL_NEW_ERROR_MSG);
    }
    _syncInfo.syncDbId = syncDbId_;
    _syncInfo.driveDbId = sync.driveDbId();
    _syncInfo.localPath = sync.localPath();
    _syncInfo.localPath.make_preferred();
    _syncInfo.localNodeId = sync.localNodeId();
    _syncInfo.targetPath = sync.targetPath();
    _syncInfo.targetPath.make_preferred();

    // Get drive
    Drive drive;
    if (!ParmsDb::instance()->selectDrive(driveDbId(), drive, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in ParmsDb::selectDrive");
        throw std::runtime_error(SYNCPAL_NEW_ERROR_MSG);
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "Drive not found for driveDbId=" << driveDbId());
        throw std::runtime_error(SYNCPAL_NEW_ERROR_MSG);
    }
    _syncInfo.driveId = drive.driveId();
    _syncInfo.accountDbId = drive.accountDbId();
    _syncInfo.driveName = drive.name();

    // Get account
    Account account;
    if (!ParmsDb::instance()->selectAccount(accountDbId(), account, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in ParmsDb::selectAccount");
        throw std::runtime_error(SYNCPAL_NEW_ERROR_MSG);
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "Account not found for accountDbId=" << accountDbId());
        throw std::runtime_error(SYNCPAL_NEW_ERROR_MSG);
    }
    _syncInfo.userDbId = account.userDbId();

    // Get user
    User user;
    if (!ParmsDb::instance()->selectUser(userDbId(), user, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in ParmsDb::selectUser");
        throw std::runtime_error(SYNCPAL_NEW_ERROR_MSG);
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "User not found for userDbId=" << userDbId());
        throw std::runtime_error(SYNCPAL_NEW_ERROR_MSG);
    }
    _syncInfo.userId = user.userId();

    if (sync.dbPath().empty()) {
        // Set Db path
        bool alreadyExist;
        SyncPath dbPath = Db::makeDbName(user.userId(), account.accountId(), drive.driveId(), syncDbId(), alreadyExist);
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

    fixInconsistentFileNames();
    fixNodeTableDeleteItemsWithNullParentNodeId();
}

SyncPal::~SyncPal() {
    SyncNodeCache::instance()->clear(syncDbId());
    LOG_SYNCPAL_DEBUG(_logger, "~SyncPal");
}

void SyncPal::setVfs(std::shared_ptr<Vfs> vfs) {
    assert(!isRunning());
    _vfs = vfs;
}

bool SyncPal::isRunning() const {
    return (_syncPalWorker && _syncPalWorker->isRunning());
}

SyncStatus SyncPal::status() const {
    if (!_syncPalWorker) {
        // Not started
        return SyncStatus::Stopped;
    } else {
        // Has started
        if (_syncPalWorker->isRunning()) {
            // Still running
            LOG_IF_FAIL(_localFSObserverWorker)
            LOG_IF_FAIL(_remoteFSObserverWorker)

            if (_syncPalWorker->isPaused()) {
                // Auto paused after a NON fatal error
                return SyncStatus::Paused;
            } else if (_syncPalWorker->stopAsked()) {
                // Stopping at the request of the user
                return SyncStatus::StopAsked;
            } else if (!_localFSObserverWorker || !_remoteFSObserverWorker) {
                return SyncStatus::Error;
            } else if (_syncPalWorker->step() == SyncStep::Idle && !restart() && !liveSnapshot(ReplicaSide::Local).updated() &&
                       !liveSnapshot(ReplicaSide::Remote).updated()) {
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
                return SyncStatus::Stopped;
            } else {
                // Stopped after a fatal error (DB access, system...)
                return SyncStatus::Error;
            }
        }
    }
}

SyncStep SyncPal::step() const {
    return (_syncPalWorker ? _syncPalWorker->step() : SyncStep::None);
}

ExitCode SyncPal::fileStatus(ReplicaSide side, const SyncPath &path, SyncFileStatus &status) const {
    if (_tmpBlacklistManager && _tmpBlacklistManager->isTmpBlacklisted(path, side)) {
        if (path == Utility::sharedFolderName()) {
            status = SyncFileStatus::Success;
        } else {
            status = SyncFileStatus::Ignored;
        }
        return ExitCode::Ok;
    }

    bool found;
    if (!_syncDb->status(side, path, status, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::status");
        return ExitCode::DbError;
    }
    if (!found) {
        status = SyncFileStatus::Unknown;
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
        LOGW_SYNCPAL_WARN(_logger, L"Node not found in node table: " << Utility::formatSyncPath(path));
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
        LOGW_SYNCPAL_WARN(_logger, L"Node not found in node table: " << Utility::formatSyncPath(path));
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
        LOGW_SYNCPAL_WARN(_logger, L"Node not found in node table: " << Utility::formatSyncPath(path));
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
        if (jobPtr->exitInfo().code() != ExitCode::Ok) {
            LOG_SYNCPAL_WARN(_logger, "Error in PropagatorJob");
            addError(Error(syncDbId(), ERR_ID, jobPtr->exitInfo().code(), ExitCause::Unknown));
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
            _sendSignal(SignalNum::NODE_FIX_CONFLICTED_FILES_COMPLETED, syncDbId(), _conflictingFilesCorrector->nbErrors());
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

bool SyncPal::wipeVirtualFiles() {
    LOG_SYNCPAL_INFO(_logger, "Wiping virtual files");
    if (!vfs()) {
        addError(Error(syncDbId(), ERR_ID, ExitCode::LogicError, ExitCause::Unknown));
        return false;
    }
    VirtualFilesCleaner virtualFileCleaner(localPath(), _syncDb, vfs());
    if (!virtualFileCleaner.run()) {
        LOG_SYNCPAL_WARN(_logger, "Error in VirtualFilesCleaner::run");
        addError(Error(syncDbId(), ERR_ID, virtualFileCleaner.exitCode(), virtualFileCleaner.exitCause()));
        return false;
    }
    return true;
}

bool SyncPal::wipeOldPlaceholders() {
    LOG_SYNCPAL_INFO(_logger, "Wiping old placeholders files");
    VirtualFilesCleaner virtualFileCleaner(localPath());
    std::vector<SyncPath> failedToRemovePlaceholders;
    if (!virtualFileCleaner.removeDehydratedPlaceholders(failedToRemovePlaceholders)) {
        LOG_SYNCPAL_WARN(_logger, "Error in VirtualFilesCleaner::removeDehydratedPlaceholders");
        for (auto &failedItem: failedToRemovePlaceholders) {
            addError(Error(syncDbId(), "", "", NodeType::File, failedItem, ConflictType::None, InconsistencyType::None,
                           CancelType::None, "", virtualFileCleaner.exitCode(), virtualFileCleaner.exitCause()));
        }
        return false;
    }
    return true;
}

void SyncPal::loadProgress(SyncProgress &syncProgress) const {
    syncProgress._currentFile = _progressInfo->completedFiles();
    syncProgress._totalFiles = std::max(_progressInfo->completedFiles(), _progressInfo->totalFiles());
    syncProgress._completedSize = _progressInfo->completedSize();
    syncProgress._totalSize = std::max(_progressInfo->completedSize(), _progressInfo->totalSize());
    syncProgress._estimatedRemainingTime = _progressInfo->totalProgress().estimatedEta();
}

void SyncPal::createSharedObjects() {
    LOG_SYNCPAL_DEBUG(_logger, "Create shared objects");
    _localOperationSet = std::make_shared<FSOperationSet>(ReplicaSide::Local);
    _remoteOperationSet = std::make_shared<FSOperationSet>(ReplicaSide::Remote);
    _localUpdateTree = std::make_shared<UpdateTree>(ReplicaSide::Local, _syncDb->rootNode());
    _remoteUpdateTree = std::make_shared<UpdateTree>(ReplicaSide::Remote, _syncDb->rootNode());
    _conflictQueue = std::make_shared<ConflictQueue>(_localUpdateTree, _remoteUpdateTree);
    _syncOps = std::make_shared<SyncOperationList>();
    _progressInfo = std::make_shared<ProgressInfo>(shared_from_this());

    initSharedObjects();
}

void SyncPal::freeSharedObjects() {
    LOG_SYNCPAL_DEBUG(_logger, "Free shared objects");
    _localSnapshot.reset();
    _remoteSnapshot.reset();
    _localOperationSet.reset();
    _remoteOperationSet.reset();
    _localUpdateTree.reset();
    _remoteUpdateTree.reset();
    _conflictQueue.reset();
    _syncOps.reset();
    _progressInfo.reset();

    // Check that there is no memory leak
    LOG_IF_FAIL(_localSnapshot.use_count() == 0);
    LOG_IF_FAIL(_remoteSnapshot.use_count() == 0);
    LOG_IF_FAIL(_localOperationSet.use_count() == 0);
    LOG_IF_FAIL(_remoteOperationSet.use_count() == 0);
    LOG_IF_FAIL(_localUpdateTree.use_count() == 0);
    LOG_IF_FAIL(_remoteUpdateTree.use_count() == 0);
    LOG_IF_FAIL(_conflictQueue.use_count() == 0);
    LOG_IF_FAIL(_syncOps.use_count() == 0);
    LOG_IF_FAIL(_progressInfo.use_count() == 0);
}

void SyncPal::initSharedObjects() {
    LOG_SYNCPAL_DEBUG(_logger, "Init shared objects");
    if (_localUpdateTree) _localUpdateTree->init();
    if (_remoteUpdateTree) _remoteUpdateTree->init();

    setSyncHasFullyCompleted(false);
}

void SyncPal::resetSharedObjects() {
    LOG_SYNCPAL_DEBUG(_logger, "Reset shared objects");

    if (_localOperationSet) _localOperationSet->clear();
    if (_remoteOperationSet) _remoteOperationSet->clear();
    if (_localUpdateTree) _localUpdateTree->clear();
    if (_remoteUpdateTree) _remoteUpdateTree->clear();
    if (_conflictQueue) _conflictQueue->clear();
    if (_syncOps) _syncOps->clear();

    initSharedObjects();

    LOG_SYNCPAL_DEBUG(_logger, "Reset shared objects done");
}

void SyncPal::createWorkers(const std::chrono::seconds &startDelay) {
    LOG_SYNCPAL_DEBUG(_logger, "Create workers");
#if defined(KD_WINDOWS)
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
    _syncPalWorker = std::shared_ptr<SyncPalWorker>(new SyncPalWorker(shared_from_this(), "Main", "MAIN", startDelay));

    _tmpBlacklistManager = std::shared_ptr<TmpBlacklistManager>(new TmpBlacklistManager(shared_from_this()));
}

void SyncPal::freeWorkers() {
    LOG_SYNCPAL_DEBUG(_logger, "Free workers");
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
}

ExitCode SyncPal::setSyncPaused(bool value) {
    bool found = false;
    if (!ParmsDb::instance()->setSyncPaused(syncDbId(), value, found)) {
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
    } catch (std::exception const &e) {
        const auto exceptionMsg = CommonUtility::s2ws(std::string(e.what()));
        LOGW_SYNCPAL_WARN(
                _logger, L"Error in SyncDb::SyncDb: " << Utility::formatSyncPath(syncDbPath) << L", Exception: " << exceptionMsg);
        return false;
    }

    if (!_syncDb->init(version)) {
        LOGW_SYNCPAL_WARN(_logger, L"Error in SyncDb::init: " << Utility::formatSyncPath(syncDbPath));
        _syncDb.reset();
        return false;
    }

    // Init SyncNode table cache
    SyncNodeCache::instance()->initCache(syncDbId(), _syncDb);

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

bool SyncPal::initProgress(const SyncFileItem &item) {
    return _progressInfo->initProgress(item);
}

bool SyncPal::setProgress(const SyncPath &relativePath, int64_t current) {
    if (!_progressInfo->setProgress(relativePath, current)) {
        LOG_SYNCPAL_WARN(_logger, "Error in ProgressInfo::setProgress");
        return false;
    }

    bool found = false;
    if (!_syncDb->setStatus(ReplicaSide::Remote, relativePath, SyncFileStatus::Syncing, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::setStatus");
        return false;
    }
    if (!found) {
        SyncFileItem item;
        if (!getSyncFileItem(relativePath, item)) {
            LOG_SYNCPAL_WARN(_logger, "Error in SyncPal::getSyncFileItem");
            return false;
        }
        if (item.instruction() != SyncFileInstruction::Get && item.instruction() != SyncFileInstruction::Put) {
            LOGW_SYNCPAL_WARN(_logger, L"Node not found: " << Utility::formatSyncPath(relativePath));
            return false;
        }
    }
    return true;
}

bool SyncPal::setProgressComplete(const SyncPath &relativeLocalPath, SyncFileStatus status, const NodeId &newRemoteNodeId) {
    if (!newRemoteNodeId.empty()) {
        if (!_progressInfo->setSyncFileItemRemoteId(relativeLocalPath, newRemoteNodeId)) {
            LOG_SYNCPAL_WARN(_logger, "Error in ProgressInfo::setSyncFileItemRemoteId");
            // Continue anyway as this is not critical, the share menu on activities will not be available for this file
        }
    }

    if (!_progressInfo->setProgressComplete(relativeLocalPath, status)) {
        LOG_SYNCPAL_WARN(_logger, "Error in ProgressInfo::setProgressComplete");
        return false;
    }

    vfs()->fileStatusChanged(localPath() / relativeLocalPath, status);

    bool found = false;
    if (!_syncDb->setStatus(ReplicaSide::Local, relativeLocalPath, status, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::setStatus");
        return false;
    }
    if (!found) {
        // Can happen for a dehydrated placeholder
        LOGW_SYNCPAL_DEBUG(_logger, L"Node not found: " << Utility::formatSyncPath(relativeLocalPath));
    }
    return true;
}

void SyncPal::directDownloadCallback(UniqueId jobId) {
    const std::lock_guard lock(_directDownloadJobsMapMutex);
    auto directDownloadJobsMapIt = _directDownloadJobsMap.find(jobId);
    if (directDownloadJobsMapIt == _directDownloadJobsMap.end()) {
        // No need to send a warning, the job might have been canceled, and therefor not in the map anymore
        return;
    }

    const auto downloadJob = directDownloadJobsMapIt->second;
    if (downloadJob->getStatusCode() == Poco::Net::HTTPResponse::HTTP_NOT_FOUND) {
        Error error;
        error.setLevel(ErrorLevel::Node);
        error.setSyncDbId(syncDbId());
        error.setRemoteNodeId(downloadJob->remoteNodeId());
        error.setPath(downloadJob->localPath());
        error.setExitCode(ExitCode::BackError);
        error.setExitCause(ExitCause::NotFound);
        addError(error);

        vfs()->cancelHydrate(downloadJob->localPath());
    }

    (void) _syncPathToDownloadJobMap.erase(downloadJob->affectedFilePath());
    (void) _directDownloadJobsMap.erase(directDownloadJobsMapIt);
    for (auto it = _folderHydrationInProgress.begin(); it != _folderHydrationInProgress.end();) {
        const auto &parentFolderPath = it->first;
        if (it->second.erase(downloadJob->affectedFilePath())) {
            LOGW_INFO(_logger, L"Download of item " << Utility::formatSyncPath(downloadJob->affectedFilePath())
                                                    << L" from parent folder " << Utility::formatSyncPath(parentFolderPath)
                                                    << L" terminated.");
        }
        if (it->second.empty()) {
            // Notify the LiteSync extension that the hydration of the folder is terminated.
            if (const auto exitInfo = _vfs->updateFetchStatus(parentFolderPath, "OK"); !exitInfo) {
                LOGW_WARN(_logger,
                          L"Error in vfsUpdateFetchStatus: " << Utility::formatSyncPath(parentFolderPath) << L" : " << exitInfo);
            } else {
                LOGW_INFO(_logger, L"Hydration of folder: " << Utility::formatSyncPath(parentFolderPath) << L" terminated.");
            }
            it = _folderHydrationInProgress.erase(it);
            continue;
        }
        it++;
    }
}

bool SyncPal::getSyncFileItem(const SyncPath &path, SyncFileItem &item) {
    return _progressInfo->getSyncFileItem(path, item);
}

void SyncPal::resetSnapshotInvalidationCounters() {
    _localFSObserverWorker->resetInvalidateCounter();
    _remoteFSObserverWorker->resetInvalidateCounter();
}

ExitCode SyncPal::addDlDirectJob(const SyncPath &relativePath, const SyncPath &absoluteLocalPath,
                                 const SyncPath &parentFolderPath) {
    std::optional<NodeId> localNodeId = std::nullopt;
    bool found = false;
    if (!_syncDb->id(ReplicaSide::Local, relativePath, localNodeId, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::id");
        return ExitCode::DbError;
    }
    if (!found) {
        LOGW_SYNCPAL_WARN(_logger, L"Node not found in node table: " << Utility::formatSyncPath(relativePath));
        return ExitCode::DataError;
    }

    NodeId remoteNodeId;
    if (!_syncDb->correspondingNodeId(ReplicaSide::Local, *localNodeId, remoteNodeId, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::correspondingNodeId");
        return ExitCode::DbError;
    }
    if (!found) {
        LOGW_SYNCPAL_WARN(_logger, L"Node not found in node table for localNodeId=" << CommonUtility::s2ws(*localNodeId));
        return ExitCode::DataError;
    }

    int64_t expectedSize = -1;
    if (!_syncDb->size(ReplicaSide::Local, *localNodeId, expectedSize, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::size");
        return ExitCode::DbError;
    }
    if (!found) {
        LOGW_SYNCPAL_WARN(_logger, L"Node not found in node table for localNodeId=" << CommonUtility::s2ws(*localNodeId));
        return ExitCode::DataError;
    }

    // Hydration job
    std::shared_ptr<DownloadJob> job = nullptr;
    try {
        job = std::make_shared<DownloadJob>(vfs(), driveDbId(), remoteNodeId, absoluteLocalPath, expectedSize);
        if (!job) {
            LOG_SYNCPAL_WARN(_logger, "Memory allocation error");
            return ExitCode::SystemError;
        }
        job->setAffectedFilePath(absoluteLocalPath);
    } catch (const std::exception &e) {
        LOG_SYNCPAL_WARN(Log::instance()->getLogger(), "Error in DownloadJob::DownloadJob: error=" << e.what());
        addError(Error(syncDbId(), ERR_ID, ExitCode::Unknown, ExitCause::Unknown));
        return AbstractTokenNetworkJob::exception2ExitCode(e);
    }

    // Queue job
    std::function<void(UniqueId)> callback = std::bind_front(&SyncPal::directDownloadCallback, this);
    job->setAdditionalCallback(callback);
    JobManager::instance()->queueAsyncJob(job, Poco::Thread::PRIO_HIGH);

    const std::lock_guard lock(_directDownloadJobsMapMutex);
    (void) _directDownloadJobsMap.try_emplace(job->jobId(), job);
    (void) _syncPathToDownloadJobMap.try_emplace(absoluteLocalPath, job->jobId());
    if (!parentFolderPath.empty() && _folderHydrationInProgress.contains(parentFolderPath)) {
        (void) _folderHydrationInProgress[parentFolderPath].emplace(absoluteLocalPath);
    }

    return ExitCode::Ok;
}

void SyncPal::monitorFolderHydration(const SyncPath &absoluteLocalPath) {
    const std::lock_guard lock(_directDownloadJobsMapMutex);
    (void) _folderHydrationInProgress.try_emplace(absoluteLocalPath);
    LOGW_INFO(_logger, L"Monitoring folder hydration: " << Utility::formatSyncPath(absoluteLocalPath));
}

ExitCode SyncPal::cancelDlDirectJobs(const std::vector<SyncPath> &fileList) {
    for (const auto &filePath: fileList) {
        const std::lock_guard lock(_directDownloadJobsMapMutex);

        if (const auto itId = _syncPathToDownloadJobMap.find(filePath); itId != _syncPathToDownloadJobMap.end()) {
            if (const auto itJob = _directDownloadJobsMap.find(itId->second); itJob != _directDownloadJobsMap.end()) {
                itJob->second->abort();
                (void) _directDownloadJobsMap.erase(itJob);
            }
            (void) _syncPathToDownloadJobMap.erase(itId);
        }
        if (_folderHydrationInProgress.contains(filePath)) {
            _vfs->cancelHydrate(filePath);
            (void) _folderHydrationInProgress.erase(filePath);
        }
    }

    return ExitCode::Ok;
}

ExitCode SyncPal::cancelAllDlDirectJobs(bool quit) {
    LOG_SYNCPAL_INFO(_logger, "Cancelling all direct download jobs");

    const std::lock_guard<std::mutex> lock(_directDownloadJobsMapMutex);
    for (auto &directDownloadJobsMapElt: _directDownloadJobsMap) {
        LOG_SYNCPAL_DEBUG(_logger, "Cancelling download job " << directDownloadJobsMapElt.first);
        if (quit) {
            directDownloadJobsMapElt.second->setAdditionalCallback(nullptr);
        }
        directDownloadJobsMapElt.second->abort();
    }

    _directDownloadJobsMap.clear();
    _syncPathToDownloadJobMap.clear();
    for (const auto &[parentFolderPath, _]: _folderHydrationInProgress) {
        _vfs->cancelHydrate(parentFolderPath);
    }
    _folderHydrationInProgress.clear();

    LOG_SYNCPAL_INFO(_logger, "Cancelling all direct download jobs done");

    return ExitCode::Ok;
}

void SyncPal::setSyncHasFullyCompletedInParms(bool syncHasFullyCompleted) {
    setSyncHasFullyCompleted(syncHasFullyCompleted);

    bool found = false;
    if (!ParmsDb::instance()->setSyncHasFullyCompleted(syncDbId(), syncHasFullyCompleted, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in ParmsDb::setSyncHasFullyCompleted");
        return;
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "Sync not found");
    }
}

ExitInfo SyncPal::isRootFolderValid() {
    if (NodeId rootNodeId; IoHelper::getNodeId(localPath(), rootNodeId)) {
        if (rootNodeId.empty()) {
            LOGW_SYNCPAL_WARN(_logger, L"Unable to get root folder nodeId: " << Utility::formatSyncPath(localPath()));
            return ExitCode::SystemError;
        }

        if (localNodeId().empty()) {
            if (ExitInfo exitInfo = setLocalNodeId(rootNodeId); !exitInfo) {
                LOGW_SYNCPAL_WARN(_logger, L"Error in SyncPal::setLocalNodeId: " << exitInfo);
                return exitInfo;
            }
            return ExitCode::Ok;
        }

        return localNodeId() == rootNodeId ? ExitInfo(ExitCode::Ok) : ExitInfo(ExitCode::DataError, ExitCause::SyncDirChanged);
    } else {
        LOGW_SYNCPAL_WARN(_logger, L"Error in IoHelper::getNodeId for root folder: " << Utility::formatSyncPath(localPath()));
        return ExitCode::SystemError;
    }
}

ExitInfo SyncPal::setLocalNodeId(const NodeId &localNodeId) {
    _syncInfo.localNodeId = localNodeId;
    Sync sync;
    bool found = false;
    if (!ParmsDb::instance()->selectSync(syncDbId(), sync, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in ParmsDb::selectSync");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "Sync not found");
        return ExitCode::DataError;
    }

    sync.setLocalNodeId(localNodeId);
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

ExitInfo SyncPal::setListingCursor(const std::string &value, int64_t timestamp) {
    Sync sync;
    bool found;
    if (!ParmsDb::instance()->selectSync(syncDbId(), sync, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in ParmsDb::selectSync");
        return {ExitCode::DbError, ExitCause::DbAccessError};
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "Sync not found");
        return {ExitCode::DataError, ExitCause::DbEntryNotFound};
    }

    sync.setListingCursor(value, timestamp);
    if (!ParmsDb::instance()->updateSync(sync, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in ParmsDb::updateSync");
        return {ExitCode::DbError, ExitCause::DbAccessError};
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "Sync not found");
        return {ExitCode::DataError, ExitCause::DbEntryNotFound};
    }

    return ExitCode::Ok;
}

ExitInfo SyncPal::listingCursor(std::string &value, int64_t &timestamp) {
    Sync sync;
    bool found;
    if (!ParmsDb::instance()->selectSync(syncDbId(), sync, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in ParmsDb::selectSync");
        return {ExitCode::DbError, ExitCause::DbAccessError};
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "Sync not found");
        return {ExitCode::DataError, ExitCause::DbEntryNotFound};
    }

    sync.listingCursor(value, timestamp);
    return ExitCode::Ok;
}

std::shared_ptr<ConstSnapshot> SyncPal::snapshot(const ReplicaSide side) const {
    LOG_IF_FAIL(side != ReplicaSide::Unknown);
    if (side == ReplicaSide::Unknown) {
        LOG_ERROR(_logger, "Call to SyncPal::snapshot with 'ReplicaSide::Unknown').");
        return nullptr;
    }
    LOG_IF_FAIL(_localSnapshot);
    LOG_IF_FAIL(_remoteSnapshot);
    return (side == ReplicaSide::Local ? _localSnapshot : _remoteSnapshot);
}

const LiveSnapshot &SyncPal::liveSnapshot(ReplicaSide side) const {
    LOG_IF_FAIL(side != ReplicaSide::Unknown);
    LOG_IF_FAIL(_localFSObserverWorker);
    LOG_IF_FAIL(_remoteFSObserverWorker);
    return (side == ReplicaSide::Local ? _localFSObserverWorker->liveSnapshot() : _remoteFSObserverWorker->liveSnapshot());
}

std::shared_ptr<FSOperationSet> SyncPal::operationSet(ReplicaSide side) const {
    if (side == ReplicaSide::Unknown) {
        LOG_ERROR(_logger, "Call to SyncPal::operationSet with 'ReplicaSide::Unknown').");
        return nullptr;
    }
    return (side == ReplicaSide::Local ? _localOperationSet : _remoteOperationSet);
}

std::shared_ptr<UpdateTree> SyncPal::updateTree(ReplicaSide side) const {
    if (side == ReplicaSide::Unknown) {
        LOG_ERROR(_logger, "Call to SyncPal::updateTree with 'ReplicaSide::Unknown').");
        return nullptr;
    }
    return (side == ReplicaSide::Local ? _localUpdateTree : _remoteUpdateTree);
}

void SyncPal::createProgressInfo() {
    _progressInfo = std::shared_ptr<ProgressInfo>(new ProgressInfo(shared_from_this()));
}

ExitCode SyncPal::fileRemoteIdFromLocalPath(const SyncPath &path, NodeId &nodeId) const {
    DbNodeId dbNodeId = -1;
    bool found = false;
    if (!_syncDb->dbId(ReplicaSide::Local, path, dbNodeId, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbId");
        return ExitCode::DbError;
    }
    if (!found) {
        LOGW_SYNCPAL_WARN(_logger, L"Node not found in node table: " << Utility::formatSyncPath(path));
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

bool SyncPal::checkIfExistsOnServer(const SyncPath &path, bool &exists) const {
    exists = false;

    if (!_remoteFSObserverWorker) return false;

    // Path is normalized on server side
    SyncPath normalizedPath;
    if (!Utility::normalizedSyncPath(path, normalizedPath)) {
        LOGW_SYNCPAL_WARN(_logger, L"Error in Utility::normalizedSyncPath: " << Utility::formatSyncPath(path));
        return false;
    }
    const NodeId nodeId = liveSnapshot(ReplicaSide::Remote).itemId(normalizedPath);
    exists = !nodeId.empty();
    return true;
}

bool SyncPal::checkIfCanShareItem(const SyncPath &path, bool &canShare) const {
    canShare = false;

    if (!_remoteFSObserverWorker) return false;

    // Path is normalized on server side
    SyncPath normalizedPath;
    if (!Utility::normalizedSyncPath(path, normalizedPath)) {
        LOGW_SYNCPAL_WARN(_logger, L"Error in Utility::normalizedSyncPath: " << Utility::formatSyncPath(path));
        return false;
    }

    if (const NodeId nodeId = liveSnapshot(ReplicaSide::Remote).itemId(normalizedPath); !nodeId.empty()) {
        canShare = liveSnapshot(ReplicaSide::Remote).canShare(nodeId);
    }

    return true;
}

ExitCode SyncPal::syncIdSet(SyncNodeType type, NodeSet &nodeIdSet) {
    ExitCode exitCode = SyncNodeCache::instance()->syncNodes(syncDbId(), type, nodeIdSet);
    if (exitCode != ExitCode::Ok) {
        LOG_SYNCPAL_WARN(Log::instance()->getLogger(), "Error in SyncNodeCache::syncNodes");
        return exitCode;
    }

    return ExitCode::Ok;
}

ExitCode SyncPal::setSyncIdSet(SyncNodeType type, const NodeSet &nodeIdSet) {
    ExitCode exitCode = SyncNodeCache::instance()->update(syncDbId(), type, nodeIdSet);
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
    _blacklistPropagator->setAdditionalCallback(callback);
    JobManager::instance()->queueAsyncJob(_blacklistPropagator, Poco::Thread::PRIO_HIGHEST);

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
    _excludeListPropagator->setAdditionalCallback(callback);
    JobManager::instance()->queueAsyncJob(_excludeListPropagator, Poco::Thread::PRIO_HIGHEST);

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
    _conflictingFilesCorrector->setAdditionalCallback(callback);
    JobManager::instance()->queueAsyncJob(_conflictingFilesCorrector, Poco::Thread::PRIO_HIGHEST);

    return ExitCode::Ok;
}

ExitCode SyncPal::fixCorruptedFile(const std::unordered_map<NodeId, SyncPath> &localFileMap) {
    for (const auto &localFileInfo: localFileMap) {
        SyncPath destPath;
        if (ExitCode exitCode = PlatformInconsistencyCheckerUtility::renameLocalFile(
                    localFileInfo.second, PlatformInconsistencyCheckerUtility::SuffixType::Conflict, &destPath);
            exitCode != ExitCode::Ok) {
            LOGW_SYNCPAL_WARN(_logger, L"Fail to rename " << Utility::formatSyncPath(localFileInfo.second) << L" into "
                                                          << Utility::formatSyncPath(destPath));

            return exitCode;
        }

        DbNodeId dbId = -1;
        bool found = false;
        if (!_syncDb->dbId(ReplicaSide::Local, localFileInfo.first, dbId, found)) {
            LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::dbId for nodeId=" << localFileInfo.first);
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

void SyncPal::start(const std::chrono::seconds &startDelay) {
    LOG_SYNCPAL_DEBUG(_logger, "SyncPal start");

    // Load VFS mode
    Sync sync;
    bool found;
    if (!ParmsDb::instance()->selectSync(syncDbId(), sync, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in ParmsDb::selectSync");
        addError(Error(syncDbId(), ERR_ID, ExitCode::DbError, ExitCause::Unknown));
        return;
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "Sync not found in sync table for syncDbId=" << syncDbId());
        addError(Error(syncDbId(), ERR_ID, ExitCode::DataError, ExitCause::Unknown));
        return;
    }
    setVfsMode(sync.virtualFileMode());

    // Clear tmp blacklist
    SyncNodeCache::instance()->update(syncDbId(), SyncNodeType::TmpRemoteBlacklist, NodeSet());
    SyncNodeCache::instance()->update(syncDbId(), SyncNodeType::TmpLocalBlacklist, NodeSet());

    // Create and init shared objects
    createSharedObjects();

    // Create workers
    createWorkers(startDelay);

    // Reset paused flag
    ExitCode exitCode = setSyncPaused(false);
    if (exitCode != ExitCode::Ok) {
        LOG_SYNCPAL_DEBUG(_logger, "Error in SyncPal::setSyncPaused");
        addError(Error(syncDbId(), ERR_ID, exitCode, ExitCause::Unknown));
        return;
    }

    exitCode = cleanOldUploadSessionTokens();
    if (exitCode != ExitCode::Ok) {
        LOG_SYNCPAL_DEBUG(_logger, "Error in SyncPal::cleanOldUploadSessionTokens");
        addError(Error(syncDbId(), ERR_ID, exitCode, ExitCause::Unknown));
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

        setIsPaused(true);
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
            addError(Error(syncDbId(), ERR_ID, exitCode, ExitCause::Unknown));
        }

        // Unpause main worker
        _syncPalWorker->unpause();

        setIsPaused(false);
    }
}

std::chrono::time_point<std::chrono::steady_clock> SyncPal::pauseTime() const {
    if (_syncPalWorker) {
        return _syncPalWorker->pauseTime();
    }
    return std::chrono::steady_clock::now();
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
            addError(Error(syncDbId(), ERR_ID, exitCode, ExitCause::Unknown));
        }
    }

    // Free workers
    freeWorkers();

    // Free shared objects
    freeSharedObjects();

    _syncDb->setAutoDelete(clear);
}

bool SyncPal::isPaused() const {
    return _syncPalWorker && _syncPalWorker->isPaused();
}

bool SyncPal::pauseAsked() const {
    return _syncPalWorker && _syncPalWorker->pauseAsked();
}

bool SyncPal::isIdle() const {
    if (_syncPalWorker) {
        return (_syncPalWorker->step() == SyncStep::Idle && !restart());
    }
    return false;
}

ExitCode SyncPal::cleanOldUploadSessionTokens() {
    std::vector<UploadSessionToken> uploadSessionTokenList;
    if (!_syncDb->selectAllUploadSessionTokens(uploadSessionTokenList)) {
        LOG_WARN(_logger, "Error in SyncDb::selectAllUploadSessionTokens");
        return ExitCode::DbError;
    }

    for (auto &uploadSessionToken: uploadSessionTokenList) {
        try {
            auto job = std::make_shared<UploadSessionCancelJob>(UploadSessionType::Drive, driveDbId(), "",
                                                                uploadSessionToken.token());
            ExitCode exitCode = job->runSynchronously();
            if (exitCode != ExitCode::Ok) {
                LOG_SYNCPAL_WARN(_logger, "Error in UploadSessionCancelJob::runSynchronously: code=" << exitCode);
                if (exitCode == ExitCode::NetworkError) {
                    return exitCode;
                }
            }

            if (job->hasHttpError()) {
                LOG_SYNCPAL_WARN(_logger, "Upload Session Token: " << uploadSessionToken.token()
                                                                   << " has already been canceled or has expired.");
            }
        } catch (const std::exception &e) {
            LOG_WARN(_logger, "Error in UploadSessionCancelJob: error=" << e.what());
            return AbstractTokenNetworkJob::exception2ExitCode(e);
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

    for (const auto &[path, _]: _syncPathToDownloadJobMap) {
        if (CommonUtility::isSubDir(localPath, path)) {
            return true;
        }
    }

    return false;
}

void SyncPal::fixInconsistentFileNames() {
    if (_syncDb->fromVersion().empty()) {
        // New DB
        return;
    }

    std::string dbFromVersionNumber = CommonUtility::dbVersionNumber(_syncDb->fromVersion());
    if (CommonUtility::isVersionLower(dbFromVersionNumber, "3.4.9")) {
        LOG_DEBUG(KDC::Log::instance()->getLogger(), "Fix inconsistent file names");

        std::vector<DbNode> dbNodeList;
        if (!_syncDb->selectAllRenamedNodes(dbNodeList, false)) {
            LOG_WARN(KDC::Log::instance()->getLogger(), "Error in SyncDb::selectAllRenamedNodes");
            return;
        }

        LOG_DEBUG(KDC::Log::instance()->getLogger(), "Delete " << dbNodeList.size() << " files");
        for (DbNode &dbNode: dbNodeList) {
            SyncPath oldLocalPath;
            SyncPath remotePath;
            bool found;
            if (!_syncDb->path(dbNode.nodeId(), oldLocalPath, remotePath, found)) {
                LOG_WARN(KDC::Log::instance()->getLogger(), "Error in SyncDb::path");
                continue;
            }
            if (!found) {
                LOG_WARN(KDC::Log::instance()->getLogger(), "Node not found for id=" << dbNode.nodeId());
                continue;
            }

            found = false;
            if (!_syncDb->deleteNode(dbNode.nodeId(), found)) {
                LOG_WARN(KDC::Log::instance()->getLogger(), "Node not found for id=" << dbNode.nodeId());
                continue;
            }
            if (!found) {
                LOG_WARN(KDC::Log::instance()->getLogger(), "Node not found for id=" << dbNode.nodeId());
                continue;
            }

            LocalDeleteJob deleteJob(syncInfo(), oldLocalPath, false, *dbNode.nodeIdRemote(), true);
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

void SyncPal::increaseErrorCount(const NodeId &nodeId, const NodeType type, const SyncPath &relativePath, const ReplicaSide side,
                                 const ExitInfo exitInfo /*= ExitInfo()*/) {
    _tmpBlacklistManager->increaseErrorCount(nodeId, type, relativePath, side, exitInfo);
}

void SyncPal::blacklistTemporarily(const NodeId &nodeId, const SyncPath &relativePath, const ReplicaSide side) {
    _tmpBlacklistManager->blacklistItem(nodeId, relativePath, side);
}

bool SyncPal::isTmpBlacklisted(const SyncPath &relativePath, const ReplicaSide side) const {
    return _tmpBlacklistManager->isTmpBlacklisted(relativePath, side);
}

void SyncPal::refreshTmpBlacklist() {
    _tmpBlacklistManager->refreshBlacklist();
}

void SyncPal::removeItemFromTmpBlacklist(const NodeId &nodeId, const ReplicaSide side) {
    _tmpBlacklistManager->removeItemFromTmpBlacklist(nodeId, side);
}

void SyncPal::removeItemFromTmpBlacklist(const SyncPath &relativePath) {
    _tmpBlacklistManager->removeItemFromTmpBlacklist(relativePath);
}

ExitInfo SyncPal::handleAccessDeniedItem(const SyncPath &relativeLocalPath, ExitCause cause) {
    std::shared_ptr<Node> dummyNodePtr;
    return handleAccessDeniedItem(relativeLocalPath, dummyNodePtr, dummyNodePtr, cause);
}

ExitInfo SyncPal::handleAccessDeniedItem(const SyncPath &relativeLocalPath, std::shared_ptr<Node> &localBlacklistedNode,
                                         std::shared_ptr<Node> &remoteBlacklistedNode, ExitCause cause) {
    if (relativeLocalPath.empty()) {
        LOG_SYNCPAL_WARN(_logger, "Access error on root folder");
        return ExitInfo(ExitCode::SystemError, ExitCause::SyncDirAccessError);
    }
    Error error(syncDbId(), "", "", relativeLocalPath.extension() == SyncPath() ? NodeType::Directory : NodeType::File,
                relativeLocalPath, ConflictType::None, InconsistencyType::None, CancelType::None, "", ExitCode::SystemError,
                cause);
    addError(error);

    LOG_IF_FAIL(_localFSObserverWorker)
    LOG_IF_FAIL(_remoteFSObserverWorker)
    if (!_localFSObserverWorker || !_remoteFSObserverWorker) return ExitCode::LogicError;

    NodeId localNodeId = liveSnapshot(ReplicaSide::Local).itemId(relativeLocalPath);
    if (localNodeId.empty()) {
        SyncPath absolutePath = localPath() / relativeLocalPath;
        if (!IoHelper::getNodeId(absolutePath, localNodeId)) {
            bool exists = false;
            IoError ioError = IoError::Success;
            if (!IoHelper::checkIfPathExists(absolutePath, exists, ioError)) {
                LOGW_WARN(_logger, L"IoHelper::checkIfPathExists failed with: " << Utility::formatIoError(absolutePath, ioError));
                return ExitCode::SystemError;
            }
            if (ioError == IoError::AccessDenied) { // A parent of the file does not have sufficient right
                LOGW_DEBUG(_logger, L"A parent of " << Utility::formatSyncPath(relativeLocalPath)
                                                    << L"does not have sufficient right, blacklisting the parent item.");
                return handleAccessDeniedItem(relativeLocalPath.parent_path(), localBlacklistedNode, remoteBlacklistedNode,
                                              cause);
            }
        }
    }

    LOGW_SYNCPAL_DEBUG(_logger, L"Item " << Utility::formatSyncPath(relativeLocalPath) << L" (NodeId: "
                                         << CommonUtility::s2ws(localNodeId)
                                         << L" is blacklisted temporarily because of a denied access.");

    NodeId remoteNodeId = liveSnapshot(ReplicaSide::Remote).itemId(relativeLocalPath);
    if (bool found; remoteNodeId.empty() && !localNodeId.empty() &&
                    !_syncDb->correspondingNodeId(ReplicaSide::Local, localNodeId, remoteNodeId, found)) {
        LOG_SYNCPAL_WARN(_logger, "Error in SyncDb::correspondingNodeId");
        return {ExitCode::DbError, ExitCause::Unknown};
    }

    // Blacklist the item
    if (!localNodeId.empty()) {
        _tmpBlacklistManager->blacklistItem(localNodeId, relativeLocalPath, ReplicaSide::Local);
        if (!updateTree(ReplicaSide::Local)->deleteNode(localNodeId)) {
            // Do nothing: Can happen if the UpdateTreeWorker step has never been launched
        }
    }

    if (!remoteNodeId.empty()) {
        _tmpBlacklistManager->blacklistItem(remoteNodeId, relativeLocalPath, ReplicaSide::Remote);
        if (!updateTree(ReplicaSide::Remote)->deleteNode(remoteNodeId)) {
            // Do nothing: Can happen if the UpdateTreeWorker step has never been launched
        }
    }

    return ExitCode::Ok;
}

void SyncPal::copySnapshots() {
    LOG_IF_FAIL(_localFSObserverWorker)
    LOG_IF_FAIL(_remoteFSObserverWorker)

    _localSnapshot = std::make_shared<ConstSnapshot>(liveSnapshot(ReplicaSide::Local));
    _remoteSnapshot = std::make_shared<ConstSnapshot>(liveSnapshot(ReplicaSide::Remote));
    liveSnapshot(ReplicaSide::Local).startRead();
    liveSnapshot(ReplicaSide::Remote).startRead();
}

void SyncPal::freeSnapshotsCopies() {
    assert(_localSnapshot.use_count() == 1);
    _localSnapshot.reset();

    assert(_remoteSnapshot.use_count() == 1);
    _remoteSnapshot.reset();
}

void SyncPal::tryToInvalidateSnapshots() {
    _localFSObserverWorker->tryToInvalidateSnapshot();
    _remoteFSObserverWorker->forceUpdate();
    _remoteFSObserverWorker->tryToInvalidateSnapshot();
}

void SyncPal::forceInvalidateSnapshots() {
    _localFSObserverWorker->invalidateSnapshot();
    _remoteFSObserverWorker->forceUpdate();
    _remoteFSObserverWorker->invalidateSnapshot();
}

} // namespace KDC
