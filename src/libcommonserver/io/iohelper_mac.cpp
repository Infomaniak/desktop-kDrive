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

#include "permissionsholder.h"
#include "libcommon/utility/types.h"

#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"

#include <log4cplus/loggingmacros.h>

#include <sys/xattr.h>
#include <sys/stat.h>

namespace KDC {

namespace {
inline bool _isXAttrValueExpectedError(IoError error) {
    return (error == IoError::NoSuchFileOrDirectory) || (error == IoError::AttrNotFound) || (error == IoError::AccessDenied);
}
} // namespace

bool IoHelper::getXAttrValue(const SyncPath &path, const std::string_view &attrName, std::string &value,
                             IoError &ioError) noexcept {
    value = "";
    ItemType itemType;
    if (!getItemType(path, itemType)) {
        LOGW_WARN(logger(), L"Error in IoHelper::getItemType for " << Utility::formatIoError(path, itemType.ioError));
        ioError = itemType.ioError;
        return false;
    }

    // The item indicated by `path` doesn't exist.
    if (itemType.linkType == LinkType::None && itemType.ioError == IoError::NoSuchFileOrDirectory) {
        ioError = IoError::NoSuchFileOrDirectory;
        return true;
    }

    const bool isSymlink = itemType.linkType == LinkType::Symlink;

    for (;;) {
        const ssize_t bufferLength = getxattr(path.native().c_str(), attrName.data(), NULL, 0, 0, isSymlink ? XATTR_NOFOLLOW : 0);
        if (bufferLength == -1) {
            ioError = posixError2ioError(errno);
            return _isXAttrValueExpectedError(ioError);
        }

        value.resize(static_cast<size_t>(bufferLength));
        if (getxattr(path.native().c_str(), attrName.data(), value.data(), static_cast<size_t>(bufferLength), 0,
                     isSymlink ? XATTR_NOFOLLOW : 0) != bufferLength) {
            ioError = posixError2ioError(errno);
            if (ioError == IoError::ResultOutOfRange) {
                // XAttr length has changed, retry
                continue;
            }
            return _isXAttrValueExpectedError(ioError);
        }

        // XAttr has been read
        ioError = IoError::Success;
        return true;
    }
}

bool IoHelper::setXAttrValue(const SyncPath &path, const std::string_view &attrName, const std::string_view &value,
                             IoError &ioError) noexcept {
    ItemType itemType;
    if (!getItemType(path, itemType)) {
        LOGW_WARN(logger(), L"Error in IoHelper::getItemType for " << Utility::formatIoError(path, itemType.ioError));
        ioError = itemType.ioError;
        return false;
    }

    // The item indicated by `path` doesn't exist.
    if (itemType.linkType == LinkType::None && itemType.ioError == IoError::NoSuchFileOrDirectory) {
        ioError = IoError::NoSuchFileOrDirectory;
        return true;
    }

    const bool isSymlink = itemType.linkType == LinkType::Symlink;
    PermissionsHolder permsHolder(path, logger());
    if (setxattr(path.native().c_str(), attrName.data(), value.data(), value.size(), 0, isSymlink ? XATTR_NOFOLLOW : 0) == -1) {
        ioError = posixError2ioError(errno);
        return _isXAttrValueExpectedError(ioError);
    }

    // XAttr has been set
    ioError = IoError::Success;
    return true;
}

bool IoHelper::removeXAttrs(const SyncPath &path, const std::vector<std::string_view> &attrNames, IoError &ioError) noexcept {
    PermissionsHolder permsHolder(path, logger());
    for (const auto &attrName: attrNames) {
        if (removexattr(path.native().c_str(), attrName.data(), XATTR_NOFOLLOW) == -1) {
            ioError = posixError2ioError(errno);
            return _isXAttrValueExpectedError(ioError);
        }
    }

    // XAttr has been removed
    ioError = IoError::Success;
    return true;
}

bool IoHelper::removeLiteSyncXAttrs(const SyncPath &path, IoError &ioError) noexcept {
    static const std::vector<std::string_view> liteSyncAttrName = {litesync_attrs::status, litesync_attrs::pinState};
    return removeXAttrs(path, liteSyncAttrName, ioError);
}

bool IoHelper::checkIfFileIsDehydrated(const SyncPath &itemPath, bool &isDehydrated, IoError &ioError) noexcept {
    isDehydrated = false;
    ioError = IoError::Success;

    std::string value;
    const bool result = IoHelper::getXAttrValue(itemPath.native(), litesync_attrs::status, value, ioError);
    if (!result) {
        return false;
    }

    if (!value.empty()) {
        isDehydrated = (value != litesync_attrs::statusOffline);
    }

    return true;
}

bool IoHelper::_getFileStatFn(const SyncPath &path, FileStat *buf, IoError &ioError) noexcept {
    ioError = IoError::Success;

    struct stat sb;

    if (lstat(path.string().c_str(), &sb) < 0) {
        ioError = posixError2ioError(errno);
        return isExpectedError(ioError);
    }

    buf->isHidden = false;
    buf->_flags = sb.st_flags;
    if (sb.st_flags & UF_HIDDEN) {
        buf->isHidden = true;
    }

    buf->inode = sb.st_ino;
    buf->creationTime =
            sb.st_birthtime; // Supported on all 64-bits macOS versions (32-bits macOS are not supported since nov 2014)
    buf->modificationTime = sb.st_mtime;
    buf->size = sb.st_size;
    if (S_ISLNK(sb.st_mode)) {
        // The item is a symlink.
        struct stat sbTarget;
        if (stat(path.string().c_str(), &sbTarget) < 0) {
            // Cannot access target => undetermined
            buf->nodeType = NodeType::Unknown;
        } else {
            buf->nodeType = S_ISDIR(sbTarget.st_mode) ? NodeType::Directory : NodeType::File;
        }
    } else {
        buf->nodeType = S_ISDIR(sb.st_mode) ? NodeType::Directory : NodeType::File;
    }

    return true;
}

IoError IoHelper::lock(const SyncPath &path) noexcept {
    // Set uchg flag to lock the item.
    if (chflags(path.string().c_str(), UF_IMMUTABLE)) {
        LOGW_DEBUG(Log::instance()->getLogger(), L"Failed to set uchg flag for " << Utility::formatSyncPath(path));
        return IoError::Unknown;
    }
    return IoError::Success;
}

IoError IoHelper::unlock(const SyncPath &path) noexcept {
    FileStat filestat;
    bool found = false;
    IoHelper::getFileStat(path, &filestat, found);
    if (!found) {
        LOGW_DEBUG(Log::instance()->getLogger(), L"Not found: " << Utility::formatSyncPath(path));
        return IoError::NoSuchFileOrDirectory;
    }

    // Unset uchg flag to unlock the item.
    u_int flags = filestat._flags;
    flags &= ~UF_IMMUTABLE;
    if (chflags(path.string().c_str(), flags)) {
        LOGW_DEBUG(Log::instance()->getLogger(), L"Failed to unset uchg flag for " << Utility::formatSyncPath(path));
        return IoError::Unknown;
    }
    return IoError::Success;
}

IoError IoHelper::isLocked(const SyncPath &path, bool &locked) noexcept {
    FileStat filestat;
    bool found = false;
    IoHelper::getFileStat(path, &filestat, found);
    if (!found) {
        LOGW_DEBUG(Log::instance()->getLogger(), L"Not found: " << Utility::formatSyncPath(path));
        return IoError::NoSuchFileOrDirectory;
    }

    locked = filestat._flags & UF_IMMUTABLE;
    return IoError::Success;
}

} // namespace KDC
