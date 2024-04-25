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

#include "libcommon/utility/types.h"

#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"  // Path2WStr


#include <log4cplus/loggingmacros.h>

#include <sys/stat.h>
#include <sys/xattr.h>

namespace KDC {

bool isLocked(const SyncPath &path);

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

    if (sb.st_flags & UF_HIDDEN) {
        buf->isHidden = true;
    }

    buf->inode = sb.st_ino;
#if !defined(_POSIX_C_SOURCE) || defined(_DARWIN_C_SOURCE)
    buf->creationTime = sb.st_birthtime;
#else
    buf->creationTime = sb.st_ctime;
#endif
    buf->modtime = sb.st_mtime;
    buf->size = sb.st_size;

    return true;
}

bool IoHelper::isFileAccessible(const SyncPath &absolutePath, IoError &ioError) {
    return true;
}

bool IoHelper::_checkIfIsHiddenFile(const SyncPath &filepath, bool &isHidden, IoError &ioError) noexcept {
    static const std::string VolumesFolder = "/Volumes";

    isHidden = false;
    ioError = IoErrorSuccess;

    if (filepath == VolumesFolder) {
        // `VolumesFolder` is always hidden on MacOSX whereas kDrive needs to consider it as visible.
        isHidden = false;
        return true;
    }

    FileStat filestat;
    bool exists = false;

    if (!getFileStat(filepath.string().c_str(), &filestat, exists, ioError)) {
        LOGW_WARN(logger(), L"Error in IoHelper::getFileStat: " << Utility::formatIoError(filepath, ioError).c_str());
        return false;
    }

    isHidden = filestat.isHidden || filepath.filename().string()[0] == '.';

    return true;
}

namespace {
inline bool _isXAttrValueExpectedError(IoError error) {
    return (error == IoErrorNoSuchFileOrDirectory) || (error == IoErrorAttrNotFound) || (error == IoErrorAccessDenied);
};
}  // namespace

bool IoHelper::getXAttrValue(const SyncPath &path, const std::string &attrName, std::string &value, IoError &ioError) noexcept {
    value = "";
    ItemType itemType;
    if (!getItemType(path, itemType)) {
        LOGW_WARN(logger(), L"Error in IoHelper::getItemType for " << Utility::formatIoError(path, itemType.ioError).c_str());
        ioError = itemType.ioError;
        return false;
    }

    // The item indicated by `path` doesn't exist.
    if (itemType.linkType == LinkTypeNone && itemType.ioError == IoErrorNoSuchFileOrDirectory) {
        ioError = IoErrorNoSuchFileOrDirectory;
        return true;
    }

    const bool isSymlink = itemType.linkType == LinkTypeSymlink;

    for (;;) {
        const long bufferLength = getxattr(path.native().c_str(), attrName.c_str(), NULL, 0, 0, isSymlink ? XATTR_NOFOLLOW : 0);
        if (bufferLength == -1) {
            ioError = posixError2ioError(errno);
            return _isXAttrValueExpectedError(ioError);
        }

        value.resize(bufferLength);
        if (getxattr(path.native().c_str(), attrName.c_str(), value.data(), bufferLength, 0, isSymlink ? XATTR_NOFOLLOW : 0) !=
            bufferLength) {
            ioError = posixError2ioError(errno);
            if (ioError == IoErrorResultOutOfRange) {
                // XAttr length has changed, retry
                continue;
            }
            return _isXAttrValueExpectedError(ioError);
        }

        // XAttr has been read
        ioError = IoErrorSuccess;
        return true;
    }
}

bool IoHelper::setXAttrValue(const SyncPath &path, const std::string &attrName, const std::string &value,
                             IoError &ioError) noexcept {
    ItemType itemType;
    if (!getItemType(path, itemType)) {
        LOGW_WARN(logger(), L"Error in IoHelper::getItemType for " << Utility::formatIoError(path, itemType.ioError).c_str());
        ioError = itemType.ioError;
        return false;
    }

    // The item indicated by `path` doesn't exist.
    if (itemType.linkType == LinkTypeNone && itemType.ioError == IoErrorNoSuchFileOrDirectory) {
        ioError = IoErrorNoSuchFileOrDirectory;
        return true;
    }

    const bool isSymlink = itemType.linkType == LinkTypeSymlink;

    if (setxattr(path.native().c_str(), attrName.c_str(), value.data(), value.size(), 0, isSymlink ? XATTR_NOFOLLOW : 0) == -1) {
        ioError = posixError2ioError(errno);
        return _isXAttrValueExpectedError(ioError);
    }

    // XAttr has been set
    ioError = IoErrorSuccess;
    return true;
}

bool IoHelper::checkIfFileIsDehydrated(const SyncPath &itemPath, bool &isDehydrated, IoError &ioError) noexcept {
    isDehydrated = false;
    ioError = IoErrorSuccess;

    static const std::string EXT_ATTR_STATUS = "com.infomaniak.drive.desktopclient.litesync.status";

    std::string value;
    const bool result = IoHelper::getXAttrValue(itemPath.native(), EXT_ATTR_STATUS, value, ioError);
    if (!result) {
        return false;
    }

    if (!value.empty()) {
        isDehydrated = (value != "F");
    }

    return true;
}

bool IoHelper::getRights(const SyncPath &path, bool &read, bool &write, bool &exec, bool &exists) noexcept {
    read = false;
    write = false;
    exec = false;
    exists = false;

    ItemType itemType;
    const bool success = getItemType(path, itemType);
    if (!success) {
        LOGW_WARN(logger(),
                  L"Failed to check if the item is a symlink: " << Utility::formatIoError(path, itemType.ioError).c_str());
        return false;
    }
    exists = itemType.ioError != IoErrorNoSuchFileOrDirectory;
    if (!exists) {
        return true;
    }
    const bool isSymlink = itemType.linkType == LinkTypeSymlink;

    std::error_code ec;
    std::filesystem::perms perms =
        isSymlink ? std::filesystem::symlink_status(path, ec).permissions() : std::filesystem::status(path, ec).permissions();
    if (ec.value() != 0) {
        exists = (ec.value() != static_cast<int>(std::errc::no_such_file_or_directory));
        if (!exists) {
            // Path doesn't exist
            return true;
        }

        LOGW_WARN(logger(), L"Failed to get permissions - " << Utility::formatStdError(path, ec).c_str());
        return false;
    }

    exists = true;
    read = ((perms & std::filesystem::perms::owner_read) != std::filesystem::perms::none);
    write = isLocked(path) ? false : ((perms & std::filesystem::perms::owner_write) != std::filesystem::perms::none);
    exec = ((perms & std::filesystem::perms::owner_exec) != std::filesystem::perms::none);
    return true;
}

}  // namespace KDC
