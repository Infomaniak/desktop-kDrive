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

#include "driveuploadsession.h"

#include "io/filestat.h"
#include "utility/utility.h"

namespace KDC {

DriveUploadSession::DriveUploadSession(const std::shared_ptr<Vfs> vfs, const DriveDbId driveDbId,
                                       const std::shared_ptr<SyncDb> syncDb, const SyncPath &filepath, const SyncName &filename,
                                       const NodeId &remoteParentDirId, const SyncTime creationTime,
                                       const SyncTime modificationTime, const bool liteSyncActivated,
                                       const uint64_t nbParallelThread) :
    AbstractUploadSession(filepath, filename, nbParallelThread),
    _driveDbId(driveDbId),
    _syncDb(syncDb),
    _creationTimeIn(creationTime),
    _modificationTimeIn(modificationTime),
    _remoteParentDirId(remoteParentDirId),
    _vfs(vfs) {
    (void) liteSyncActivated;
    _uploadSessionType = UploadSessionType::Drive;
}

DriveUploadSession::DriveUploadSession(const std::shared_ptr<Vfs> vfs, const DriveDbId driveDbId,
                                       const std::shared_ptr<SyncDb> syncDb, const SyncPath &filepath, const NodeId &fileId,
                                       const SyncTime modificationTime, const bool liteSyncActivated,
                                       const uint64_t nbParallelThread) :
    DriveUploadSession(vfs, driveDbId, syncDb, filepath, SyncName(), fileId, 0, modificationTime, liteSyncActivated,
                       nbParallelThread) {
    _fileId = fileId;

    // Retrieve creation date from the local file
    FileStat fileStat;
    auto ioError = IoError::Unknown;
    if (!IoHelper::getFileStat(filepath, &fileStat, ioError, IoHelper::PathCheckOption::Insensitive) ||
        ioError != IoError::Success) {
        LOGW_WARN(getLogger(), L"Failed to get FileStat for " << Utility::formatSyncPath(filepath) << L": " << ioError);
    }
    _creationTimeIn = fileStat.creationTime;
}

DriveUploadSession::~DriveUploadSession() {
    if (!_vfs || isAborted()) return;
    constexpr VfsStatus vfsStatus({.isHydrated = true, .isSyncing = false, .progress = 100});
    if (const auto exitInfo = _vfs->forceStatus(getFilePath(), vfsStatus); !exitInfo) {
        LOGW_WARN(getLogger(), L"Error in vfsForceStatus: " << Utility::formatSyncPath(getFilePath()) << L": " << exitInfo);
    }
}

ExitInfo DriveUploadSession::checkHashMatch() {
    _shouldUpload = true;
    FileStat fileStat;
    IoError ioError = IoError::Success;
    if (!IoHelper::getFileStat(getFilePath(), &fileStat, ioError, IoHelper::PathCheckOption::Insensitive) ||
        ioError != IoError::Success) {
        LOGW_WARN(getLogger(), L"CheckHashMatchJob: failed to get file size for " << Utility::formatSyncPath(getFilePath()));
        return ExitCode::Ok; // Non-fatal: fall through to upload
    }
    CheckHashMatchJob hashJob(_driveDbId, getFilePath(), _fileId, fileStat.size);
    const ExitInfo exitInfo = hashJob.runSynchronously();
    if (!exitInfo) {
        LOGW_DEBUG(getLogger(), L"CheckHashMatchJob failed: " << exitInfo << L" Proceeding UploadJob normally.");
        return ExitCode::Ok; // Non-fatal: fall through to upload
    }
    _shouldUpload = !hashJob.hashMatch();
    return ExitCode::Ok;
}

ExitInfo DriveUploadSession::runJob() noexcept {
    bool isCreate = _fileId.empty();
    if (!isCreate && _vfs) {
        // Get hydration status
        VfsStatus vfsStatus;
        (void) _vfs->status(getFilePath(), vfsStatus);
        _isHydrated = vfsStatus.isHydrated;
        if (_isHydrated) {
            if (const ExitInfo exitInfo = checkHashMatch(); !exitInfo) {
                LOGW_DEBUG(getLogger(), L"Error in checkHashMatch: " << exitInfo);
                return exitInfo;
            }
            if (!_shouldUpload) {
                LOGW_DEBUG(getLogger(), L"hanging last modified date without uploading");
                if (const ExitInfo exitInfo = applyFileDates(); !exitInfo) return exitInfo;
                return ExitCode::Ok;
            }
        }
    }
    return AbstractUploadSession::runJob();
}

ExitInfo DriveUploadSession::applyFileDates() {
    PostFileModificationDateJob postJob(_driveDbId, _fileId, _modificationTimeIn);
    const ExitInfo exitInfo = postJob.runSynchronously();
    if (!exitInfo) {
        LOGW_DEBUG(getLogger(), L"PostFileModificationDateJob failed: " << exitInfo);
        return exitInfo;
    }
    IoError ioError = IoError::Success;
    uint64_t fileSize = 0;
    IoHelper::getFileSize(getFilePath(), fileSize, ioError);

    _nodeId = _fileId;
    _modificationTimeOut = postJob.lastModifiedAt();
    _creationTimeOut = _creationTimeIn;
    _sizeOut = static_cast<int64_t>(fileSize);
    return ExitCode::Ok;
}

ExitInfo DriveUploadSession::runJobInit() {
    return ExitCode::Ok;
}

std::shared_ptr<UploadSessionStartJob> DriveUploadSession::createStartJob() {
    if (_fileId.empty()) {
        return std::make_shared<UploadSessionStartJob>(UploadSessionType::Drive, _driveDbId, getFileName(), getFileSize(),
                                                       _remoteParentDirId, getTotalChunks());
    } else {
        return std::make_shared<UploadSessionStartJob>(UploadSessionType::Drive, _driveDbId, _fileId, getFileSize(),
                                                       getTotalChunks());
    }
}

std::shared_ptr<UploadSessionChunkJob> DriveUploadSession::createChunkJob(const std::string &chunkContent, uint64_t chunkNb,
                                                                          std::streamsize actualChunkSize) {
    return std::make_shared<UploadSessionChunkJob>(UploadSessionType::Drive, _driveDbId, getFilePath(), getSessionToken(),
                                                   chunkContent, chunkNb, actualChunkSize, jobId());
}

std::shared_ptr<UploadSessionFinishJob> DriveUploadSession::createFinishJob() {
    return std::make_shared<UploadSessionFinishJob>(_vfs, UploadSessionType::Drive, _driveDbId, getFilePath(), getSessionToken(),
                                                    getTotalChunkHash(), getTotalChunks(), _creationTimeIn, _modificationTimeIn);
}

std::shared_ptr<UploadSessionCancelJob> DriveUploadSession::createCancelJob() {
    return std::make_shared<UploadSessionCancelJob>(UploadSessionType::Drive, _driveDbId, getFilePath(), getSessionToken());
}

ExitInfo DriveUploadSession::handleStartJobResult(const std::shared_ptr<UploadSessionStartJob> startJob,
                                                  const std::string &uploadToken) {
    (void) startJob;
    if (_syncDb && !_syncDb->insertUploadSessionToken(UploadSessionToken(uploadToken), _uploadSessionTokenDbId)) {
        LOG_WARN(getLogger(), "Error in SyncDb::insertUploadSessionToken");
        return ExitCode::DbError;
    }
    return ExitCode::Ok;
}

ExitInfo DriveUploadSession::handleFinishJobResult(const std::shared_ptr<UploadSessionFinishJob> finishJob) {
    _nodeId = finishJob->nodeId();
    _creationTimeOut = finishJob->creationTime();
    _modificationTimeOut = finishJob->modificationTime();
    _sizeOut = finishJob->size();

    if (bool found = false; _syncDb && !_syncDb->deleteUploadSessionTokenByDbId(_uploadSessionTokenDbId, found)) {
        LOG_WARN(getLogger(), "Error in SyncDb::deleteUploadSessionTokenByDbId");
        return ExitCode::DbError;
    }

    return ExitCode::Ok;
}

ExitInfo DriveUploadSession::handleCancelJobResult(const std::shared_ptr<UploadSessionCancelJob> cancelJob) {
    if (const auto exitInfo = AbstractUploadSession::handleCancelJobResult(cancelJob); !exitInfo) {
        return exitInfo;
    }

    if (bool found = false; _syncDb && !_syncDb->deleteUploadSessionTokenByDbId(_uploadSessionTokenDbId, found)) {
        LOG_WARN(getLogger(), "Error in SyncDb::deleteUploadSessionTokenByDbId");
        return ExitCode::DbError;
    }

    return ExitCode::Ok;
}

} // namespace KDC
