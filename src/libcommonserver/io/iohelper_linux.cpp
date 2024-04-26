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

#include <sys/stat.h>
#include <sys/types.h>

namespace KDC {

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

bool IoHelper::getFileStat(const SyncPath &path, FileStat *buf, IoError &ioError) noexcept {
    ioError = IoErrorSuccess;

    struct stat sb;

    if (lstat(path.string().c_str(), &sb) < 0) {
        ioError = posixError2ioError(errno);
        return _isExpectedError(ioError);
    }

    buf->isHidden = false;  // lstat does not provide this information on Linux system
    if (!_checkIfIsHiddenFile(path, buf->isHidden, ioError)) {
        return false;
    }

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

bool IoHelper::isFileAccessible(const SyncPath &absolutePath, IoError &ioError) {
    return true;
}

bool IoHelper::_checkIfIsHiddenFile(const SyncPath &path, bool &isHidden, IoError &ioError) noexcept {
    isHidden = false;
    ioError = IoErrorSuccess;

    bool exists = false;
    if (!checkIfPathExists(path, exists, ioError)) {  // For consistency with MacOSX and Windows.
        return false;
    }

    isHidden = path.filename().string()[0] == '.';

    return true;
}

bool IoHelper::checkIfFileIsDehydrated(const SyncPath &itemPath, bool &isDehydrated, IoError &ioError) noexcept {
    isDehydrated = false;
    ioError = IoErrorSuccess;

    bool exists = false;
    if (!checkIfPathExists(itemPath, exists, ioError)) {
        return false;
    }

    if (!exists) {
        ioError = IoErrorNoSuchFileOrDirectory;
    }

    return true;
}

bool IoHelper::getRights(const SyncPath &path, bool &read, bool &write, bool &exec, bool &exists) noexcept {
    read = false;
    write = false;
    exec = false;
    exists = false;

    std::error_code ec;
    std::filesystem::perms perms = std::filesystem::status(path, ec).permissions();
    if (ec.value() != 0) {
        exists = (ec.value() != static_cast<int>(std::errc::no_such_file_or_directory));
        if (!exists) {
            // Path doesn't exist
            return true;
        }

        return false;
    }

    exists = true;
    read = ((perms & std::filesystem::perms::owner_read) != std::filesystem::perms::none);
    write = ((perms & std::filesystem::perms::owner_write) != std::filesystem::perms::none);
    exec = ((perms & std::filesystem::perms::owner_exec) != std::filesystem::perms::none);
    return true;
}


}  // namespace KDC
