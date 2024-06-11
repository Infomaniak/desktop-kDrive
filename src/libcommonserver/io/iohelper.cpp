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

#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"  // Path2WStr

#include "config.h"  // APPLICATION

#include <filesystem>
#include <system_error>

#if defined(__APPLE__) || defined(__unix__)
#include <sys/stat.h>
#endif

#include <log4cplus/loggingmacros.h>  // LOGW_WARN

namespace KDC {

// Default `std::filesytem` implementation. This can be changed in unit tests.
std::function<bool(const SyncPath &path, std::error_code &ec)> IoHelper::_isDirectory =
    static_cast<bool (*)(const SyncPath &path, std::error_code &ec)>(&std::filesystem::is_directory);
std::function<bool(const SyncPath &path, std::error_code &ec)> IoHelper::_isSymlink =
    static_cast<bool (*)(const SyncPath &path, std::error_code &ec)>(&std::filesystem::is_symlink);
std::function<SyncPath(const SyncPath &path, std::error_code &ec)> IoHelper::_readSymlink =
    static_cast<SyncPath (*)(const SyncPath &path, std::error_code &ec)>(&std::filesystem::read_symlink);
std::function<std::uintmax_t(const SyncPath &path, std::error_code &ec)> IoHelper::_fileSize =
    static_cast<std::uintmax_t (*)(const SyncPath &path, std::error_code &ec)>(&std::filesystem::file_size);
std::function<SyncPath(std::error_code &ec)> IoHelper::_tempDirectoryPath =
    static_cast<SyncPath (*)(std::error_code &ec)>(&std::filesystem::temp_directory_path);
#ifdef __APPLE__
std::function<bool(const SyncPath &path, SyncPath &targetPath, IoError &ioError)> IoHelper::_readAlias =
    [](const SyncPath &path, SyncPath &targetPath, IoError &ioError) -> bool {
    std::string data;
    return IoHelper::readAlias(path, data, targetPath, ioError);
};
#endif

#ifndef _WIN32
IoError IoHelper::stdError2ioError(int error) noexcept {
    switch (error) {
        case 0:
            return IoErrorSuccess;
        case static_cast<int>(std::errc::file_exists):
            return IoErrorFileExists;
        case static_cast<int>(std::errc::filename_too_long):
            return IoErrorFileNameTooLong;
        case static_cast<int>(std::errc::invalid_argument):
            return IoErrorInvalidArgument;
        case static_cast<int>(std::errc::is_a_directory):
            return IoErrorIsADirectory;
        case static_cast<int>(std::errc::no_such_file_or_directory):
        case static_cast<int>(std::errc::not_a_directory):  // Occurs in particular when converting a bundle into a single file
            return IoErrorNoSuchFileOrDirectory;
        case static_cast<int>(std::errc::no_space_on_device):
            return IoErrorDiskFull;
        case static_cast<int>(std::errc::permission_denied):
            return IoErrorAccessDenied;
        default:
            return IoErrorUnknown;
    }
}
#endif

log4cplus::Logger IoHelper::_logger;

IoError IoHelper::stdError2ioError(const std::error_code &ec) noexcept {
    return stdError2ioError(ec.value());
}

IoError IoHelper::posixError2ioError(int error) noexcept {
    switch (error) {
        case 0:
            return IoErrorSuccess;
        case EACCES:
            return IoErrorAccessDenied;
        case EEXIST:
            return IoErrorFileExists;
        case EISDIR:
            return IoErrorIsADirectory;
        case EINVAL:
            return IoErrorInvalidArgument;
        case ENAMETOOLONG:
            return IoErrorFileNameTooLong;
#ifdef _WIN32
        case ESRCH:
#endif
        case ENOENT:
            return IoErrorNoSuchFileOrDirectory;
#ifdef __APPLE__
        case ENOATTR:
            return IoErrorAttrNotFound;
#endif
        case ENOSPC:
            return IoErrorDiskFull;
        case ERANGE:
            return IoErrorResultOutOfRange;
        default:
            return IoErrorUnknown;
    }
}

std::string IoHelper::ioError2StdString(IoError ioError) noexcept {
    switch (ioError) {
        case IoErrorAccessDenied:
            return "Access denied";
        case IoErrorAttrNotFound:
            return "Attribute not found";
        case IoErrorDiskFull:
            return "Disk full";
        case IoErrorFileExists:
            return "File exists";
        case IoErrorFileNameTooLong:
            return "File name too long";
        case IoErrorInvalidArgument:
            return "Invalid argument";
        case IoErrorIsADirectory:
            return "Is a directory";
        case IoErrorNoSuchFileOrDirectory:
            return "No such file or directory";
        case IoErrorResultOutOfRange:
            return "Result out of range";
        case IoErrorSuccess:
            return "Success";
        case IoErrorInvalidDirectoryIterator:
            return "Invalid directory iterator";
        default:
            return "Unknown error";
    }
}

bool IoHelper::_isExpectedError(IoError ioError) noexcept {
    return (ioError == IoErrorNoSuchFileOrDirectory) || (ioError == IoErrorAccessDenied);
}
//! Set the target type of a link item.
/*!
  \param targetPath is the file system path of the target of inspected link item.
  \param itemType is the type of a link item whose targetPath is `targetPath`.
  \return true if no error occurred or if the error is expected, false otherwise.
*/
bool IoHelper::_setTargetType(ItemType &itemType) noexcept {
    if (itemType.targetPath.empty()) {
        itemType.targetType = NodeTypeUnknown;
        return true;
    }

    std::error_code ec;
    const bool isDir = _isDirectory(itemType.targetPath, ec);
    IoError ioError = stdError2ioError(ec);

    if (ioError != IoErrorSuccess) {
        const bool expected = _isExpectedError(ioError);
        if (!expected) {
            itemType.ioError = ioError;
            LOGW_WARN(logger(), L"Failed to check if the item is a directory: "
                                    << Utility::formatStdError(itemType.targetPath, ec).c_str());
        }
        return expected;
    }

    itemType.targetType = isDir ? NodeTypeDirectory : NodeTypeFile;

    return true;
}

#if defined(__APPLE__) || defined(__unix__)
bool IoHelper::fileExists(const std::error_code &ec) noexcept {
    return ec.value() != static_cast<int>(std::errc::no_such_file_or_directory);
}

bool IoHelper::getNodeId(const SyncPath &path, NodeId &nodeId) noexcept {
    struct stat sb;

    if (lstat(path.string().c_str(), &sb) < 0) {
        return false;
    }

    nodeId = std::to_string(sb.st_ino);
    return true;
}

bool IoHelper::isFileAccessible(const SyncPath &absolutePath, IoError &ioError) {
    return true;
}

bool IoHelper::getFileStat(const SyncPath &path, FileStat *buf, IoError &ioError) noexcept {
    ioError = IoErrorSuccess;

    struct stat sb;

    if (lstat(path.string().c_str(), &sb) < 0) {
        ioError = posixError2ioError(errno);
        return _isExpectedError(ioError);
    }

#if defined(__APPLE__)
    buf->isHidden = false;
    if (sb.st_flags & UF_HIDDEN) {
        buf->isHidden = true;
    }
#elif defined(__unix__)
    if (!_checkIfIsHiddenFile(path, buf->isHidden, ioError)) {
        return false;
    }
#endif

    buf->inode = sb.st_ino;
#if !defined(_POSIX_C_SOURCE) || defined(_DARWIN_C_SOURCE)
    buf->creationTime = sb.st_birthtime;
#else
    buf->creationTime = sb.st_ctime;
#endif
    buf->modtime = sb.st_mtime;
    buf->size = sb.st_size;
    if (S_ISLNK(sb.st_mode)) {
        // Symlink
        struct stat sbTarget;
        if (stat(path.string().c_str(), &sbTarget) < 0) {
            // Cannot access target => undetermined
            buf->nodeType = NodeTypeUnknown;
        } else {
            buf->nodeType = S_ISDIR(sbTarget.st_mode) ? NodeTypeDirectory : NodeTypeFile;
        }
    } else {
        buf->nodeType = S_ISDIR(sb.st_mode) ? NodeTypeDirectory : NodeTypeFile;
    }

    return true;
}

bool IoHelper::_checkIfIsHiddenFile(const SyncPath &path, bool &isHidden, IoError &ioError) noexcept {
    ioError = IoErrorSuccess;

    isHidden = path.filename().string()[0] == '.';
    if (isHidden) {
        return true;
    }

#ifdef __APPLE__
    static const std::string VolumesFolder = "/Volumes";

    if (path == VolumesFolder) {
        // `VolumesFolder` is always hidden on MacOSX whereas kDrive needs to consider it as visible.
        isHidden = false;
        return true;
    }

    FileStat filestat;
    if (!getFileStat(path, &filestat, ioError)) {
        LOGW_WARN(logger(), L"Error in IoHelper::getFileStat: " << Utility::formatIoError(path, ioError).c_str());
        return false;
    }

    if (ioError != IoErrorSuccess) {
        return _isExpectedError(ioError);
    }

    isHidden = filestat.isHidden;
#endif  // #ifdef __APPLE__

    return true;
}
#endif  // #if defined(__APPLE__) || defined(__unix__)

bool IoHelper::getItemType(const SyncPath &path, ItemType &itemType) noexcept {
    // Check whether the item indicated by `path` is a symbolic link.
    std::error_code ec;
    const bool isSymlink = _isSymlink(path, ec);

    itemType.ioError = stdError2ioError(ec);
    const bool fsSupportsSymlinks =
        itemType.ioError != IoErrorInvalidArgument;  // If true, we assume that the file system in use does support symlinks.

    if (!isSymlink && itemType.ioError != IoErrorSuccess && fsSupportsSymlinks) {
        if (_isExpectedError(itemType.ioError)) {
            return true;
        }
        LOGW_WARN(logger(), L"Failed to check if the item is a symlink: " << Utility::formatStdError(path, ec).c_str());
        return false;
    }

    if (isSymlink) {
        itemType.targetPath = _readSymlink(path, ec);
        itemType.ioError = IoHelper::stdError2ioError(ec);
        if (itemType.ioError != IoErrorSuccess) {
            const bool success = _isExpectedError(itemType.ioError);
            if (!success) {
                LOGW_WARN(logger(), L"Failed to read symlink: " << Utility::formatStdError(path, ec).c_str());
            }
            return success;
        }

        itemType.nodeType = NodeTypeFile;
        itemType.linkType = LinkTypeSymlink;

        // Get target type
        FileStat filestat;
        if (!getFileStat(path, &filestat, itemType.ioError)) {
            LOGW_WARN(logger(), L"Error in getFileStat: " << Utility::formatStdError(path, ec).c_str());
            return false;
        }

        if (itemType.ioError != IoErrorSuccess) {
            return _isExpectedError(itemType.ioError);
        }

        itemType.targetType = filestat.nodeType;
        return true;
    }

#ifdef __APPLE__
    // Check whether the item indicated by `path` is an alias.
    bool isAlias = false;
    if (!_checkIfAlias(path, isAlias, itemType.ioError)) {
        LOGW_WARN(logger(),
                  L"Failed to check if the item is an alias: " << Utility::formatIoError(path, itemType.ioError).c_str());
        return false;
    }

    if (itemType.ioError != IoErrorSuccess) {
        return _isExpectedError(itemType.ioError);
    }

    if (isAlias) {
        // !!! isAlias is true for a symlink and for a Finder alias !!!
        IoError aliasReadError = IoErrorSuccess;
        if (!_readAlias(path, itemType.targetPath, aliasReadError)) {
            LOGW_WARN(logger(), L"Failed to read an item first identified as an alias: "
                                    << Utility::formatIoError(path, itemType.ioError).c_str());
            itemType.ioError = aliasReadError;

            return false;
        }

        itemType.nodeType = NodeTypeFile;
        itemType.linkType = LinkTypeFinderAlias;

        if (itemType.ioError != IoErrorSuccess) {
            return _isExpectedError(itemType.ioError);
        }

        return _setTargetType(itemType);
    }
#endif

#ifdef _WIN32
    // Check whether the item indicated by `path` is a junction.
    if (fsSupportsSymlinks) {
        bool isJunction = false;
        if (!checkIfIsJunction(path, isJunction, itemType.ioError)) {
            LOGW_WARN(logger(),
                      L"Failed to check if the item is a junction: " << Utility::formatIoError(path, itemType.ioError).c_str());

            return false;
        }

        if (itemType.ioError != IoErrorSuccess) {
            return _isExpectedError(itemType.ioError);
        }

        if (isJunction) {
            itemType.nodeType = NodeTypeFile;
            itemType.linkType = LinkTypeJunction;
            itemType.targetType = NodeTypeDirectory;

            std::string data;
            if (!IoHelper::readJunction(path, data, itemType.targetPath, itemType.ioError)) {
                LOGW_WARN(logger(), L"Failed to read an item identified as a junction: "
                                        << Utility::formatIoError(path, itemType.ioError).c_str());
                return false;
            }

            return true;
        }
    }
#endif

    // The remaining case: a file or a directory which is not a link
    const bool isDir = _isDirectory(path, ec);
    itemType.ioError = stdError2ioError(ec);
    if (itemType.ioError != IoErrorSuccess) {
        const bool success = _isExpectedError(itemType.ioError);
        if (!success) {
            LOGW_WARN(logger(), L"Failed to check that item is a directory: " << Utility::formatStdError(path, ec).c_str());
        }
        return success;
    }

    itemType.nodeType = isDir ? NodeTypeDirectory : NodeTypeFile;

    return true;
}

bool IoHelper::getFileSize(const SyncPath &path, uint64_t &size, IoError &ioError) {
    ItemType itemType;
    const bool success = getItemType(path, itemType);
    ioError = itemType.ioError;
    if (!success) {
        return false;
    }

    assert(ioError != IoErrorUnknown);

    if (itemType.nodeType == NodeTypeDirectory) {
        ioError = IoErrorIsADirectory;
        return false;
    }

    const bool isSymlink = itemType.linkType == LinkTypeSymlink;
    if (isSymlink) {
        size = itemType.targetPath.native().length();
    } else {
        if (itemType.nodeType != NodeTypeFile) {
            assert(ioError != IoErrorSuccess);
            return ioError != IoErrorUnknown;
        }

        std::error_code ec;
        size = _fileSize(path, ec);  // The std::filesystem implementation reports the correct size for a MacOSX alias.
        ioError = stdError2ioError(ec);
        if (ioError != IoErrorUnknown) {
            return true;
        }

        LOGW_WARN(logger(), L"Failed to get file size: " << Utility::formatStdError(path, ec).c_str());
        return false;
    }

    return true;
}

bool IoHelper::getDirectorySize(const SyncPath &path, uint64_t &size, IoError &ioError, unsigned int maxDepth) {
    size = 0;
    ItemType itemType;
    const bool success = getItemType(path, itemType);
    ioError = itemType.ioError;
    if (!success || ioError != IoErrorSuccess) {
        return _isExpectedError(ioError);
    }

    assert(ioError != IoErrorUnknown);

    if (itemType.nodeType != NodeTypeDirectory) {
        ioError = IoErrorIsAFile;
        return false;
    }

    IoHelper::DirectoryIterator dir;
    IoHelper::getDirectoryIterator(path, true, ioError, dir);
    if (ioError != IoErrorSuccess) {
        LOGW_WARN(logger(), L"Error in DirectoryIterator: " << Utility::formatIoError(path, ioError).c_str());
        return _isExpectedError(ioError);
    }

    DirectoryEntry entry;
    ioError = IoErrorSuccess;
    bool endOfDirectory = false;
    while (dir.next(entry, endOfDirectory, ioError) && !endOfDirectory) {
        if (entry.is_directory()) {
            if (maxDepth == 0) {
                LOGW_WARN(logger(), L"Max depth reached in getDirectorySize, skipping deeper directories: "
                                        << Utility::formatSyncPath(path).c_str());
                ioError = IoErrorMaxDepthExceeded;
                return _isExpectedError(ioError);
            }
            uint64_t entrySize = 0;
            if (!getDirectorySize(entry.path(), entrySize, ioError, maxDepth - 1)) {
                return false;
            }
            size += entrySize;
            continue;
        }
        std::error_code ec;
        uint64_t entrySize = _fileSize(entry.path(), ec);
        if (!ec) {
            size += entrySize;
        } else {
            LOGW_WARN(logger(), L"Error in file_size: " << Utility::formatStdError(entry.path(), ec).c_str());
            ioError = stdError2ioError(ec);
            return _isExpectedError(ioError);
        }
    }

    if (!endOfDirectory) {
        LOGW_WARN(logger(), L"Error in DirectoryIterator: " << Utility::formatIoError(path, ioError).c_str());
        return _isExpectedError(ioError);
    }

    return true;
}

bool IoHelper::tempDirectoryPath(SyncPath &directoryPath, IoError &ioError) noexcept {
    std::error_code ec;
    directoryPath = _tempDirectoryPath(ec);  // The std::filesystem implementation returns an empty path on error.
    ioError = stdError2ioError(ec);

    return ioError == IoError::IoErrorSuccess;
}

bool IoHelper::logDirectoryPath(SyncPath &directoryPath, IoError &ioError) noexcept {
    //!\ Don't use IoHelper::logger() here, as Log::_instance may not be initialized yet. /!\_
    try {
        if (directoryPath = Log::instance()->getLogFilePath().parent_path(); !directoryPath.empty()) {
            return true;
        } else {
            throw std::runtime_error("Log directory path is empty.");
        }
    } catch (const std::exception &e) {
        if (Log::isSet()) {
            LOGW_WARN(logger(), L"Error in IoHelper::logDirectoryPath: " << e.what());
        } 
        // We can't log the error, so we just generate the path for the logger to initialize.
    }

    if (!tempDirectoryPath(directoryPath, ioError)) {
        return false;
    }

    static const std::string LOGDIR_SUFFIX = "-logdir/";
    const SyncName logDirName = SyncName(Str2SyncName(APPLICATION_NAME)) + SyncName(Str2SyncName(LOGDIR_SUFFIX));
    directoryPath /= logDirName;

    return ioError == IoError::IoErrorSuccess;
}

bool IoHelper::logArchiverDirectoryPath(SyncPath &directoryPath, IoError &ioError) noexcept {
    SyncPath tempDir;
    tempDirectoryPath(tempDir, ioError);
    if (ioError != IoErrorSuccess) {
        return false;
    }
    const SyncName logArchiverDirName = SyncName(Str2SyncName(APPLICATION_NAME)) + SyncName(Str2SyncName("-logarchiverdir/"));
    directoryPath = tempDir / logArchiverDirName;
    return true;
}


bool IoHelper::checkIfPathExists(const SyncPath &path, bool &exists, IoError &ioError) noexcept {
    exists = false;
    ioError = IoErrorSuccess;

    ItemType itemType;
    const bool getItemTypeSuccess = getItemType(path, itemType);
    ioError = itemType.ioError;

    if (!getItemTypeSuccess) {
        LOGW_WARN(logger(), L"Error in IoHelper::getItemType: " << Utility::formatIoError(path, itemType.ioError).c_str());
        return false;
    }

    if (itemType.linkType == LinkTypeNone && itemType.ioError != IoErrorSuccess) {
        exists = ioError != IoErrorNoSuchFileOrDirectory;
        return _isExpectedError(ioError);
    }

    ioError = IoErrorSuccess;
    exists = true;

    return true;
}

bool IoHelper::checkIfPathExistsWithSameNodeId(const SyncPath &path, const NodeId &nodeId, bool &exists,
                                               IoError &ioError) noexcept {
    exists = false;
    ioError = IoErrorSuccess;

    if (!checkIfPathExists(path, exists, ioError)) {
        return false;
    }

    if (exists) {
        // Check nodeId
        NodeId tmpNodeId;
        if (!getNodeId(path, tmpNodeId)) {
            LOGW_WARN(logger(), L"Error in IoHelper::getNodeId for path=" << Path2WStr(path).c_str());
        }
        exists = (nodeId == tmpNodeId);
    }

    return true;
}

void IoHelper::getFileStat(const SyncPath &path, FileStat *buf, bool &exists) {
    IoError ioError = IoErrorSuccess;
    if (!getFileStat(path, buf, ioError)) {
        exists = (ioError != IoErrorNoSuchFileOrDirectory);
        std::string message = ioError2StdString(ioError);
        throw std::runtime_error("IoHelper::getFileStat error: " + message);
    }
}

bool IoHelper::checkIfFileChanged(const SyncPath &path, int64_t previousSize, time_t previousMtime, bool &changed,
                                  IoError &ioError) noexcept {
    changed = false;

    FileStat fileStat;
    if (!getFileStat(path, &fileStat, ioError)) {
        LOGW_WARN(logger(), L"Failed to get file status: " << Utility::formatIoError(path, ioError).c_str());

        return false;
    }

    if (ioError != IoErrorSuccess) {
        return _isExpectedError(ioError);
    }

    changed = (previousSize != fileStat.size) || (previousMtime != fileStat.modtime);

    return true;
}

bool IoHelper::checkIfIsHiddenFile(const SyncPath &path, bool checkAncestors, bool &isHidden, IoError &ioError) noexcept {
    ioError = IoErrorSuccess;
    isHidden = false;

    if (!checkAncestors) {
        return _checkIfIsHiddenFile(path, isHidden, ioError);
    }

    // Check if any item in path is hidden
    SyncPath tmpPath = path;
    SyncPath parentPath = tmpPath.parent_path();
    while (tmpPath != parentPath) {
        if (!_checkIfIsHiddenFile(tmpPath, isHidden, ioError)) {
            return false;
        }

        if (isHidden) {
            break;
        }

        tmpPath = parentPath;
        parentPath = tmpPath.parent_path();
    }

    return true;
}

bool IoHelper::checkIfIsHiddenFile(const SyncPath &path, bool &isHidden, IoError &ioError) noexcept {
    return checkIfIsHiddenFile(path, true, isHidden, ioError);
}

bool IoHelper::checkIfIsDirectory(const SyncPath &path, bool &isDirectory, IoError &ioError) noexcept {
    isDirectory = false;

    ItemType itemType;
    const bool success = getItemType(path, itemType);
    ioError = itemType.ioError;

    if (!success) {
        LOGW_WARN(logger(), L"Error in IoHelper::getItemType: " << Utility::formatIoError(path, ioError).c_str());
        return false;
    }

    isDirectory = itemType.nodeType == NodeTypeDirectory;

    return true;
}

bool IoHelper::createDirectory(const SyncPath &path, IoError &ioError) noexcept {
    std::error_code ec;
    const bool creationSuccess = std::filesystem::create_directory(path, ec);
    ioError = stdError2ioError(ec);

    if (!creationSuccess && ioError == IoErrorSuccess) {
        // The directory wasn't created because it already existed,
        // see https://en.cppreference.com/w/cpp/filesystem/create_directory
        ioError = IoErrorDirectoryExists;
    }

    return creationSuccess;
}

bool IoHelper::deleteDirectory(const SyncPath &path, IoError &ioError) noexcept {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
    ioError = stdError2ioError(ec);
    return ioError == IoErrorSuccess;
}

bool IoHelper::copyFileOrDirectory(const SyncPath &sourcePath, const SyncPath &destinationPath, IoError &ioError) noexcept {
    std::error_code ec;
    std::filesystem::copy(sourcePath, destinationPath, std::filesystem::copy_options::recursive, ec);
    ioError = IoHelper::stdError2ioError(ec);

    return ioError == IoErrorSuccess;
}

bool IoHelper::getDirectoryIterator(const SyncPath &path, bool recursive, IoError &ioError,
                                    DirectoryIterator &iterator) noexcept {
    iterator = DirectoryIterator(path, recursive, ioError);
    return ioError == IoErrorSuccess;
}

bool IoHelper::getDirectoryEntry(const SyncPath &path, IoError &ioError, DirectoryEntry &entry) noexcept {
    std::error_code ec;
    entry = std::filesystem::directory_entry(path, ec);
    ioError = stdError2ioError(ec);
    return ioError == IoErrorSuccess;
}

bool IoHelper::createSymlink(const SyncPath &targetPath, const SyncPath &path, bool isFolder, IoError &ioError) noexcept {
    if (targetPath == path) {
        LOGW_DEBUG(logger(), L"Cannot create symlink on itself: " << Utility::formatSyncPath(path).c_str());
        ioError = IoErrorInvalidArgument;
        return false;
    }

    std::error_code ec;
    if (isFolder) {
        LOGW_DEBUG(logger(), L"Create directory symlink: target " << Path2WStr(targetPath).c_str() << L", "
                                                                  << Utility::formatSyncPath(path).c_str());
        std::filesystem::create_directory_symlink(targetPath, path, ec);
    } else {
        LOGW_DEBUG(logger(), L"Create file symlink: target " << Path2WStr(targetPath).c_str() << L", "
                                                             << Utility::formatSyncPath(path).c_str());
        std::filesystem::create_symlink(targetPath, path, ec);
    }

    ioError = stdError2ioError(ec);

    return ioError == IoErrorSuccess;
}

// DirectoryIterator

IoHelper::DirectoryIterator::DirectoryIterator(const SyncPath &directoryPath, bool recursive, IoError &ioError)
    : _recursive(recursive), _directoryPath(directoryPath) {
    std::error_code ec;

    _dirIterator = std::filesystem::begin(
        std::filesystem::recursive_directory_iterator(directoryPath, DirectoryOptions::skip_permission_denied, ec));
    ioError = IoHelper::stdError2ioError(ec);
}


bool IoHelper::DirectoryIterator::next(DirectoryEntry &nextEntry, bool &endOfDirectory, IoError &ioError) {
    std::error_code ec;
    endOfDirectory = false;

    if (_invalid) {
        ioError = IoErrorInvalidDirectoryIterator;
        return true;
    }

    if (_directoryPath == "") {
        ioError = IoErrorInvalidArgument;
        return false;
    }

    if (_dirIterator == std::filesystem::end(std::filesystem::recursive_directory_iterator(_directoryPath, ec))) {
        endOfDirectory = true;
        ioError = IoErrorSuccess;
        return true;
    }

    if (!_recursive) {
        disableRecursionPending();
    }

    if (!_firstElement) {
        _dirIterator.increment(ec);
        ioError = IoHelper::stdError2ioError(ec);

        if (ioError != IoErrorSuccess) {
            _invalid = true;
            return true;
        }

    } else {
        _firstElement = false;
    }

    if (_dirIterator != std::filesystem::end(std::filesystem::recursive_directory_iterator(_directoryPath, ec))) {
        ioError = IoHelper::stdError2ioError(ec);

        if (ioError != IoErrorSuccess) {
            _invalid = true;
            return true;
        }

#ifdef _WIN32
        // skip_permission_denied doesn't work on Windows
        try {
            bool dummy = _dirIterator->exists();
            (void)dummy;
            nextEntry = *_dirIterator;
            return true;
        } catch (std::filesystem::filesystem_error &) {
            _dirIterator.disable_recursion_pending();
            return next(nextEntry, endOfDirectory, ioError);
        }

#endif
        nextEntry = *_dirIterator;
        return true;
    } else {
        ioError = IoErrorSuccess;
        endOfDirectory = true;
        return true;
    }
}

void IoHelper::DirectoryIterator::disableRecursionPending() {
    _dirIterator.disable_recursion_pending();
}

#ifndef _WIN32
// See iohelper_win.cpp for the Windows implementation
bool IoHelper::setRights(const SyncPath &path, bool read, bool write, bool exec, IoError &ioError) noexcept {
    return _setRightsStd(path, read, write, exec, ioError);
}
#endif

bool IoHelper::_setRightsStd(const SyncPath &path, bool read, bool write, bool exec, IoError &ioError) noexcept {
    ioError = IoErrorSuccess;
    std::filesystem::perms perms = std::filesystem::perms::none;
    if (read) {
        perms |= std::filesystem::perms::owner_read;
    }
    if (write) {
        perms |= std::filesystem::perms::owner_write;
    }
    if (exec) {
        perms |= std::filesystem::perms::owner_exec;
    }

    std::error_code ec;
    std::filesystem::permissions(path, perms, ec);
    if (ec) {
        ioError = posixError2ioError(ec.value());
        return _isExpectedError(ioError);
    }

    return true;
}
}  // namespace KDC
