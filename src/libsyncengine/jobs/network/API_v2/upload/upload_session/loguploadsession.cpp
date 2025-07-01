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

#include "loguploadsession.h"
#include "libparms/db/parmsdb.h"
#include "log/log.h"

namespace KDC {

LogUploadSession::LogUploadSession(const SyncPath &filepath, const uint64_t nbParallelThread) :
    AbstractUploadSession(filepath, filepath.filename(), nbParallelThread) {
    _uploadSessionType = UploadSessionType::Log;
}

bool LogUploadSession::runJobInit() {
    return true;
}

std::shared_ptr<UploadSessionStartJob> LogUploadSession::createStartJob() {
    return std::make_shared<UploadSessionStartJob>(UploadSessionType::Log, getFileName(), getFileSize(), getTotalChunks());
}

std::shared_ptr<UploadSessionChunkJob> LogUploadSession::createChunkJob(const std::string &chunkContent, uint64_t chunkNb,
                                                                        std::streamsize actualChunkSize) {
    return std::make_shared<UploadSessionChunkJob>(UploadSessionType::Log, getFilePath(), getSessionToken(), chunkContent,
                                                   chunkNb, actualChunkSize, jobId());
}

std::shared_ptr<UploadSessionFinishJob> LogUploadSession::createFinishJob() {
    SyncTime modtimeIn =
            std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count();

    return std::make_shared<UploadSessionFinishJob>(UploadSessionType::Log, getFilePath(), getSessionToken(), getTotalChunkHash(),
                                                    getTotalChunks(), modtimeIn, modtimeIn);
}

std::shared_ptr<UploadSessionCancelJob> LogUploadSession::createCancelJob() {
    return std::make_shared<UploadSessionCancelJob>(UploadSessionType::Log, getSessionToken());
}

bool LogUploadSession::handleStartJobResult(const std::shared_ptr<UploadSessionStartJob> &startJob,
                                            const std::string &uploadToken) {
    (void) startJob;

    AppStateValue appStateValue = "";
    if (bool found = false; !ParmsDb::instance()->selectAppState(AppStateKey::LogUploadToken, appStateValue, found) || !found) {
        LOG_WARN(getLogger(), "Error in ParmsDb::selectAppState");
    }

    if (const std::string logUploadToken = std::get<std::string>(appStateValue); !logUploadToken.empty()) {
        std::unique_ptr<UploadSessionCancelJob> cancelJob;
        try {
            cancelJob = std::make_unique<UploadSessionCancelJob>(UploadSessionType::Log, logUploadToken);
        } catch (const std::runtime_error &e) {
            LOG_WARN(getLogger(), "Error in UploadSessionCancelJob::UploadSessionCancelJob: error=" << e.what());
            return false;
        }

        if (const ExitCode exitCode = cancelJob->runSynchronously(); exitCode != ExitCode::Ok) {
            LOG_WARN(getLogger(), "Error in UploadSessionCancelJob::runSynchronously: code=" << exitCode);
        } else {
            LOG_INFO(getLogger(), "Previous Log upload api call cancelled");
            if (bool found = true;
                !ParmsDb::instance()->updateAppState(AppStateKey::LogUploadToken, std::string(), found) || !found) {
                LOG_WARN(getLogger(), "Error in ParmsDb::updateAppState");
            }
        }
    }

    if (bool found = false; !ParmsDb::instance()->updateAppState(AppStateKey::LogUploadToken, uploadToken, found) || !found) {
        LOG_WARN(getLogger(), "Error in ParmsDb::updateAppState");
    }
    return true;
}

bool LogUploadSession::handleFinishJobResult(const std::shared_ptr<UploadSessionFinishJob> &finishJob) {
    (void) finishJob;

    if (bool found = true; !ParmsDb::instance()->updateAppState(AppStateKey::LogUploadToken, std::string(), found) || !found) {
        LOG_WARN(getLogger(), "Error in ParmsDb::updateAppState");
    }
    return true;
}

bool LogUploadSession::handleCancelJobResult(const std::shared_ptr<UploadSessionCancelJob> &cancelJob) {
    if (!AbstractUploadSession::handleCancelJobResult(cancelJob)) {
        return false;
    }
    return handleFinishJobResult(nullptr);
}
} // namespace KDC
