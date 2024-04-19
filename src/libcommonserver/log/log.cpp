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

#include "log.h"
#include "customrollingfileappender.h"
#include "utility/utility.h"
#include "libparms/db/user.h"
#include "libparms/db/parmsdb.h"
#include "libparms/db/drive.h"

#include "libcommonserver/io/iohelper.h"
#include "libcommon/utility/utility.h"

#include <log4cplus/initializer.h>
#include <log4cplus/fileappender.h>
#include <log4cplus/loggingmacros.h>

#include <codecvt>

#define LOGGER_INSTANCE_NAME L"Main"
#define LOGGER_APP_RF_NAME L"RollingFileAppender"
#define LOGGER_APP_RF_PATTERN L"%D{%Y-%m-%d %H:%M:%S:%q} [%-0.-1p] (%t) %b:%L - %m%n"
#define LOGGER_APP_RF_MAX_FILE_SIZE 500  // MB
#define LOGGER_APP_RF_MAX_BACKUP_IDX 4
#define LOGGER_APP_PURGE_AFTER_HOURS 24

namespace KDC {

std::shared_ptr<Log> Log::_instance = nullptr;

Log::~Log() {
    log4cplus::Logger::shutdown();
}

std::shared_ptr<Log> Log::instance(const log4cplus::tstring &filePath) {
    if (_instance == nullptr) {
        if (filePath.empty()) {
            throw std::runtime_error("Log must be initialized!");
        } else {
            _instance = std::shared_ptr<Log>(new Log(filePath));
        }
    }

    return _instance;
}

bool Log::configure(bool useLog, LogLevel logLevel, bool purgeOldLogs) {
    if (useLog) {
        // Set log level
        switch (logLevel) {
            case LogLevelDebug:
                _logger.setLogLevel(log4cplus::DEBUG_LOG_LEVEL);
                break;
            case LogLevelInfo:
                _logger.setLogLevel(log4cplus::INFO_LOG_LEVEL);
                break;
            case LogLevelWarning:
                _logger.setLogLevel(log4cplus::WARN_LOG_LEVEL);
                break;
            case LogLevelError:
                _logger.setLogLevel(log4cplus::ERROR_LOG_LEVEL);
                break;
            case LogLevelFatal:
                _logger.setLogLevel(log4cplus::FATAL_LOG_LEVEL);
                break;
        }
    } else {
        _logger.setLogLevel(log4cplus::OFF_LOG_LEVEL);
    }

    // Set purge duration
    log4cplus::SharedAppenderPtr rfAppenderPtr = _logger.getAppender(LOGGER_APP_RF_NAME);
    static_cast<CustomRollingFileAppender *>(rfAppenderPtr.get())->setExpire(purgeOldLogs ? LOGGER_APP_PURGE_AFTER_HOURS : 0);

    return true;
}

Log::Log(const log4cplus::tstring &filePath) {
    // Instantiate an appender object
    CustomRollingFileAppender *rfAppender = new CustomRollingFileAppender(filePath, LOGGER_APP_RF_MAX_FILE_SIZE * 1024 * 1024,
                                                                          LOGGER_APP_RF_MAX_BACKUP_IDX, true, true);

    // Unicode management
    std::locale loc(std::locale(), new std::codecvt_utf8<wchar_t>);
    rfAppender->imbue(loc);

    log4cplus::SharedAppenderPtr appender(std::move(rfAppender));
    appender->setName(LOGGER_APP_RF_NAME);

    // Instantiate a layout object && attach the layout object to the appender
    appender->setLayout(std::unique_ptr<log4cplus::Layout>(new log4cplus::PatternLayout(LOGGER_APP_RF_PATTERN)));

    // Instantiate a logger object
    _logger = log4cplus::Logger::getInstance(LOGGER_INSTANCE_NAME);
    _logger.setLogLevel(log4cplus::TRACE_LOG_LEVEL);

    // Attach the appender object to the logger
    _logger.addAppender(appender);

    LOG_INFO(_logger, "Logger initialization done");
}

int Log::getLogEstimatedSize(IoError &ioError) {
    SyncPath logPath;

    IoHelper::logDirectoryPath(logPath, ioError);
    if (ioError != IoErrorSuccess) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in IoHelper::logDirectoryPath : " << Utility::formatIoError(logPath, ioError).c_str());
        return -1;
    }

    DirectoryIterator dir;
    IoHelper::getDirectoryIterator(logPath, false, ioError, dir);
    if (ioError != IoErrorSuccess) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in DirectoryIterator : " << Utility::formatIoError(logPath, ioError).c_str());
        return -1;
    }

    // Estimate the size of the log files
    int size = 0;
    DirectoryEntry entry;
    bool retried = true;
    ioError = IoErrorSuccess;

    for (int i = 0; i < 2; i++) {  // Retry once in case a log file is archived/created during the first iteration
        size = 0;
        while (dir.next(entry, ioError)) {
            size += entry.file_size();
        }

        if (ioError == IoErrorEndOfDirectory) {
            break;
        } else {
            LOG_WARN(Log::instance()->getLogger(),
                     "Error in DirectoryIterator : " << Utility::formatIoError(logPath, ioError).c_str() << " retrying once.");
        }
    }

    if (ioError != IoErrorEndOfDirectory) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in DirectoryIterator : " << Utility::formatIoError(logPath, ioError).c_str());
        return -1;
    }
    return size;
}

ExitCode Log::generateLogsSupportArchive(bool includeOldLogs, const SyncPath &outputPath, const SyncPath &archiveName,
                                         ExitCause &exitCause, std::function<void(int64_t)> progressCallback) {
    // Get the log directory path
    SyncPath logPath;
    exitCause = ExitCauseUnknown;
    IoError ioError;
    if (!IoHelper::logDirectoryPath(logPath, ioError)) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in IoHelper::logDirectoryPath : " << Utility::formatIoError(logPath, ioError).c_str());
        exitCause = ExitCauseUnknown;
        return ExitCodeSystemError;
    }

    SyncPath tempDirectory =
        logPath / "send_log_directory_temp" / ("tempLogArchive_" + CommonUtility::generateRandomStringAlphaNum(10));

    // Create temp folder

    if (!IoHelper::createDirectory(tempDirectory.parent_path(), ioError) && ioError != IoErrorDirectoryExists) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in IoHelper::createDirectory : " << Utility::formatIoError(tempDirectory.parent_path(), ioError).c_str());
        if (ioError == IoErrorDiskFull) {
            exitCause = ExitCauseNotEnoughDiskSpace;
        } else {
            exitCause = ExitCauseUnknown;
        }
        return ExitCodeSystemError;
    }

    if (!IoHelper::createDirectory(tempDirectory, ioError)) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in IoHelper::createDirectory : " << Utility::formatIoError(tempDirectory, ioError).c_str());
        IoError ignoreError;
        IoHelper::deleteDirectory(tempDirectory.parent_path(), ignoreError);
        if (ioError == IoErrorDiskFull) {
            exitCause = ExitCauseNotEnoughDiskSpace;
        } else {
            exitCause = ExitCauseUnknown;
        }
        return ExitCodeSystemError;
    }

    ExitCode exitCode = ExitCodeUnknown;
    ExitCause exitCause = ExitCauseUnknown;
    // Copy log files to temp folder
    exitCode = copyLogsTo(tempDirectory, includeOldLogs, exitCause);
    if (exitCode != ExitCodeOk) {
        LOG_WARN(Log::instance()->getLogger(), "Unable to copy logs to temp folder : " << exitCause);
        IoHelper::deleteDirectory(tempDirectory.parent_path(), ioError);
        return exitCode;
    }

    // Copy .parmsdb to temp folder
    exitCode = copyParmsDbTo(tempDirectory / ".parms.db", exitCause);
    if (exitCode != ExitCodeOk) {
        LOG_WARN(Log::instance()->getLogger(), "Unable to copy .parms.db to temp folder : " << exitCause);
        IoHelper::deleteDirectory(tempDirectory.parent_path(), ioError);
        return exitCode;
    }

    // Generate user description file
    exitCode = generateUserDescriptionFile(tempDirectory, exitCause);
    if (exitCode != ExitCodeOk) {
        LOG_WARN(Log::instance()->getLogger(), "Unable to generate user description file : " << exitCause);
        IoHelper::deleteDirectory(tempDirectory.parent_path(), ioError);
        return exitCode;
    }

    // compress all the files in the folder
    exitCode = compressLogs(tempDirectory, exitCause, progressCallback);
    if (exitCode != ExitCodeOk) {
        LOG_WARN(Log::instance()->getLogger(), "Unable to compress logs : " << exitCause);
        IoHelper::deleteDirectory(tempDirectory.parent_path(), ioError);
        return exitCode;
    }

    // Generate the archive
    int err = 0;
    zip_t *archive = zip_open((logPath / "send_log_directory_temp" / archiveName).string().c_str(), ZIP_CREATE | ZIP_EXCL, &err);
    if (err != ZIP_ER_OK) {
        LOG_WARN(Log::instance()->getLogger(), "Error in zip_open : " << zip_strerror(archive));
        IoHelper::deleteDirectory(tempDirectory.parent_path(), ioError);

        exitCause = ExitCauseUnknown;
        return ExitCodeUnknown;
    }

    DirectoryIterator dir;
    DirectoryEntry entry;
    if (!IoHelper::getDirectoryIterator(tempDirectory, false, ioError, dir)) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in DirectoryIterator : " << Utility::formatIoError(tempDirectory, ioError).c_str());
        IoHelper::deleteDirectory(tempDirectory.parent_path(), ioError);
        
        exitCause = ExitCauseUnknown;
        return ExitCodeUnknown;
    }

    while (dir.next(entry, ioError)) {
        std::string entryPath = entry.path().string();
        zip_source_t *source = zip_source_file(archive, entryPath.c_str(), 0, ZIP_LENGTH_TO_END);
        if (source == nullptr) {
            LOG_WARN(Log::instance()->getLogger(), "Error in zip_source_file : " << zip_strerror(archive));
            IoHelper::deleteDirectory(tempDirectory.parent_path(), ioError);

            exitCause = ExitCauseUnknown;
            return ExitCodeUnknown;
        }

        std::string entryName = entry.path().filename().string();
        if (zip_file_add(archive, entryName.c_str(), source, ZIP_FL_OVERWRITE) < 0) {
            LOG_WARN(Log::instance()->getLogger(), "Error in zip_file_add : " << zip_strerror(archive));
            IoHelper::deleteDirectory(tempDirectory.parent_path(), ioError);

            exitCause = ExitCauseUnknown;
            return ExitCodeUnknown;
        }
    }

    if (ioError != IoErrorEndOfDirectory) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in DirectoryIterator : " << Utility::formatIoError(tempDirectory, ioError).c_str());
        IoHelper::deleteDirectory(tempDirectory.parent_path(), ioError);

        exitCause = ExitCauseUnknown;
        return ExitCodeUnknown;
    }

    // Close the archive
    if (zip_close(archive) < 0) {
        LOG_WARN(Log::instance()->getLogger(), "Error in zip_close : " << zip_strerror(archive));
        IoHelper::deleteDirectory(tempDirectory.parent_path(), ioError);

        exitCause = ExitCauseUnknown;
        return ExitCodeUnknown;
    }

    // Copy the archive to the output path
    if (!IoHelper::copyFileOrDirectory(logPath / "send_log_directory_temp" / archiveName, outputPath / archiveName, ioError)) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in IoHelper::copyFileOrDirectory : "
                     << Utility::formatIoError(logPath / "send_log_directory_temp" / archiveName, ioError).c_str());
        IoHelper::deleteDirectory(tempDirectory.parent_path(), ioError);

        exitCause = ExitCauseUnknown;
        return ExitCodeUnknown;
    }

    // Delete the temp folder
    if (!IoHelper::deleteDirectory(tempDirectory.parent_path(), ioError)) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in IoHelper::deleteDirectory : " << Utility::formatIoError(tempDirectory.parent_path(), ioError).c_str());
    }

    exitCause = ExitCauseUnknown;
    return ExitCodeOk;
}

ExitCode Log::copyParmsDbTo(const SyncPath &outputPath, ExitCause &exitCause) {
    const SyncPath parmsDbName = ".parms.db";
    const SyncPath parmsDbPath = CommonUtility::getAppSupportDir() / parmsDbName;
    DirectoryEntry entryParmsDb;
    IoError ioError = IoErrorUnknown;
    exitCause = ExitCauseUnknown;

    if (!IoHelper::getDirectoryEntry(parmsDbPath, ioError, entryParmsDb)) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in IoHelper::getDirectoryEntry : " << Utility::formatIoError(parmsDbPath, ioError).c_str());
        if (ioError == IoErrorNoSuchFileOrDirectory) {
            exitCause = ExitCauseFileAccessError;
        } else {
            exitCause = ExitCauseUnknown;
        }
        return ExitCodeSystemError;
    }

    if (!IoHelper::copyFileOrDirectory(parmsDbPath, outputPath, ioError)) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in IoHelper::copyFileOrDirectory : " << Utility::formatIoError(parmsDbPath, ioError).c_str());

        if (ioError == IoErrorDiskFull) {
            exitCause = ExitCauseNotEnoughDiskSpace;
        } else {
            exitCause = ExitCauseUnknown;
        }
        return ExitCodeSystemError;
    }
    exitCause = ExitCauseUnknown;
    return ExitCodeOk;
}

ExitCode Log::copyLogsTo(const SyncPath &outputPath, bool includeOldLogs, ExitCause &exitCause) {
    SyncPath logPath;
    IoError ioError = IoErrorUnknown;

    if (!IoHelper::logDirectoryPath(logPath, ioError)) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in IoHelper::logDirectoryPath : " << Utility::formatIoError(logPath, ioError).c_str());
        exitCause = ExitCauseUnknown;
        return ExitCodeSystemError;
    }

    DirectoryIterator dir;
    if (!IoHelper::getDirectoryIterator(logPath, false, ioError, dir)) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in DirectoryIterator : " << Utility::formatIoError(logPath, ioError).c_str());
        exitCause = ExitCauseUnknown;
        return ExitCodeSystemError;
    }

    DirectoryEntry entry;
    while (dir.next(entry, ioError)) {
        if (entry.is_directory()) {
            LOG_WARN(Log::instance()->getLogger(), "Ignoring temp directory " << entry.path().filename().string().c_str());
            continue;
        }

        if (!includeOldLogs && entry.path().filename().string().find(".gz") != std::string::npos) {
            LOG_WARN(Log::instance()->getLogger(), "Ignoring old log file " << entry.path().filename().string().c_str());
            continue;
        }

        if (!IoHelper::copyFileOrDirectory(entry.path(), outputPath / entry.path().filename(), ioError)) {
            LOG_WARN(Log::instance()->getLogger(),
                     "Error in IoHelper::copyFileOrDirectory : " << Utility::formatIoError(entry.path(), ioError).c_str());
            if (ioError == IoErrorDiskFull) {
                exitCause = ExitCauseNotEnoughDiskSpace;
                return ExitCodeSystemError;
            }
        }
    }

    if (ioError != IoErrorEndOfDirectory) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in DirectoryIterator : " << Utility::formatIoError(logPath, ioError).c_str());
        exitCause = ExitCauseUnknown;
        return ExitCodeSystemError;
    }

    ioError = IoErrorSuccess;
    exitCause = ExitCauseUnknown;
    return ExitCodeOk;
}

ExitCode Log::compressLogs(const SyncPath &directoryToCompress, ExitCause &exitCause,
                           std::function<void(int64_t)> progressCallback) {
    DirectoryIterator dir;
    IoError ioError = IoErrorUnknown;
    if (!IoHelper::getDirectoryIterator(directoryToCompress, false, ioError, dir)) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in DirectoryIterator : " << Utility::formatIoError(directoryToCompress, ioError).c_str());
        exitCause = ExitCauseUnknown;
        return ExitCodeSystemError;
    }

    bool progressMonitoring = progressCallback != nullptr;
    float nbFiles = 0;
    DirectoryEntry entry;

    if (progressMonitoring) {
        while (dir.next(entry, ioError)) {
            nbFiles++;
        }
        if (!IoHelper::getDirectoryIterator(directoryToCompress, false, ioError, dir)) {
            LOG_WARN(Log::instance()->getLogger(),
                     "Error in DirectoryIterator : " << Utility::formatIoError(directoryToCompress, ioError).c_str());
            exitCause = ExitCauseUnknown;
            return ExitCodeSystemError;
        }
    }

    float progress = 0.0;
    while (dir.next(entry, ioError)) {
        std::string entryPath = entry.path().string();
        if (entryPath.find(".gz") != std::string::npos) {
            continue;
        }
        QString destPath = QString::fromStdString(entryPath + ".gz");
        if (!CommonUtility::compressFile(QString::fromStdString(entryPath), destPath)) {
            LOG_WARN(Log::instance()->getLogger(),
                     "Error in compressFile for " << entryPath.c_str() << " to " << destPath.toStdString().c_str());

            exitCause = ExitCauseUnknown;
            return ExitCodeSystemError;
        }

        if (!IoHelper::deleteDirectory(entry.path(), ioError)) {
            LOG_WARN(Log::instance()->getLogger(),
                     "Error in IoHelper::deleteDirectory : " << Utility::formatIoError(entry.path(), ioError).c_str());
            exitCause = ExitCauseUnknown;
            return ExitCodeSystemError;
        }
        if (progressMonitoring) {
            progress++;
            int64_t progressPercent = 100.0 * progress / nbFiles;
            progressCallback(progressPercent);
        }
    }

    if (ioError != IoErrorEndOfDirectory) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in DirectoryIterator : " << Utility::formatIoError(directoryToCompress, ioError).c_str());
        exitCause = ExitCauseUnknown;
        return ExitCodeSystemError;
    }

    return ExitCodeOk;
}

ExitCode Log::generateUserDescriptionFile(const SyncPath &outputPath, ExitCause &exitCause) {
    SyncPath filePath = outputPath / "user_description.txt";

    std::string osName;
    std::string osArch;
    std::string appVersion;
    std::string userId;

    osName = CommonUtility::platformName().toStdString();
    osArch = CommonUtility::platformArch().toStdString();
    appVersion = CommonUtility::userAgentString();

    IoError ioError = IoErrorUnknown;
    std::ofstream file(filePath.string());
    if (!file.is_open()) {
        LOG_WARN(Log::instance()->getLogger(), "Error in creating file : " << Utility::formatIoError(filePath, ioError).c_str());
        ioError = IoErrorUnknown;
        return ExitCodeOk;
    }
    file << "OS Name: " << osName << std::endl;
    file << "OS Architecture: " << osArch << std::endl;
    file << "App Version: " << appVersion << std::endl;
    file << "User ID(s): ";

    std::vector<User> userList;
    if (ParmsDb::instance()->selectAllUsers(userList)) {
        for (const User &user : userList) {
            file << user.userId() << " | ";
        }
        file << std::endl;
    } else {
        file << "Unable to retrieve user ID(s)" << std::endl;
        LOG_WARN(_logger, "Error in ParmsDb::selectAllUsers");
    }

    file << "Drive ID(s): ";
    std::vector<Drive> driveList;
    if (ParmsDb::instance()->selectAllDrives(driveList)) {
        for (const Drive &drive : driveList) {
            file << drive.driveId() << " | ";
        }
        file << std::endl;
    } else {
        file << "Unable to retrieve drive ID(s)" << std::endl;
        LOG_WARN(_logger, "Error in ParmsDb::selectAllUsers");
    }


    file.close();
    if (file.bad()) {
        exitCause = ExitCauseNotEnoughDiskSpace;
        return ExitCodeSystemError;
    }

    return ExitCodeOk;
}


}  // namespace KDC
