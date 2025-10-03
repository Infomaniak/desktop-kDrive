// Infomaniak kDrive - Desktop
// Copyright (C) 2023-2025 Infomaniak Network SA
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "loguploadjob.h"
#include "libcommon/utility/types.h"
#include "libcommon/utility/utility.h"

#include "libcommonserver/commonserverlib.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/log/log.h"

#include "libparms/db/user.h"
#include "libparms/db/parmsdb.h"
#include "libparms/db/drive.h"

#include "upload_session/loguploadsession.h"

#include <fstream>
#include <zip.h>

namespace KDC {

std::mutex LogUploadJob::_runningJobMutex = std::mutex();
std::shared_ptr<LogUploadJob> LogUploadJob::_runningJob = nullptr;

LogUploadJob::LogUploadJob(const bool includeArchivedLog, const std::function<void(LogUploadState, int)> &progressCallback,
                           const std::function<void(const Error &error)> &addErrorCallback) :
    _includeArchivedLog(includeArchivedLog),
    _progressCallback(progressCallback),
    _addErrorCallback(addErrorCallback) {
    if (!_progressCallback) {
        assert(_progressCallback && "progressCallback must be set");
        _progressCallback = [](LogUploadState, int) { return true; };
    }
    if (!_addErrorCallback) {
        assert(_addErrorCallback && "addErrorCallback must be set");
        _addErrorCallback = [](const Error &) { return true; };
    }
}

void LogUploadJob::abort() {
    SyncJob::abort();
    const LogUploadState logUploadState = getDbUploadState();

    if (logUploadState == LogUploadState::CancelRequested || logUploadState == LogUploadState::Canceled) {
        LOG_INFO(Log::instance()->getLogger(), "The operation is already canceled");
        return;
    }

    if (logUploadState != LogUploadState::Uploading && logUploadState != LogUploadState::Archiving) {
        LOG_INFO(Log::instance()->getLogger(), "The operation is not in progress");
        return;
    }

    updateDbUploadState(LogUploadState::CancelRequested);
    if (const ExitInfo exitInfo = notifyLogUploadProgress(LogUploadState::CancelRequested, 0); !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Error in LogUploadJob::notifyLogUploadProgress: " << exitInfo);
    }
}

void LogUploadJob::cancelUpload() {
    const std::scoped_lock lock(_runningJobMutex);
    if (_runningJob) {
        LOG_INFO(Log::instance()->getLogger(), "Cancelling log upload job.");
        _runningJob->abort();
        return;
    }
    LOG_WARN(Log::instance()->getLogger(), "No log upload in progress, unable to cancel the job.");
}

bool LogUploadJob::getLogDirEstimatedSize(uint64_t &size, IoError &ioError) {
    const SyncPath logPath = Log::instance()->getLogFilePath().parent_path();
    bool result = false;
    size = 0;
    ioError = IoError::Unknown;
    for (int i = 0; i < 2; i++) { // Retry once in case a log file is archived/created during the first iteration
        result = IoHelper::getDirectorySize(logPath, size, ioError);
        if (ioError == IoError::Success) {
            size = static_cast<uint64_t>(static_cast<double>(size) *
                                         0.8); // The compressed logs will be smaller than the original ones. We estimate at worst
                                               // 80% of the original size.
            return true;
        }
        Utility::msleep(100);
    }
    LOGW_WARN(Log::instance()->getLogger(),
              L"Error in LogArchiver::getLogDirEstimatedSize: " << Utility::formatIoError(logPath, ioError));

    return result;
}

ExitInfo LogUploadJob::runJob() {
    if (const auto exitInfo = canRun(); !exitInfo) {
        LOG_DEBUG(Log::instance()->getLogger(), "LogUploadJob job cannot run.");
        return ExitCode::Ok;
    }

    if (const ExitInfo exitInfo = init(); !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Error in LogUploadJob::init: " << exitInfo);
        handleJobFailure(exitInfo);
        return exitInfo;
    }

    if (const ExitInfo exitInfo = archive(_generatedArchivePath); !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Error in LogUploadJob::archive: " << exitInfo);
        handleJobFailure(exitInfo);
        return exitInfo;
    }

    // Save the path of the generated archive, so the user can attach it to the support request if the upload fails
    if (bool found = false;
        !ParmsDb::instance()->updateAppState(AppStateKey::LastLogUploadArchivePath, _generatedArchivePath.string(), found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateAppState");
        _addErrorCallback(Error(ERR_ID, ExitCode::DbError, ExitCause::DbAccessError));
    } else if (!found) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in ParmsDb::updateAppState: " << AppStateKey::LastLogUploadArchivePath << " key not found");
    }

    if (const ExitInfo exitInfo = upload(_generatedArchivePath); !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Error in LogUploadJob::sendLogToSupport: " << exitInfo);
        handleJobFailure(exitInfo, false);
        return exitInfo;
    }

    finalize();
    return ExitCode::Ok;
}

ExitInfo LogUploadJob::init() {
    if (bool found = false; !ParmsDb::instance()->updateAppState(AppStateKey::LogUploadState, LogUploadState::None, found) ||
                            !found) { // Reset status
        LOG_WARN(_logger, "Error in ParmsDb::updateAppState");
        // Do not return here because it is not a critical error, especially in this context where we are trying to send logs
    }

    // Ensure that the UI is aware that we are archiving the logs
    if (const ExitInfo exitInfo = notifyLogUploadProgress(LogUploadState::Archiving, 0); !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Error in LogUploadJob::notifyLogUploadProgress: " << exitInfo);
        return exitInfo;
    }

    if (bool found = false;
        !ParmsDb::instance()->updateAppState(AppStateKey::LastLogUploadArchivePath, std::string{}, found) || !found) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateAppState");
        // Do not return here because it is not a critical error, especially in this context where we are trying to send logs
    }

    if (const ExitInfo exitInfo = getTmpJobWorkingDir(_tmpJobWorkingDir); !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Error in LogUploadJob::getTmpJobWorkingDir: " << exitInfo);
        return exitInfo;
    }

    return ExitCode::Ok;
}

void LogUploadJob::finalize() {
    std::string uploadDate;
    const std::time_t now = std::time(nullptr);
    const std::tm tm = *std::localtime(&now);
    std::ostringstream woss;
    woss << std::put_time(&tm, "%m,%d,%y,%H,%M,%S");
    uploadDate = woss.str();

    if (bool found = false;
        !ParmsDb::instance()->updateAppState(AppStateKey::LastSuccessfulLogUploadDate, uploadDate, found) || !found) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateAppState " << AppStateKey::LastSuccessfulLogUploadDate);
    }
    if (bool found = false;
        !ParmsDb::instance()->updateAppState(AppStateKey::LastLogUploadArchivePath, std::string{}, found) || !found) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateAppState " << AppStateKey::LastLogUploadArchivePath);
    }

    auto ioError = IoError::Success;
    (void) IoHelper::deleteItem(_tmpJobWorkingDir, ioError);
    if (ioError != IoError::Success) {
        LOGW_INFO(Log::instance()->getLogger(),
                  L"Error in IoHelper::deleteItem: " << Utility::formatIoError(_tmpJobWorkingDir, ioError));
    }

    (void) IoHelper::deleteItem(_generatedArchivePath, ioError);
    if (ioError != IoError::Success) {
        LOGW_INFO(Log::instance()->getLogger(),
                  L"Error in IoHelper::deleteItem: " << Utility::formatIoError(_generatedArchivePath, ioError));
    }

    (void) notifyLogUploadProgress(LogUploadState::Success, 100);
    _runningJob.reset();
}

ExitInfo LogUploadJob::canRun() {
    const std::scoped_lock lock(_runningJobMutex);
    if (_runningJob) {
        LOG_WARN(Log::instance()->getLogger(), "Another log upload job is already running");
        return ExitInfo();
    }

    _runningJob = shared_from_this();
    return ExitCode::Ok;
}

ExitInfo LogUploadJob::archive(SyncPath &generatedArchivePath) {
    if (const ExitInfo exitInfo = notifyLogUploadProgress(LogUploadState::Archiving, 0); !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Error in LogUploadJob::notifyLogUploadProgress: " << exitInfo);
        return exitInfo;
    }

    if (const ExitInfo exitInfo = copyLogsTo(_tmpJobWorkingDir, _includeArchivedLog); !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Unable to copy logs to tmp folder: " << exitInfo);
        return exitInfo;
    }

    if (const ExitInfo exitInfo = copyParmsDbTo(_tmpJobWorkingDir); !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Unable to copy parms.db to tmp folder: " << exitInfo);
        return exitInfo;
    }

    if (const ExitInfo exitInfo = generateUserDescriptionFile(_tmpJobWorkingDir); !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Unable to generate user description file: " << exitInfo);
        return exitInfo;
    }

    SyncName archiveName;
    if (const ExitInfo exitInfo = getArchiveName(archiveName); !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Error in LogUploadJob::getArchiveName: " << exitInfo);
        return exitInfo;
    }

    if (const ExitInfo exitInfo = notifyLogUploadProgress(LogUploadState::Archiving, 10); !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Error in LogUploadJob::notifyLogUploadProgress: " << exitInfo);
        return exitInfo;
    }

    const SyncPath archivePath = _tmpJobWorkingDir.parent_path() / archiveName;

    if (const ExitInfo exitInfo =
                generateArchive(_tmpJobWorkingDir, _tmpJobWorkingDir.parent_path(), archiveName, generatedArchivePath);
        !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Unable to generate archive: " << exitInfo);
        return exitInfo;
    }

    if (const ExitInfo exitInfo = notifyLogUploadProgress(LogUploadState::Archiving, 100); !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Error in LogUploadJob::notifyLogUploadProgress: " << exitInfo);
        return exitInfo;
    }
    return ExitCode::Ok;
}

ExitInfo LogUploadJob::getTmpJobWorkingDir(SyncPath &tmpJobWorkingDir) const {
    // Get the log directory path
    const SyncPath logPath = Log::instance()->getLogFilePath().parent_path();
    tmpJobWorkingDir = logPath / ("tmpLogArchive_" + CommonUtility::generateRandomStringAlphaNum(10));

    // Create tmp folder
    if (IoError ioError = IoError::Unknown;
        !IoHelper::createDirectory(tmpJobWorkingDir, false, ioError) && ioError != IoError::DirectoryExists) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in IoHelper::createDirectory: " << Utility::formatIoError(tmpJobWorkingDir, ioError));
        switch (ioError) {
            case IoError::DiskFull:
                return {ExitCode::SystemError, ExitCause::NotEnoughDiskSpace};
            case IoError::AccessDenied:
                return {ExitCode::SystemError, ExitCause::FileAccessError};
            default:
                return ExitCode::SystemError;
        }
    }

    return ExitCode::Ok;
}

ExitInfo LogUploadJob::getArchiveName(SyncName &archiveName) const {
    // Generate archive name: <drive id 1>-<drive id 2>...-<drive id N>-yyyyMMdd-HHmmss.zip
    std::string archiveNameStr;
    std::vector<Drive> driveList;
    if (!ParmsDb::instance()->selectAllDrives(driveList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllDrives");
        return {ExitCode::DbError, ExitCause::DbAccessError};
    }

    if (driveList.empty()) {
        LOG_WARN(Log::instance()->getLogger(), "No drive found - Unable to send log");
        return {ExitCode::InvalidToken, ExitCause::LoginError}; // Currently, we can't send logs if no drive is found
    }

    for (const auto &drive: driveList) {
        archiveNameStr += std::to_string(drive.driveId()) + "-";
    }

    const std::time_t now = std::time(nullptr);
    const std::tm tm = *std::localtime(&now);
    std::ostringstream woss;
    woss << std::put_time(&tm, "%Y%m%d_%H%M%S");
    archiveNameStr += woss.str();

    archiveName = Str2SyncName(archiveNameStr);
    return ExitCode::Ok;
}

ExitInfo LogUploadJob::copyLogsTo(const SyncPath &outputPath, const bool includeArchivedLogs) const {
    const SyncPath logFilePath = Log::instance()->getLogFilePath();
    const SyncPath logDirPath = logFilePath.parent_path();
    const std::string logFileDate = logFilePath.filename().string().substr(0, 14);
    IoError ioError = IoError::Success;
    IoHelper::DirectoryIterator dir;
    if (!IoHelper::getDirectoryIterator(logDirPath, false, ioError, dir)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in DirectoryIterator: " << Utility::formatIoError(logDirPath, ioError));
        return ExitCode::SystemError;
    }

    DirectoryEntry entry;
    bool endOfDirectory = false;
    while (dir.next(entry, endOfDirectory, ioError) && !endOfDirectory) {
        if (entry.is_directory()) {
            LOG_INFO(Log::instance()->getLogger(), "Ignoring temp directory " << entry.path().filename().string());
            continue;
        }

        if (!includeArchivedLogs && !entry.path().filename().string().starts_with(logFileDate) &&
            entry.path().filename().extension() != ".log") {
            LOG_INFO(Log::instance()->getLogger(), "Ignoring old log file " << entry.path().filename().string());
            continue;
        }

        if (!IoHelper::copyFileOrDirectory(entry.path(), outputPath / entry.path().filename(), ioError)) {
            LOGW_WARN(Log::instance()->getLogger(),
                      L"Error in IoHelper::copyFileOrDirectory: " << Utility::formatIoError(entry.path(), ioError));
            if (ioError == IoError::DiskFull) {
                return {ExitCode::SystemError, ExitCause::NotEnoughDiskSpace};
            }
            return ExitCode::SystemError;
        }
    }

    if (!endOfDirectory) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in DirectoryIterator: " << Utility::formatIoError(logDirPath, ioError));
        return ExitCode::SystemError;
    }

    return ExitCode::Ok;
}

ExitInfo LogUploadJob::copyParmsDbTo(const SyncPath &outputPath) const {
    const SyncPath parmsDbPath = ParmsDb::instance()->dbPath();
    auto ioError = IoError::Unknown;
    if (DirectoryEntry entryParmsDb; !IoHelper::getDirectoryEntry(parmsDbPath, ioError, entryParmsDb)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in IoHelper::getDirectoryEntry: " << Utility::formatIoError(parmsDbPath, ioError));
        if (ioError == IoError::NoSuchFileOrDirectory) {
            return {ExitCode::SystemError, ExitCause::NotFound};
        } else {
            return ExitCode::SystemError;
        }
    }

    if (!IoHelper::copyFileOrDirectory(parmsDbPath, outputPath, ioError)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in IoHelper::copyFileOrDirectory: " << Utility::formatIoError(parmsDbPath, ioError));

        if (ioError == IoError::DiskFull) {
            return {ExitCode::SystemError, ExitCause::NotEnoughDiskSpace};
        }
        return ExitCode::SystemError;
    }
    return ExitCode::Ok;
}

ExitInfo LogUploadJob::generateUserDescriptionFile(const SyncPath &outputPath) const {
    const std::string osName = CommonUtility::platformName().toStdString();
    const std::string osArch = CommonUtility::platformArch().toStdString();
    const std::string appVersion = CommonUtility::userAgentString();

    std::ofstream file((outputPath / "user_description.txt").string());
    if (!file.is_open()) {
        LOGW_WARN(Log::instance()->getLogger(), L"Unable to create : " << Utility::formatSyncPath(outputPath));
        return ExitCode::Ok; // We don't want to stop the process if we can't create the file
    }
    file << "OS Name: " << osName << std::endl;
    file << "OS Architecture: " << osArch << std::endl;
    file << "App Version: " << appVersion << std::endl;
    file << "User ID(s): ";

    if (std::vector<User> userList; ParmsDb::instance()->selectAllUsers(userList)) {
        for (const User &user: userList) {
            file << user.userId() << " | ";
        }
        file << std::endl;
    } else {
        file << "Unable to retrieve user ID(s)" << std::endl;
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllUsers");
        _addErrorCallback(Error(ERR_ID, ExitCode::DbError, ExitCause::DbAccessError));
    }

    file << "Drive ID(s): ";
    if (std::vector<Drive> driveList; ParmsDb::instance()->selectAllDrives(driveList)) {
        for (const Drive &drive: driveList) {
            file << drive.driveId() << " | ";
        }
        file << std::endl;
    } else {
        file << "Unable to retrieve drive ID(s)" << std::endl;
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllUsers");
        _addErrorCallback(Error(ERR_ID, ExitCode::DbError, ExitCause::DbAccessError));
    }


    file.close();
    if (file.bad()) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in file.close() for " << Utility::formatSyncPath(outputPath));
        // We don't want to stop the process if we can't close the file
    }

    return ExitCode::Ok;
}

ExitInfo LogUploadJob::generateArchive(const SyncPath &directoryToCompress, const SyncPath &destPath,
                                       const SyncName &archiveNameWithoutExtension, SyncPath &finalPath) {
    // Generate the archive
    const SyncPath archivePath = destPath / (archiveNameWithoutExtension + Str2SyncName(".zip"));
    int err = 0;
    zip_t *archive = zip_open(archivePath.string().c_str(), ZIP_CREATE | ZIP_EXCL, &err);
    if (!archive || err != ZIP_ER_OK) {
        zip_error_t error;
        zip_error_init_with_code(&error, err);
        LOG_WARN(Log::instance()->getLogger(), "Error in zip_open: " << zip_error_strerror(&error));
        zip_error_fini(&error);
        return ExitCode::SystemError;
    }

    IoHelper::DirectoryIterator dir;
    IoError ioError = IoError::Unknown;
    if (!IoHelper::getDirectoryIterator(directoryToCompress, false, ioError, dir)) {
        return ExitCode::SystemError;
    }

    bool endOfDirectory = false;
    DirectoryEntry entry;
    int progress = 10;
    while (dir.next(entry, endOfDirectory, ioError) && !endOfDirectory) {
        if (const ExitInfo exitInfo = notifyLogUploadProgress(LogUploadState::Archiving, std::min(++progress, 90)); !exitInfo) {
            LOG_WARN(Log::instance()->getLogger(), "Error in LogUploadJob::notifyLogUploadProgress: " << exitInfo);
            return exitInfo;
        }

        const std::string entryPath = entry.path().string();
        zip_source_t *source = zip_source_file(archive, entryPath.c_str(), 0, ZIP_LENGTH_TO_END);
        if (source == nullptr) {
            LOG_WARN(Log::instance()->getLogger(), "Error in zip_source_file: " << zip_strerror(archive));
            return ExitCode::SystemError;
        }

        const std::string entryName = entry.path().filename().string();
        if (zip_file_add(archive, entryName.c_str(), source, ZIP_FL_OVERWRITE) < 0) {
            LOG_WARN(Log::instance()->getLogger(), "Error in zip_file_add: " << zip_strerror(archive));
            return ExitCode::SystemError;
        }
    }

    if (!endOfDirectory) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in DirectoryIterator: " << Utility::formatIoError(directoryToCompress, ioError));
        return ExitCode::SystemError;
    }

    if (const ExitInfo exitInfo = notifyLogUploadProgress(LogUploadState::Archiving, 90); !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Error in LogUploadJob::notifyLogUploadProgress: " << exitInfo);
        return exitInfo;
    }

    // Close the archive
    if (zip_close(archive) < 0) {
        LOG_WARN(Log::instance()->getLogger(), "Error in zip_close: " << zip_strerror(archive));
        return ExitCode::SystemError;
    }
    finalPath = archivePath;
    return ExitCode::Ok;
}

ExitInfo LogUploadJob::upload(const SyncPath &archivePath) {
    // Upload archive
    if (const ExitInfo exitInfo = notifyLogUploadProgress(LogUploadState::Uploading, 0); !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Error in LogUploadJob::notifyLogUploadProgress: " << exitInfo);
        return exitInfo;
    }

    std::shared_ptr<LogUploadSession> uploadSessionLog = nullptr;
    try {
        uploadSessionLog = std::make_shared<LogUploadSession>(archivePath, 1);
    } catch (const std::exception &e) {
        LOG_WARN(Log::instance()->getLogger(), "Error in LogUploadSession::LogUploadSession: error=" << e.what());
        return AbstractTokenNetworkJob::exception2ExitCode(e);
    };

    bool canceledByUser = false;
    const std::function<void(UniqueId, int percent)> progressCallbackUploadingWrapper =
            [&uploadSessionLog, &canceledByUser, this](UniqueId, int percent) { // Progress callback
                if (notifyLogUploadProgress(LogUploadState::Uploading, percent).code() == ExitCode::OperationCanceled) {
                    uploadSessionLog->abort();
                    canceledByUser = true;
                }
            };
    uploadSessionLog->setProgressPercentCallback(progressCallbackUploadingWrapper);

    const std::function<void(uint64_t)> uploadSessionLogFinisCallback = [&canceledByUser, this](uint64_t) {
        canceledByUser = notifyLogUploadProgress(LogUploadState::Uploading, 100).code() == ExitCode::OperationCanceled;
    };
    uploadSessionLog->setAdditionalCallback(uploadSessionLogFinisCallback);
    (void) uploadSessionLog->runSynchronously();
    if (canceledByUser) {
        return {ExitCode::OperationCanceled, ExitCause::Unknown};
    }

    if (const ExitInfo jobExitInfo = uploadSessionLog->exitInfo(); !jobExitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Error during log upload: " << jobExitInfo);
        return jobExitInfo;
    }
    return ExitCode::Ok;
}

void LogUploadJob::updateLogUploadState(const LogUploadState newState) const {
    if (bool found = false; !ParmsDb::instance()->updateAppState(AppStateKey::LogUploadState, newState, found) || !found) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateAppState");
    }
}

LogUploadState LogUploadJob::getDbUploadState() const {
    AppStateValue appStateValue = LogUploadState::None;
    if (bool found = false; !ParmsDb::instance()->selectAppState(AppStateKey::LogUploadState, appStateValue, found)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectAppState");
        _addErrorCallback(Error(ERR_ID, ExitCode::DbError, ExitCause::DbAccessError));
        return LogUploadState::None;
    } else if (!found) {
        LOG_WARN(_logger, AppStateKey::LogUploadState << " not found in the database");
        return LogUploadState::None;
    }

    return std::get<LogUploadState>(appStateValue);
}

void LogUploadJob::updateDbUploadState(LogUploadState newState) const {
    bool found = false;
    if (!ParmsDb::instance()->updateAppState(AppStateKey::LogUploadState, newState, found)) {
        LOG_WARN(_logger, "Error in ParmsDb::updateAppState");
        _addErrorCallback(Error(ERR_ID, ExitCode::DbError, ExitCause::DbAccessError));
        return;
    }
    if (!found) {
        LOG_WARN(_logger, AppStateKey::LogUploadState << " not found in the database");
        return;
    }
}

ExitInfo LogUploadJob::notifyLogUploadProgress(const LogUploadState newState, const int progressPercent) {
    const LogUploadState logUploadState = getDbUploadState();
    const bool canceled = logUploadState == LogUploadState::Canceled || logUploadState == LogUploadState::CancelRequested ||
                          newState == LogUploadState::Canceled || newState == LogUploadState::CancelRequested;

    if (newState != _previousState || progressPercent != _previousProgress) {
        LOG_DEBUG(_logger, "Log transfert progress: " << newState << " | " << progressPercent << " %");
        if (_progressCallback && (_lastProgressUpdateTimeStamp + std::chrono::seconds(1) < std::chrono::system_clock::now() ||
                                  newState != _previousState || progressPercent - _previousProgress > 10)) {
            _lastProgressUpdateTimeStamp = std::chrono::system_clock::now();
            _previousProgress = progressPercent;
            _previousState = newState;
            _progressCallback(newState, progressPercent);
        }
    }

    return canceled ? ExitCode::OperationCanceled : ExitCode::Ok;
}

void LogUploadJob::handleJobFailure(const ExitInfo &exitInfo, const bool clearTmpDir) {
    const LogUploadState logUploadState =
            exitInfo.code() == ExitCode::OperationCanceled ? LogUploadState::Canceled : LogUploadState::Failed;
    updateLogUploadState(logUploadState);
    (void) notifyLogUploadProgress(logUploadState, 0);
    if (clearTmpDir || exitInfo.code() == ExitCode::OperationCanceled) {
        IoError ioError = IoError::Success;
        (void) IoHelper::deleteItem(_tmpJobWorkingDir, ioError);
        if (ioError != IoError::Success) {
            LOGW_INFO(Log::instance()->getLogger(),
                      L"Error in IoHelper::deleteItem: " << Utility::formatIoError(_tmpJobWorkingDir, ioError));
        }
    }

    IoError ioError = IoError::Success;
    IoHelper::deleteItem(_tmpJobWorkingDir, ioError);
    if (ioError != IoError::Success) {
        LOGW_INFO(Log::instance()->getLogger(),
                  L"Error in IoHelper::deleteItem: " << Utility::formatIoError(_tmpJobWorkingDir, ioError));
    }

    if (exitInfo.code() != ExitCode::NetworkError) {
        sentry::Handler::captureMessage(sentry::Level::Error, "Log upload failed",
                                        "Log upload failed with exitInfo: " + exitInfo.operator std::string());
    }
    _runningJob.reset();
}

bool LogUploadJob::getFileSize(const SyncPath &path, uint64_t &size) {
    size = 0;

    IoError ioError = IoError::Unknown;
    if (!IoHelper::getFileSize(path, size, ioError)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in IoHelper::getFileSize for " << Utility::formatIoError(path, ioError));
        return false;
    }
    if (ioError != IoError::Success) {
        LOGW_WARN(Log::instance()->getLogger(), L"Unable to read file size for " << Utility::formatIoError(path, ioError));
        return false;
    }

    return true;
}

} // namespace KDC
