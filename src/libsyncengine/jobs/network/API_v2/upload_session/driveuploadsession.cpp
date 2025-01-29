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

#include "driveuploadsession.h"
#include "utility/utility.h"

namespace KDC {
DriveUploadSession::DriveUploadSession(const std::shared_ptr<Vfs> &vfs, int driveDbId, std::shared_ptr<SyncDb> syncDb,
                                       const SyncPath &filepath, const NodeId &fileId, SyncTime modtime, bool liteSyncActivated,
                                       uint64_t nbParalleleThread /*= 1*/) :
    DriveUploadSession(vfs, driveDbId, syncDb, filepath, SyncName(), fileId, modtime, liteSyncActivated, nbParalleleThread) {
    _fileId = fileId;
}

DriveUploadSession::DriveUploadSession(const std::shared_ptr<Vfs> &vfs, int driveDbId, std::shared_ptr<SyncDb> syncDb,
                                       const SyncPath &filepath, const SyncName &filename, const NodeId &remoteParentDirId,
                                       SyncTime modtime, bool liteSyncActivated, uint64_t nbParalleleThread /*= 1*/) :
    AbstractUploadSession(filepath, filename, nbParalleleThread), _driveDbId(driveDbId), _syncDb(syncDb), _modtimeIn(modtime),
    _remoteParentDirId(remoteParentDirId), _vfs(vfs) {
    (void) liteSyncActivated;
    _uploadSessionType = UploadSessionType::Drive;
}

DriveUploadSession::~DriveUploadSession() {
    if (!_vfs) return;
    if (ExitInfo exitInfo = _vfs->forceStatus(getFilePath(), false, 100, true); !exitInfo) {
        LOGW_WARN(getLogger(), L"Error in vfsForceStatus: " << Utility::formatSyncPath(getFilePath()) << L" : " << exitInfo);
    }
}

bool DriveUploadSession::runJobInit() {
    return true;
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

std::shared_ptr<UploadSessionChunkJob> DriveUploadSession::createChunkJob(const std::string &chunckContent, uint64_t chunkNb,
                                                                          std::streamsize actualChunkSize) {
    return std::make_shared<UploadSessionChunkJob>(UploadSessionType::Drive, _driveDbId, getFilePath(), getSessionToken(),
                                                   chunckContent, chunkNb, actualChunkSize, jobId());
}

std::shared_ptr<UploadSessionFinishJob> DriveUploadSession::createFinishJob() {
    return std::make_shared<UploadSessionFinishJob>(_vfs, UploadSessionType::Drive, _driveDbId, getFilePath(), getSessionToken(),
                                                    getTotalChunkHash(), getTotalChunks(), _modtimeIn);
}

std::shared_ptr<UploadSessionCancelJob> DriveUploadSession::createCancelJob() {
    return std::make_shared<UploadSessionCancelJob>(UploadSessionType::Drive, _driveDbId, getFilePath(), getSessionToken());
}

bool DriveUploadSession::handleStartJobResult(const std::shared_ptr<UploadSessionStartJob> &startJob, std::string uploadToken) {
    (void) startJob;
    if (_syncDb && !_syncDb->insertUploadSessionToken(UploadSessionToken(uploadToken), _uploadSessionTokenDbId)) {
        LOG_WARN(getLogger(), "Error in SyncDb::insertUploadSessionToken");
        _exitCode = ExitCode::DbError;
        return false;
    }
    return true;
}

bool DriveUploadSession::handleFinishJobResult(const std::shared_ptr<UploadSessionFinishJob> &finishJob) {
    _nodeId = finishJob->nodeId();
    _modtimeOut = finishJob->modtime();

    bool found = false;
    if (_syncDb && !_syncDb->deleteUploadSessionTokenByDbId(_uploadSessionTokenDbId, found)) {
        LOG_WARN(getLogger(), "Error in SyncDb::deleteUploadSessionTokenByDbId");
        _exitCode = ExitCode::DbError;
        return false;
    }

    return true;
}

bool DriveUploadSession::handleCancelJobResult(const std::shared_ptr<UploadSessionCancelJob> &cancelJob) {
    if (!AbstractUploadSession::handleCancelJobResult(cancelJob)) {
        return false;
    }

    bool found = false;
    if (_syncDb && !_syncDb->deleteUploadSessionTokenByDbId(_uploadSessionTokenDbId, found)) {
        LOG_WARN(getLogger(), "Error in SyncDb::deleteUploadSessionTokenByDbId");
        _exitCode = ExitCode::DbError;
        return false;
    }

    return true;
}

void DriveUploadSession::abort() {
    AbstractUploadSession::abort();
    setVfsForceStatusCallback(nullptr);
}

} // namespace KDC
