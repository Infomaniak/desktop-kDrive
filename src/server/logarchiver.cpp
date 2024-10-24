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

#include "requests/parameterscache.h"

#include <fstream>

#include <zip.h>

namespace KDC {

bool LogArchiver::getLogDirEstimatedSize(uint64_t &size, IoError &ioError) {
    const SyncPath logPath = Log::instance()->getLogFilePath().parent_path();
    bool result = false;
    for (int i = 0; i < 2; i++) { // Retry once in case a log file is archived/created during the first iteration
        result = IoHelper::getDirectorySize(logPath, size, ioError);
        if (ioError == IoError::Success) {
            size *= 0.8; // The compressed logs will be smaller than the original ones. We estimate at worst 80% of the
                         // original size.
            return true;
        }
    }
    LOGW_WARN(Log::instance()->getLogger(),
              L"Error in LogArchiver::getLogDirEstimatedSize: " << Utility::formatIoError(logPath, ioError).c_str());

    return result;
}

ExitCode LogArchiver::generateLogsSupportArchive(bool includeArchivedLogs, const SyncPath &outputPath,
                                                 const std::function<bool(int)> &progressCallback, SyncPath &archivePath,
                                                 ExitCause &exitCause, bool test) {
    const std::function<bool(int)> safeProgressCallback = progressCallback ? progressCallback : [](int) { return true; };

    // Get the log directory path
    const SyncPath logPath = Log::instance()->getLogFilePath().parent_path();
    const SyncPath tempLogArchiveDir =
            logPath / "temp_support_archive_generator" / ("tempLogArchive_" + CommonUtility::generateRandomStringAlphaNum(10));
    exitCause = ExitCause::Unknown;
    IoError ioError = IoError::Success;

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
                return ExitCode::InvalidToken; // Currently, we can't send logs if no drive is found
            }
        } catch (const std::runtime_error &e) {
            LOG_WARN(Log::instance()->getLogger(), "Error in generateLogsSupportArchive: " << e.what());
            exitCause = ExitCause::DbAccessError;
            return ExitCode::DbError;
        }

        for (const auto &drive: driveList) {
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
    if (!IoHelper::createDirectory(tempLogArchiveDir.parent_path(), ioError) && ioError != IoError::DirectoryExists) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in IoHelper::createDirectory: "
                          << Utility::formatIoError(tempLogArchiveDir.parent_path(), ioError).c_str());
        if (ioError == IoError::DiskFull) {
            exitCause = ExitCause::NotEnoughDiskSpace;
        } else {
            exitCause = ExitCause::Unknown;
        }
        return ExitCode::SystemError;
    }

    if (!IoHelper::createDirectory(tempLogArchiveDir, ioError)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in IoHelper::createDirectory: " << Utility::formatIoError(tempLogArchiveDir, ioError).c_str());
        IoError ignoreError = IoError::Success;
        IoHelper::deleteItem(tempLogArchiveDir.parent_path(), ignoreError);
        if (ioError == IoError::DiskFull) {
            exitCause = ExitCause::NotEnoughDiskSpace;
        } else {
            exitCause = ExitCause::Unknown;
        }
        return ExitCode::SystemError;
    }

    ExitCode exitCode = copyLogsTo(tempLogArchiveDir, includeArchivedLogs, exitCause);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Unable to copy logs to temp folder: " << exitCause);
        IoHelper::deleteItem(tempLogArchiveDir.parent_path(), ioError);
        return exitCode;
    }

    // Copy .parmsdb to temp folder
    exitCode = copyParmsDbTo(tempLogArchiveDir / ".parms.db", exitCause);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Unable to copy .parms.db to temp folder: " << exitCause);
        IoHelper::deleteItem(tempLogArchiveDir.parent_path(), ioError);
        return exitCode;
    }

    // Generate user description file
    exitCode = generateUserDescriptionFile(tempLogArchiveDir / "user_description.txt", exitCause);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Unable to generate user description file: " << exitCause);
        IoHelper::deleteItem(tempLogArchiveDir.parent_path(), ioError);
        return exitCode;
    }

    // compress all the files in the folder
    std::function<bool(int)> compressLogFilesProgressCallback = [&safeProgressCallback](int progressPercent) {
        return safeProgressCallback((progressPercent * 95) / 100); // The last 5% is for the archive generation
    };
    exitCode = compressLogFiles(tempLogArchiveDir, compressLogFilesProgressCallback, exitCause);
    if (exitCause == ExitCause::OperationCanceled) {
        IoHelper::deleteItem(tempLogArchiveDir.parent_path(), ioError);
        return ExitCode::Ok;
    } else if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Unable to compress logs: " << exitCause);
        IoHelper::deleteItem(tempLogArchiveDir.parent_path(), ioError);
        return exitCode;
    }

    // Generate the archive
    int err = 0;
    zip_t *archive = zip_open(archivePath.string().c_str(), ZIP_CREATE | ZIP_EXCL, &err);
    if (err != ZIP_ER_OK) {
        LOG_WARN(Log::instance()->getLogger(), "Error in zip_open: " << zip_strerror(archive));
        IoHelper::deleteItem(tempLogArchiveDir.parent_path(), ioError);

        exitCause = ExitCause::Unknown;
        return ExitCode::SystemError;
    }

    IoHelper::DirectoryIterator dir;
    if (!IoHelper::getDirectoryIterator(tempLogArchiveDir, false, ioError, dir)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in DirectoryIterator: " << Utility::formatIoError(tempLogArchiveDir, ioError).c_str());
        IoHelper::deleteItem(tempLogArchiveDir.parent_path(), ioError);

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
            IoHelper::deleteItem(tempLogArchiveDir.parent_path(), ioError);
            exitCause = ExitCause::Unknown;
            return ExitCode::Unknown;
        }

        const std::string entryName = entry.path().filename().string();
        if (zip_file_add(archive, entryName.c_str(), source, ZIP_FL_OVERWRITE) < 0) {
            LOG_WARN(Log::instance()->getLogger(), "Error in zip_file_add: " << zip_strerror(archive));
            IoHelper::deleteItem(tempLogArchiveDir.parent_path(), ioError);
            exitCause = ExitCause::Unknown;
            return ExitCode::Unknown;
        }
    }

    if (!endOfDirectory) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in DirectoryIterator: " << Utility::formatIoError(tempLogArchiveDir, ioError).c_str());
        IoHelper::deleteItem(tempLogArchiveDir.parent_path(), ioError);
        exitCause = ExitCause::Unknown;
        return ExitCode::Unknown;
    }

    // Close the archive
    if (zip_close(archive) < 0) {
        LOG_WARN(Log::instance()->getLogger(), "Error in zip_close: " << zip_strerror(archive));
        IoHelper::deleteItem(tempLogArchiveDir.parent_path(), ioError);
        exitCause = ExitCause::Unknown;
        return ExitCode::Unknown;
    }

    // Delete the temp folder
    if (!IoHelper::deleteItem(tempLogArchiveDir.parent_path(), ioError)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in IoHelper::deleteItem: " << Utility::formatIoError(tempLogArchiveDir.parent_path(), ioError).c_str());
    }
    safeProgressCallback(100);
    exitCause = ExitCause::Unknown;
    return ExitCode::Ok;
}

ExitCode LogArchiver::copyLogsTo(const SyncPath &outputPath, bool includeArchivedLogs, ExitCause &exitCause) {
    exitCause = ExitCause::Unknown;
    SyncPath logPath = Log::instance()->getLogFilePath().parent_path();

    IoError ioError = IoError::Success;
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
            if (ioError == IoError::DiskFull) {
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
    IoError ioError = IoError::Unknown;
    exitCause = ExitCause::Unknown;

    if (!IoHelper::getDirectoryEntry(parmsDbPath, ioError, entryParmsDb)) {
        LOGW_DEBUG(KDC::Log::instance()->getLogger(), L"Creating ");

        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in IoHelper::getDirectoryEntry: " << Utility::formatIoError(parmsDbPath, ioError).c_str());
        if (ioError == IoError::NoSuchFileOrDirectory) {
            exitCause = ExitCause::NotFound;
        }
        return ExitCode::SystemError;
    }

    if (!IoHelper::copyFileOrDirectory(parmsDbPath, outputPath, ioError)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in IoHelper::copyFileOrDirectory: " << Utility::formatIoError(parmsDbPath, ioError).c_str());

        if (ioError == IoError::DiskFull) {
            exitCause = ExitCause::NotEnoughDiskSpace;
        }
        return ExitCode::SystemError;
    }
    return ExitCode::Ok;
}

ExitCode LogArchiver::compressLogFiles(const SyncPath &directoryToCompress, const std::function<bool(int)> &progressCallback,
                                       ExitCause &exitCause) {
    const std::function<bool(int)> safeProgressCallback = progressCallback ? progressCallback : [](int) { return true; };
    IoHelper::DirectoryIterator dir;
    IoError ioError = IoError::Unknown;
    exitCause = ExitCause::Unknown;
    if (!IoHelper::getDirectoryIterator(directoryToCompress, true, ioError, dir)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in DirectoryIterator: " << Utility::formatIoError(directoryToCompress, ioError).c_str());
        return ExitCode::SystemError;
    }

    // Count the size of the files to compress
    uint64_t totalSize = 1; // Avoid division by zero
    DirectoryEntry entry;
    bool endOfDirectory = false;
    while (dir.next(entry, endOfDirectory, ioError) && !endOfDirectory) { // cannot use IoHelper::getDirectorySize because it
                                                                          // will count the size of the already compressed files
        if (entry.path().filename().extension() == Str(".gz")) {
            continue;
        }

        ItemType itemType;
        if (!IoHelper::getItemType(entry.path(), itemType)) {
            LOGW_WARN(Log::instance()->getLogger(),
                      L"Error in IoHelper::getItemType: " << Utility::formatIoError(entry.path(), ioError).c_str());
            continue; // Don't process the item
        }
        if (itemType.ioError != IoError::Success) {
            LOGW_WARN(Log::instance()->getLogger(),
                      L"Unable to read item type for " << Utility::formatIoError(entry.path(), itemType.ioError).c_str());
            continue; // Don't process the item
        }

        if (itemType.ioError == IoError::Success && itemType.nodeType == NodeType::File) {
            uint64_t fileSize = 0;
            if (!getFileSize(entry.path(), fileSize)) {
                LOGW_WARN(Log::instance()->getLogger(),
                          L"Error in LogArchiver::getFileSize for " << Utility::formatSyncPath(entry.path()).c_str());
                continue; // Don't process the file
            }

            totalSize += fileSize;
        }
    }

    if (!IoHelper::getDirectoryIterator(directoryToCompress, true, ioError, dir)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in DirectoryIterator: " << Utility::formatIoError(directoryToCompress, ioError).c_str());
        return ExitCode::SystemError;
    }

    // Compress the files
    uint64_t compressedFilesSize = 0;
    while (dir.next(entry, endOfDirectory, ioError) && !endOfDirectory) {
        if (entry.path().filename().extension() == Str(".gz") || !entry.is_regular_file()) {
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
        std::string destPath = entryPathStr + ".gz";
        bool canceled = false;
        uint64_t fileSize = 0;
        if (!getFileSize(entry.path(), fileSize)) {
            LOGW_WARN(Log::instance()->getLogger(),
                      L"Error in LogArchiver::getFileSize for " << Utility::formatSyncPath(entry.path()).c_str());
            continue; // Don't process the file
        }

        std::function<bool(int)> compressProgressCallback = [&safeProgressCallback, &canceled, &compressedFilesSize, &entry,
                                                             &destPath, &totalSize, &fileSize](int progressPercent) {
            auto parametersCacheInstance = ParametersCache::instance();
            if (parametersCacheInstance && parametersCacheInstance->parameters().extendedLog()) {
                LOG_DEBUG(Log::instance()->getLogger(),
                          "File compression: -path: " << destPath.c_str() << " - sub percent: " << progressPercent
                                                      << " - total compression step percent: "
                                                      << (compressedFilesSize * 100 + progressPercent * fileSize) / totalSize);
            }
            canceled = !safeProgressCallback((compressedFilesSize * 100 + progressPercent * fileSize) / totalSize);
            return !canceled;
        };

        if (!CommonUtility::compressFile(entryPathStr, destPath, compressProgressCallback)) {
            LOG_WARN(Log::instance()->getLogger(),
                     "Error in compressFile for " << entryPathStr.c_str() << " to " << destPath.c_str());
            return ExitCode::SystemError;
        }

        if (canceled) {
            LOG_INFO(Log::instance()->getLogger(), "Log compression canceled");
            exitCause = ExitCause::OperationCanceled;
            return ExitCode::Ok;
        }

        if (!IoHelper::deleteItem(entry.path(), ioError)) { // Delete the original file
            LOGW_WARN(Log::instance()->getLogger(),
                      L"Error in IoHelper::deleteItem: " << Utility::formatIoError(entry.path(), ioError).c_str());
            return ExitCode::SystemError;
        }
        compressedFilesSize += fileSize;
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
            for (const User &user: userList) {
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
            for (const Drive &drive: driveList) {
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

bool LogArchiver::getFileSize(const SyncPath &path, uint64_t &size) {
    size = 0;

    IoError ioError = IoError::Unknown;
    if (!IoHelper::getFileSize(path, size, ioError)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in IoHelper::getFileSize for " << Utility::formatIoError(path, ioError).c_str());
        return false;
    }
    if (ioError != IoError::Success) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Unable to read file size for " << Utility::formatIoError(path, ioError).c_str());
        return false;
    }

    return true;
}

}; // namespace KDC
