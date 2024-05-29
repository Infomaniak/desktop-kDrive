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

#include "uploadsessiondrive.h"

namespace KDC {

UploadSessionDrive::UploadSessionDrive(int driveDbId, std::shared_ptr<SyncDb> syncDb, const SyncPath &filepath,
                                       const SyncName &filename, const NodeId &remoteParentDirId, SyncTime modtime,
                                       bool liteSyncActivated, uint64_t nbParalleleThread /*= 1*/)
    : AbstractUploadSession(filepath, filename, nbParalleleThread),
      _remoteParentDirId(remoteParentDirId),
      _liteSyncActivated(liteSyncActivated),
      _modtimeIn(modtime),
      _syncDb(syncDb),
      _driveDbId(driveDbId) {
    _uploadSessionType = UploadSessionType::Standard;
}

UploadSessionDrive::~UploadSessionDrive() {
    if (_vfsForceStatus) {
        if (!_vfsForceStatus(getFilePath(), false, 100, true)) {
            LOG_WARN(getLogger(), L"Error in vfsForceStatus - path=" << getFilePath().c_str());
        }
    }
}

bool UploadSessionDrive::runJobInit() {
    if (_vfsForceStatus) {
        if (!_vfsForceStatus(getFilePath(), true, 0, true)) {
            LOG_WARN(getLogger(), L"Error in vfsForceStatus - path=" << getFilePath().c_str());
        }
    }
    return true;
}

std::shared_ptr<UploadSessionStartJob> UploadSessionDrive::createStartJob() {
    return std::make_shared<UploadSessionStartJob>(UploadSessionType::Standard, _driveDbId, getFileName(), getFileSize(),
                                                   _remoteParentDirId, getTotalChunks());
}

std::shared_ptr<UploadSessionChunkJob> UploadSessionDrive::createChunkJob(const std::string &chunckContent, uint64_t chunkNb,
                                                                          std::streamsize actualChunkSize) {
    return std::make_shared<UploadSessionChunkJob>(UploadSessionType::Standard, _driveDbId, getFilePath(), getSessionToken(),
                                                   chunckContent, chunkNb, actualChunkSize, jobId());
}

std::shared_ptr<UploadSessionFinishJob> UploadSessionDrive::createFinishJob() {
    return std::make_shared<UploadSessionFinishJob>(UploadSessionType::Standard, _driveDbId, getFilePath(), getSessionToken(),
                                                    getTotalChunkHash(), getTotalChunks(), _modtimeIn);
}

std::shared_ptr<UploadSessionCancelJob> UploadSessionDrive::createCancelJob() {
    return std::make_shared<UploadSessionCancelJob>(UploadSessionType::Standard, _driveDbId, getFilePath(), getSessionToken());
}

bool UploadSessionDrive::prepareChunkJob(const std::shared_ptr<UploadSessionChunkJob> &chunkJob) {
    if (_liteSyncActivated) {
        // Set VFS callbacks
        chunkJob->setVfsUpdateFetchStatusCallback(_vfsUpdateFetchStatus);
        chunkJob->setVfsSetPinStateCallback(_vfsSetPinState);
        chunkJob->setVfsForceStatusCallback(_vfsForceStatus);
    }
    return true;
}

bool UploadSessionDrive::handleStartJobResult(const std::shared_ptr<UploadSessionStartJob> &StartJob, std::string uploadToken) {
    if (_syncDb && !_syncDb->insertUploadSessionToken(UploadSessionToken(uploadToken), _uploadSessionTokenDbId)) {
        LOG_WARN(getLogger(), "Error in SyncDb::insertUploadSessionToken");
        _exitCode = ExitCodeDbError;
        return false;
    }
    return true;
}

bool UploadSessionDrive::handleFinishJobResult(const std::shared_ptr<UploadSessionFinishJob> &finishJob) {
    _nodeId = finishJob->nodeId();
    _modtimeOut = finishJob->modtime();

    bool found = false;
    if (_syncDb && !_syncDb->deleteUploadSessionTokenByDbId(_uploadSessionTokenDbId, found)) {
        LOG_WARN(getLogger(), "Error in SyncDb::deleteUploadSessionTokenByDbId");
        _exitCode = ExitCodeDbError;
        return false;
    }

    return true;
}

bool UploadSessionDrive::handleCancelJobResult(const std::shared_ptr<UploadSessionCancelJob> &cancelJob) {
    if (!AbstractUploadSession::handleCancelJobResult(cancelJob)) {
        return false;
    }

    bool found = false;
    if (_syncDb && !_syncDb->deleteUploadSessionTokenByDbId(_uploadSessionTokenDbId, found)) {
        LOG_WARN(getLogger(), "Error in SyncDb::deleteUploadSessionTokenByDbId");
        _exitCode = ExitCodeDbError;
        return false;
    }

    return true;
}
}  // namespace KDC
