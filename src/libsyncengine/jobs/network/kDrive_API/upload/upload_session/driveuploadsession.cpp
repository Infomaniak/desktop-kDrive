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
#include "jobs/network/kDrive_API/postfilemodificationdatejob.h"
#include "jobs/network/kDrive_API/upload/uploadneedresolver.h"
#include "utility/utility.h"

namespace KDC {

DriveUploadSession::DriveUploadSession(const std::shared_ptr<Vfs> vfs, const DriveDbId driveDbId,
                                       const std::shared_ptr<SyncDb> syncDb, const SyncPath &filepath, const SyncName &filename,
                                       const NodeId &remoteParentDirId, const SyncTime creationTime,
                                       const SyncTime modificationTime, const uint64_t nbParallelThread) :
    AbstractUploadSession(filepath, filename, nbParallelThread),
    _driveDbId(driveDbId),
    _syncDb(syncDb),
    _creationTimeIn(creationTime),
    _modificationTimeIn(modificationTime),
    _remoteParentDirId(remoteParentDirId),
    _vfs(vfs) {
    _uploadSessionType = UploadSessionType::Drive;
}

DriveUploadSession::DriveUploadSession(const std::shared_ptr<Vfs> vfs, const DriveDbId driveDbId,
                                       const std::shared_ptr<SyncDb> syncDb, const SyncPath &filepath, const NodeId &fileId,
                                       const SyncTime modificationTime, const uint64_t nbParallelThread,
                                       const int64_t remoteSize /*= -1*/) :
    DriveUploadSession(vfs, driveDbId, syncDb, filepath, SyncName(), fileId, 0, modificationTime, nbParallelThread) {
    _fileId = fileId;
    _remoteSize = remoteSize;

    // Retrieve creation date from the local file
    FileStat fileStat;

    if (auto ioError = IoError::Unknown;
        !IoHelper::getFileStat(filepath, &fileStat, ioError, IoHelper::PathCheckOption::Insensitive) ||
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

ExitInfo DriveUploadSession::runJob() noexcept {
    if (!_fileId.empty()) {
        const auto result = KDC::resolveUploadNeed(getLogger(), _driveDbId, getFilePath(), _fileId, _remoteSize,
                                                   [this] { return applyFileDates(); });
        if (!result.shouldUpload && result.exitInfo) return ExitCode::Ok;
        LOGW_DEBUG(getLogger(), L"resolveUploadNeed: proceeding with upload - " << result.exitInfo);
    }
    return AbstractUploadSession::runJob();
}

ExitInfo DriveUploadSession::applyFileDates() {
    PostFileModificationDateJob postJob(_driveDbId, _fileId, _modificationTimeIn);
    if (const auto exitInfo = postJob.runSynchronously(); !exitInfo) {
        LOGW_DEBUG(getLogger(), L"PostFileModificationDateJob failed: " << exitInfo);
        return exitInfo;
    }

    uint64_t fileSize = 0;
    if (auto ioError = IoError::Success; !IoHelper::getFileSize(getFilePath(), fileSize, ioError)) {
        LOGW_WARN(getLogger(), L"Error in IoHelper::getFileSize for " << Utility::formatIoError(getFilePath(), ioError));
    } else if (ioError != IoError::Success) {
        LOGW_WARN(getLogger(), L"Unable to read file size for " << Utility::formatIoError(getFilePath(), ioError));
    }

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
