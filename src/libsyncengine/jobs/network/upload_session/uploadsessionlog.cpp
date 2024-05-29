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

#include "uploadsessionlog.h"
#include "libparms/db/parmsdb.h"

namespace KDC {

UploadSessionLog::UploadSessionLog(const SyncPath &filepath, uint64_t nbParalleleThread /*= 1*/)
    : AbstractUploadSession(filepath, filepath.filename(), nbParalleleThread) {
    _uploadSessionType = UploadSessionType::LogUpload;
}

bool UploadSessionLog::runJobInit() {
    return true;
}

std::shared_ptr<UploadSessionStartJob> UploadSessionLog::createStartJob() {
    return std::make_shared<UploadSessionStartJob>(UploadSessionType::LogUpload, getFileName(), getFileSize(), getTotalChunks());
}

std::shared_ptr<UploadSessionChunkJob> UploadSessionLog::createChunkJob(const std::string &chunckContent, uint64_t chunkNb,
                                                                        std::streamsize actualChunkSize) {
    return std::make_shared<UploadSessionChunkJob>(UploadSessionType::LogUpload, getFilePath(), getSessionToken(), chunckContent,
                                                   chunkNb, actualChunkSize, jobId());
}

std::shared_ptr<UploadSessionFinishJob> UploadSessionLog::createFinishJob() {
    SyncTime modtimeIn =
        std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count();

    return std::make_shared<UploadSessionFinishJob>(UploadSessionType::LogUpload, getFilePath(), getSessionToken(),
                                                    getTotalChunkHash(), getTotalChunks(), modtimeIn);
}

std::shared_ptr<UploadSessionCancelJob> UploadSessionLog::createCancelJob() {
    return std::make_shared<UploadSessionCancelJob>(UploadSessionType::LogUpload, getSessionToken());
}

bool UploadSessionLog::prepareChunkJob(const std::shared_ptr<UploadSessionChunkJob> &chunkJob) {
    return true;
}

bool UploadSessionLog::handleStartJobResult(const std::shared_ptr<UploadSessionStartJob> &StartJob, std::string uploadToken) {
    AppStateValue appStateValue = ""; // 
    if (bool found = false; !ParmsDb::instance()->selectAppState(AppStateKey::LogUploadToken, appStateValue, found) || !found) {
        LOG_ERROR(getLogger(), "Error in ParmsDb::selectAppState");
    }
    std::string logUploadToken = std::get<std::string>(appStateValue);
    if (!logUploadToken.empty()) {
        UploadSessionCancelJob cancelJob(UploadSessionType::LogUpload, logUploadToken);
        ExitCode exitCode = cancelJob.runSynchronously();
        if (exitCode != ExitCodeOk) {
            LOG_WARN(getLogger(), "Error in UploadSessionCancelJob::runSynchronously : " << exitCode);
        } else {
            LOG_INFO(getLogger(), "Previous Log upload api call cancelled");
            bool found = true;
            if (!ParmsDb::instance()->updateAppState(AppStateKey::LogUploadToken, std::string(), found) || !found) {
                LOG_WARN(getLogger(), "Error in ParmsDb::updateAppState");
            }
        }
    }

    AppStateValue value = uploadToken;
    if (bool found = false; !ParmsDb::instance()->updateAppState(AppStateKey::LogUploadToken, value, found) || !found) {
        LOG_WARN(getLogger(), "Error in ParmsDb::updateAppState");
    }
    return true;
}

bool UploadSessionLog::handleFinishJobResult(const std::shared_ptr<UploadSessionFinishJob> &finishJob) {
    bool found = true;
    if (!ParmsDb::instance()->updateAppState(AppStateKey::LogUploadToken, std::string(), found) || !found) {
        LOG_WARN(getLogger(), "Error in ParmsDb::updateAppState");
    }
    return true;
}

bool UploadSessionLog::handleCancelJobResult(const std::shared_ptr<UploadSessionCancelJob> &cancelJob) {
    if (!AbstractUploadSession::handleCancelJobResult(cancelJob)) {
        return false;
    }
    return handleFinishJobResult(nullptr);
}
}  // namespace KDC
