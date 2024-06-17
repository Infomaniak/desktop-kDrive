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

#include "logarchiver.h"

#include "libcommon/utility/types.h"
#include "libcommon/utility/utility.h"

#include "libcommonserver/commonserverlib.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/log/log.h"

#include "libparms/db/user.h"
#include "libparms/db/parmsdb.h"
#include "libparms/db/drive.h"

#include <fstream>

#include <zip.h>

namespace KDC {

bool LogArchiver::getLogDirEstimatedSize(uint64_t &size, IoError &ioError) {
    const SyncPath logPath = Log::instance()->getLogFilePath().parent_path();
    bool result = false;
    for (int i = 0; i < 2; i++) {  // Retry once in case a log file is archived/created during the first iteration
        result = IoHelper::getDirectorySize(logPath, size, ioError);
        if (ioError == IoErrorSuccess) {
            return true;
        }
    }
    LOGW_WARN(Log::instance()->getLogger(),
              L"Error in LogArchiver::getLogDirEstimatedSize: " << Utility::formatIoError(logPath, ioError).c_str());

    return result;
}

ExitCode LogArchiver::generateLogsSupportArchive(bool includeArchivedLogs, const SyncPath &outputPath,
                                                 std::function<bool(int)> progressCallback, SyncPath &archivePath,
                                                 ExitCause &exitCause, bool test) {
    // Get the log directory path
    const SyncPath logPath = Log::instance()->getLogFilePath().parent_path();
    const SyncPath tempLogArchiveDir =
        logPath / "temp_support_archive_generator" / ("tempLogArchive_" + CommonUtility::generateRandomStringAlphaNum(10));
    exitCause = ExitCause::Unknown;
    IoError ioError = IoErrorSuccess;

    // Generate archive name: <drive id 1>-<drive id 2>...-<drive id N>-yyyyMMdd-HHmmss.zip
    std::string archiveName;
    if (!test) {
        std::vector<Drive> driveList;
        try {
            if (!ParmsDb::instance()->selectAllDrives(driveList)) {
                LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllDrives");
                if (bool found = false;
                    !ParmsDb::instance()->updateAppState(AppStateKey::LogUploadState, LogUploadState::Failed, found) || !found) {
                    LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateAppState");
                }
                exitCause = ExitCause::DbAccessError;
                return ExitCode::DbError;
            }

            if (driveList.empty()) {
                LOG_WARN(Log::instance()->getLogger(), "No drive found - Unable to send log");
                if (bool found = false;
                    !ParmsDb::instance()->updateAppState(AppStateKey::LogUploadState, LogUploadState::Failed, found) || !found) {
                    LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateAppState");
                }
                exitCause = ExitCause::LoginError;
                return ExitCode::InvalidToken;  // Currently, we can't send logs if no drive is found
            }
        } catch (const std::runtime_error &e) {
            LOG_WARN(Log::instance()->getLogger(), "Error in generateLogsSupportArchive: " << e.what());
            exitCause = ExitCause::DbAccessError;
            return ExitCode::DbError;
        }

        for (const auto &drive : driveList) {
            archiveName += std::to_string(drive.driveId()) + "-";
        }

        const std::time_t now = std::time(nullptr);
        const std::tm tm = *std::localtime(&now);
        std::ostringstream woss;
        woss << std::put_time(&tm, "%Y%m%d_%H%M%S");
        archiveName += woss.str() + ".zip";
    } else {
        archiveName = "test.zip";
    }
    archivePath = outputPath / archiveName;

    // Create temp folder
    if (!IoHelper::createDirectory(tempLogArchiveDir.parent_path(), ioError) && ioError != IoErrorDirectoryExists) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in IoHelper::createDirectory: "
                                                    << Utility::formatIoError(tempLogArchiveDir.parent_path(), ioError).c_str());
        if (ioError == IoErrorDiskFull) {
            exitCause = ExitCause::NotEnoughDiskSpace;
        } else {
            exitCause = ExitCause::Unknown;
        }
        return ExitCode::SystemError;
    }

    if (!IoHelper::createDirectory(tempLogArchiveDir, ioError)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in IoHelper::createDirectory: " << Utility::formatIoError(tempLogArchiveDir, ioError).c_str());
        IoError ignoreError = IoErrorSuccess;
        IoHelper::deleteDirectory(tempLogArchiveDir.parent_path(), ignoreError);
        if (ioError == IoErrorDiskFull) {
            exitCause = ExitCause::NotEnoughDiskSpace;
        } else {
            exitCause = ExitCause::Unknown;
        }
        return ExitCode::SystemError;
    }

    ExitCode exitCode = copyLogsTo(tempLogArchiveDir, includeArchivedLogs, exitCause);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Unable to copy logs to temp folder: " << enumClassToInt(exitCause));
        IoHelper::deleteDirectory(tempLogArchiveDir.parent_path(), ioError);
        return exitCode;
    }

    // Copy .parmsdb to temp folder
    exitCode = copyParmsDbTo(tempLogArchiveDir / ".parms.db", exitCause);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Unable to copy .parms.db to temp folder: " << enumClassToInt(exitCause));
        IoHelper::deleteDirectory(tempLogArchiveDir.parent_path(), ioError);
        return exitCode;
    }

    // Generate user description file
    exitCode = generateUserDescriptionFile(tempLogArchiveDir, exitCause);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Unable to generate user description file: " << enumClassToInt(exitCause));
        IoHelper::deleteDirectory(tempLogArchiveDir.parent_path(), ioError);
        return exitCode;
    }

    // compress all the files in the folder
    exitCode = compressLogFiles(tempLogArchiveDir, progressCallback, exitCause);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Unable to compress logs: " << enumClassToInt(exitCause));
        IoHelper::deleteDirectory(tempLogArchiveDir.parent_path(), ioError);
        return exitCode;
    }

    // Generate the archive
    int err = 0;
    zip_t *archive = zip_open(archivePath.string().c_str(), ZIP_CREATE | ZIP_EXCL, &err);
    if (err != ZIP_ER_OK) {
        LOG_WARN(Log::instance()->getLogger(), "Error in zip_open: " << zip_strerror(archive));
        IoHelper::deleteDirectory(tempLogArchiveDir.parent_path(), ioError);

        exitCause = ExitCause::Unknown;
        return ExitCode::SystemError;
    }

    IoHelper::DirectoryIterator dir;
    if (!IoHelper::getDirectoryIterator(tempLogArchiveDir, false, ioError, dir)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in DirectoryIterator: " << Utility::formatIoError(tempLogArchiveDir, ioError).c_str());
        IoHelper::deleteDirectory(tempLogArchiveDir.parent_path(), ioError);

        exitCause = ExitCause::Unknown;
        return ExitCode::Unknown;
    }

    bool endOfDirectory = false;
    DirectoryEntry entry;
    while (dir.next(entry, endOfDirectory, ioError) && !endOfDirectory) {
        const std::string entryPath = entry.path().string();
        zip_source_t *source = zip_source_file(archive, entryPath.c_str(), 0, ZIP_LENGTH_TO_END);
        if (source == nullptr) {
            LOG_WARN(Log::instance()->getLogger(), "Error in zip_source_file: " << zip_strerror(archive));
            IoHelper::deleteDirectory(tempLogArchiveDir.parent_path(), ioError);

            exitCause = ExitCause::Unknown;
            return ExitCode::Unknown;
        }

        const std::string entryName = entry.path().filename().string();
        if (zip_file_add(archive, entryName.c_str(), source, ZIP_FL_OVERWRITE) < 0) {
            LOG_WARN(Log::instance()->getLogger(), "Error in zip_file_add: " << zip_strerror(archive));
            IoHelper::deleteDirectory(tempLogArchiveDir.parent_path(), ioError);

            exitCause = ExitCause::Unknown;
            return ExitCode::Unknown;
        }
    }

    if (!endOfDirectory) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in DirectoryIterator: " << Utility::formatIoError(tempLogArchiveDir, ioError).c_str());
        IoHelper::deleteDirectory(tempLogArchiveDir.parent_path(), ioError);

        exitCause = ExitCause::Unknown;
        return ExitCode::Unknown;
    }

    // Close the archive
    if (zip_close(archive) < 0) {
        LOG_WARN(Log::instance()->getLogger(), "Error in zip_close: " << zip_strerror(archive));
        IoHelper::deleteDirectory(tempLogArchiveDir.parent_path(), ioError);

        exitCause = ExitCause::Unknown;
        return ExitCode::Unknown;
    }

    // Delete the temp folder
    if (!IoHelper::deleteDirectory(tempLogArchiveDir.parent_path(), ioError)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in IoHelper::deleteDirectory: "
                                                    << Utility::formatIoError(tempLogArchiveDir.parent_path(), ioError).c_str());
    }

    exitCause = ExitCause::Unknown;
    return ExitCode::Ok;
}

ExitCode LogArchiver::copyLogsTo(const SyncPath &outputPath, bool includeArchivedLogs, ExitCause &exitCause) {
    exitCause = ExitCause::Unknown;
    SyncPath logPath = Log::instance()->getLogFilePath().parent_path();

    IoError ioError = IoErrorSuccess;
    IoHelper::DirectoryIterator dir;
    if (!IoHelper::getDirectoryIterator(logPath, false, ioError, dir)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in DirectoryIterator: " << Utility::formatIoError(logPath, ioError).c_str());
        return ExitCode::SystemError;
    }

    DirectoryEntry entry;
    bool endOfDirectory = false;
    while (dir.next(entry, endOfDirectory, ioError) && !endOfDirectory) {
        if (entry.is_directory()) {
            LOG_INFO(Log::instance()->getLogger(), "Ignoring temp directory " << entry.path().filename().string().c_str());
            continue;
        }

        if (!includeArchivedLogs && entry.path().filename().extension() == Str(".gz")) {
            LOG_INFO(Log::instance()->getLogger(), "Ignoring old log file " << entry.path().filename().string().c_str());
            continue;
        }

        if (!IoHelper::copyFileOrDirectory(entry.path(), outputPath / entry.path().filename(), ioError)) {
            LOGW_WARN(Log::instance()->getLogger(),
                      L"Error in IoHelper::copyFileOrDirectory: " << Utility::formatIoError(entry.path(), ioError).c_str());
            if (ioError == IoErrorDiskFull) {
                exitCause = ExitCause::NotEnoughDiskSpace;
            }
            return ExitCode::SystemError;
        }
    }

    if (!endOfDirectory) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in DirectoryIterator: " << Utility::formatIoError(logPath, ioError).c_str());
        return ExitCode::SystemError;
    }

    return ExitCode::Ok;
}

ExitCode LogArchiver::copyParmsDbTo(const SyncPath &outputPath, ExitCause &exitCause) {
    const SyncPath parmsDbName = ".parms.db";
    const SyncPath parmsDbPath = CommonUtility::getAppSupportDir() / parmsDbName;
    DirectoryEntry entryParmsDb;
    IoError ioError = IoErrorUnknown;
    exitCause = ExitCause::Unknown;

    if (!IoHelper::getDirectoryEntry(parmsDbPath, ioError, entryParmsDb)) {
        LOGW_DEBUG(KDC::Log::instance()->getLogger(), L"Creating ");

        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in IoHelper::getDirectoryEntry: " << Utility::formatIoError(parmsDbPath, ioError).c_str());
        if (ioError == IoErrorNoSuchFileOrDirectory) {
            exitCause = ExitCause::FileAccessError;
        }
        return ExitCode::SystemError;
    }

    if (!IoHelper::copyFileOrDirectory(parmsDbPath, outputPath, ioError)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in IoHelper::copyFileOrDirectory: " << Utility::formatIoError(parmsDbPath, ioError).c_str());

        if (ioError == IoErrorDiskFull) {
            exitCause = ExitCause::NotEnoughDiskSpace;
        }
        return ExitCode::SystemError;
    }
    return ExitCode::Ok;
}

ExitCode LogArchiver::compressLogFiles(const SyncPath &directoryToCompress, std::function<bool(int)> progressCallback,
                                       ExitCause &exitCause) {
    IoHelper::DirectoryIterator dir;
    IoError ioError = IoErrorUnknown;
    exitCause = ExitCause::Unknown;

    if (!IoHelper::getDirectoryIterator(directoryToCompress, true, ioError, dir)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in DirectoryIterator: " << Utility::formatIoError(directoryToCompress, ioError).c_str());
        return ExitCode::SystemError;
    }

    std::function<bool(int)> safeProgressCallback = progressCallback != nullptr ? progressCallback : [](int) { return true; };
    int nbFiles = 0;
    DirectoryEntry entry;

    if (!safeProgressCallback(0)) {
        LOG_INFO(Log::instance()->getLogger(), "Log compression canceled");
        return ExitCode::OperationCanceled;
    }

    bool endOfDirectory = false;
    while (dir.next(entry, endOfDirectory, ioError) && !endOfDirectory) {
        nbFiles++;
    }
    if (!IoHelper::getDirectoryIterator(directoryToCompress, true, ioError, dir)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in DirectoryIterator: " << Utility::formatIoError(directoryToCompress, ioError).c_str());
        return ExitCode::SystemError;
    }


    int progress = 0;
    while (dir.next(entry, endOfDirectory, ioError) && !endOfDirectory) {
        if (entry.path().filename().extension() == Str(".gz")) {
            continue;
        }

        ItemType itemType;
        const bool success = IoHelper::getItemType(entry.path(), itemType);
        ioError = itemType.ioError;
        if (!success) {
            return ExitCode::SystemError;
        }

        if (itemType.nodeType != NodeType::File) {
            continue;
        }
        const std::string entryPathStr = entry.path().string();
        QString destPath = QString::fromStdString(entryPathStr + ".gz");
        if (!CommonUtility::compressFile(QString::fromStdString(entryPathStr), destPath)) {
            LOG_WARN(Log::instance()->getLogger(),
                     "Error in compressFile for " << entryPathStr.c_str() << " to " << destPath.toStdString().c_str());
            return ExitCode::SystemError;
        }

        if (!IoHelper::deleteDirectory(entry.path(), ioError)) {  // Delete the original file
            LOGW_WARN(Log::instance()->getLogger(),
                      L"Error in IoHelper::deleteDirectory: " << Utility::formatIoError(entry.path(), ioError).c_str());
            return ExitCode::SystemError;
        }

        progress++;
        const int progressPercent = 100.0 * (double)progress / (double)nbFiles;
        if (!safeProgressCallback(progressPercent)) {
            LOG_INFO(Log::instance()->getLogger(), "Log compression canceled");
            return ExitCode::OperationCanceled;
        }
    }

    if (!endOfDirectory) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in DirectoryIterator: " << Utility::formatIoError(directoryToCompress, ioError).c_str());
        return ExitCode::SystemError;
    }

    return ExitCode::Ok;
}

ExitCode LogArchiver::generateUserDescriptionFile(const SyncPath &outputPath, ExitCause &exitCause) {
    exitCause = ExitCause::Unknown;

    std::string osName = CommonUtility::platformName().toStdString();
    std::string osArch = CommonUtility::platformArch().toStdString();
    std::string appVersion = CommonUtility::userAgentString();

    std::ofstream file(outputPath.string());
    if (!file.is_open()) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in creating file: " << Utility::formatSyncPath(outputPath).c_str());
        return ExitCode::Ok;
    }
    file << "OS Name: " << osName << std::endl;
    file << "OS Architecture: " << osArch << std::endl;
    file << "App Version: " << appVersion << std::endl;
    file << "User ID(s): ";

    std::vector<User> userList;
    try {
        if (ParmsDb::instance()->selectAllUsers(userList)) {
            for (const User &user : userList) {
                file << user.userId() << " | ";
            }
            file << std::endl;
        } else {
            file << "Unable to retrieve user ID(s)" << std::endl;
            LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllUsers");
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
            LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllUsers");
        }
    } catch (const std::runtime_error &e) {
        LOG_WARN(Log::instance()->getLogger(), "Error in generateUserDescriptionFile: " << e.what());
        file << "Unable to retrieve user ID(s) - " << e.what() << std::endl;
        file << "Drive ID(s): Unable to retrieve drive ID(s) - " << e.what() << std::endl;
    }

    file.close();
    if (file.bad()) {
        exitCause = ExitCause::NotEnoughDiskSpace;
        return ExitCode::SystemError;
    }

    return ExitCode::Ok;
}
};  // namespace KDC
