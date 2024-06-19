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
#include "libcommonserver/utility/utility.h"

#include <log4cplus/loggingmacros.h>

#include <sys/xattr.h>
#include <sys/stat.h>

namespace KDC {

bool isLocked(const SyncPath &path);

namespace {
inline bool _isXAttrValueExpectedError(IoError error) {
    using enum IoError;
    return (error == NoSuchFileOrDirectory) || (error == AttrNotFound) || (error == AccessDenied);
}
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
    if (itemType.linkType == LinkType::None && itemType.ioError == IoError::NoSuchFileOrDirectory) {
        ioError = IoError::NoSuchFileOrDirectory;
        return true;
    }

    const bool isSymlink = itemType.linkType == LinkType::Symlink;

    for (;;) {
        const long bufferLength = getxattr(path.native().c_str(), attrName.c_str(), nullptr, 0, 0, isSymlink ? XATTR_NOFOLLOW : 0);
        if (bufferLength == -1) {
            ioError = posixError2ioError(errno);
            return _isXAttrValueExpectedError(ioError);
        }

        value.resize(bufferLength);
        if (getxattr(path.native().c_str(), attrName.c_str(), value.data(), bufferLength, 0, isSymlink ? XATTR_NOFOLLOW : 0) !=
            bufferLength) {
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

bool IoHelper::setXAttrValue(const SyncPath &path, const std::string &attrName, const std::string &value,
                             IoError &ioError) noexcept {
    ItemType itemType;
    if (!getItemType(path, itemType)) {
        LOGW_WARN(logger(), L"Error in IoHelper::getItemType for " << Utility::formatIoError(path, itemType.ioError).c_str());
        ioError = itemType.ioError;
        return false;
    }

    // The item indicated by `path` doesn't exist.
    if (itemType.linkType == LinkType::None && itemType.ioError == IoError::NoSuchFileOrDirectory) {
        ioError = IoError::NoSuchFileOrDirectory;
        return true;
    }

    const bool isSymlink = itemType.linkType == LinkType::Symlink;

    if (setxattr(path.native().c_str(), attrName.c_str(), value.data(), value.size(), 0, isSymlink ? XATTR_NOFOLLOW : 0) == -1) {
        ioError = posixError2ioError(errno);
        return _isXAttrValueExpectedError(ioError);
    }

    // XAttr has been set
    ioError = IoError::Success;
    return true;
}

bool IoHelper::checkIfFileIsDehydrated(const SyncPath &itemPath, bool &isDehydrated, IoError &ioError) noexcept {
    isDehydrated = false;
    ioError = IoError::Success;

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

bool IoHelper::getRights(const SyncPath &path, bool &read, bool &write, bool &exec, IoError &ioError) noexcept {
    read = false;
    write = false;
    exec = false;

    ItemType itemType;
    const bool success = getItemType(path, itemType);
    if (!success) {
        LOGW_WARN(logger(), L"Failed to get item type: " << Utility::formatIoError(path, itemType.ioError).c_str());
        return false;
    }
    ioError = itemType.ioError;
    if (ioError != IoError::Success) {
        return _isExpectedError(ioError);
    }
    const bool isSymlink = itemType.linkType == LinkType::Symlink;

    std::error_code ec;
    std::filesystem::perms perms =
        isSymlink ? std::filesystem::symlink_status(path, ec).permissions() : std::filesystem::status(path, ec).permissions();
    if (ec) {
       const bool exists = (ec.value() != static_cast<int>(std::errc::no_such_file_or_directory));
        ioError = stdError2ioError(ec);
        if (!exists) {
            ioError = IoError::NoSuchFileOrDirectory;
        }
        LOGW_WARN(logger(), L"Failed to get permissions: " << Utility::formatStdError(path, ec).c_str());
        return _isExpectedError(ioError);
    }

    read = ((perms & std::filesystem::perms::owner_read) != std::filesystem::perms::none);
    write = isLocked(path) ? false : ((perms & std::filesystem::perms::owner_write) != std::filesystem::perms::none);
    exec = ((perms & std::filesystem::perms::owner_exec) != std::filesystem::perms::none);
    return true;
}

}  // namespace KDC
