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

#ifdef _WIN32
#define _WINSOCKAPI_
#endif

#include "logarchiverhelper.h"
#include "logarchiver.h"
#include "libparms/db/parmsdb.h"
#include "log/log.h"
#include "jobs/network/API_v2/upload_session/loguploadsession.h"
#include "jobs/jobmanager.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"

#include <log4cplus/loggingmacros.h>

namespace KDC {

ExitCode LogArchiverHelper::sendLogToSupport(bool includeArchivedLog,
                                             const std::function<bool(LogUploadState, int)> &progressCallback,
                                             ExitCause &exitCause) {
    exitCause = ExitCause::Unknown;
    ExitCode exitCode = ExitCode::Ok;
    std::function<bool(LogUploadState, int)> safeProgressCallback =
            progressCallback ? progressCallback
                             : std::function<bool(LogUploadState, int)>([](LogUploadState, int) { return true; });

    safeProgressCallback(LogUploadState::Archiving, 0);

    if (bool found = false;
        !ParmsDb::instance()->updateAppState(AppStateKey::LastLogUploadArchivePath, std::string(""), found) || !found) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateAppState");
        // Do not return here because it is not a critical error, especially in this context where we are trying to send logs
    }

    SyncPath logUploadTempFolder;
    IoError ioError = IoError::Success;

    IoHelper::logArchiverDirectoryPath(logUploadTempFolder, ioError);
    if (ioError != IoError::Success) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in IoHelper::logArchiverDirectoryPath: "
                                                        << Utility::formatIoError(logUploadTempFolder, ioError).c_str());
        return ExitCode::SystemError;
    }

    IoHelper::createDirectory(logUploadTempFolder, ioError);
    if (ioError == IoError::DirectoryExists) { // If the directory already exists, we delete it and recreate it
        IoHelper::deleteItem(logUploadTempFolder, ioError);
        IoHelper::createDirectory(logUploadTempFolder, ioError);
    }

    if (ioError != IoError::Success) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in IoHelper::createDirectory: " << Utility::formatIoError(logUploadTempFolder, ioError).c_str());
        exitCause = ioError == IoError::DiskFull ? ExitCause::NotEnoughDiskSpace : ExitCause::Unknown;
        return ExitCode::SystemError;
    }

    std::function<bool(int)> progressCallbackArchivingWrapper = [&safeProgressCallback](int percent) {
        return safeProgressCallback(LogUploadState::Archiving, percent);
    };

    SyncPath archivePath;
    exitCode = LogArchiver::generateLogsSupportArchive(includeArchivedLog, logUploadTempFolder, progressCallbackArchivingWrapper,
                                                       archivePath, exitCause);
    if (exitCause == ExitCause::OperationCanceled) {
        IoHelper::deleteItem(logUploadTempFolder, ioError);
        LOG_INFO(Log::instance()->getLogger(),
                 "LogArchiver::generateLogsSupportArchive canceled: code=" << exitCode << " cause=" << exitCause);
        return ExitCode::Ok;
    } else if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in LogArchiver::generateLogsSupportArchive: code=" << exitCode << " cause=" << exitCause);
        IoHelper::deleteItem(logUploadTempFolder, ioError);
        return exitCode;
    }

    if (bool found = false;
        !ParmsDb::instance()->updateAppState(AppStateKey::LastLogUploadArchivePath, archivePath.string(), found) || !found) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateAppState");
    }

    // Upload archive
    std::shared_ptr<LogUploadSession> uploadSessionLog = nullptr;
    try {
        uploadSessionLog = std::make_shared<LogUploadSession>(archivePath);
    } catch (std::exception const &e) {
        LOG_WARN(Log::instance()->getLogger(), "Error in LogUploadSession::LogUploadSession: error=" << e.what());
        return ExitCode::SystemError;
    };

    bool canceledByUser = false;
    std::function<void(UniqueId, int percent)> progressCallbackUploadingWrapper =
            [&safeProgressCallback, &uploadSessionLog, &canceledByUser](UniqueId, int percent) { // Progress callback
                if (!safeProgressCallback(LogUploadState::Uploading, percent)) {
                    uploadSessionLog->abort();
                    canceledByUser = true;
                };
            };
    uploadSessionLog->setProgressPercentCallback(progressCallbackUploadingWrapper);

    bool jobFinished = false;
    std::function<void(uint64_t)> uploadSessionLogFinisCallback = [&safeProgressCallback, &jobFinished](uint64_t) {
        safeProgressCallback(LogUploadState::Uploading, 100);
        jobFinished = true;
    };
    uploadSessionLog->setAdditionalCallback(uploadSessionLogFinisCallback);

    JobManager::instance()->queueAsyncJob(uploadSessionLog, Poco::Thread::PRIO_NORMAL);

    while (!jobFinished) {
        Utility::msleep(100);
    }

    exitCode = uploadSessionLog->exitCode();
    if (canceledByUser) {
        exitCause = ExitCause::OperationCanceled;
    } else {
        exitCause = uploadSessionLog->exitCause();
    }

    if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Error during log upload: code=" << exitCode << " cause=" << exitCause);
        // We do not delete the archive here. The path is stored in the app state so that the user can still try to upload it
        // manually.
        return exitCode;
    }

    IoHelper::deleteItem(logUploadTempFolder, ioError); // Delete temp folder if the upload was successful

    if (exitCause != ExitCause::OperationCanceled) {
        std::string uploadDate = "";
        const std::time_t now = std::time(nullptr);
        const std::tm tm = *std::localtime(&now);
        std::ostringstream woss;
        woss << std::put_time(&tm, "%m,%d,%y,%H,%M,%S");
        uploadDate = woss.str();

        if (std::pair<bool, bool> found{false, false};
            !ParmsDb::instance()->updateAppState(AppStateKey::LastSuccessfulLogUploadDate, uploadDate, found.first) ||
            !found.first ||
            !ParmsDb::instance()->updateAppState(AppStateKey::LastLogUploadArchivePath, std::string{}, found.second) ||
            !found.second) {
            LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateAppState");
        }
    }
    return ExitCode::Ok;
}

ExitCode LogArchiverHelper::cancelLogToSupport(ExitCause &exitCause) {
    exitCause = ExitCause::Unknown;
    AppStateValue appStateValue = LogUploadState::None;
    if (bool found = false; !ParmsDb::instance()->selectAppState(AppStateKey::LogUploadState, appStateValue, found) || !found) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::getAppState");
        return ExitCode::DbError;
    }
    LogUploadState logUploadState = std::get<LogUploadState>(appStateValue);

    if (logUploadState == LogUploadState::CancelRequested) {
        return ExitCode::Ok;
    }

    if (logUploadState == LogUploadState::Canceled) {
        exitCause = ExitCause::OperationCanceled;
        return ExitCode::Ok; // The user has already canceled the operation
    }

    if (logUploadState != LogUploadState::Uploading && logUploadState != LogUploadState::Archiving) {
        return ExitCode::InvalidOperation; // The operation is not in progress
    }

    if (bool found = false;
        !ParmsDb::instance()->updateAppState(AppStateKey::LogUploadState, LogUploadState::CancelRequested, found) || !found) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateAppState");
        return ExitCode::DbError;
    }

    return ExitCode::Ok;
}


} // namespace KDC
