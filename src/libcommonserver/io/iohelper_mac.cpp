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

static const std::string EXT_ATTR_STATUS = "com.infomaniak.drive.desktopclient.litesync.status";
static const std::string EXT_ATTR_PIN_STATE = "com.infomaniak.drive.desktopclient.litesync.pinstate";


namespace {
inline bool _isXAttrValueExpectedError(IoError error) {
    return (error == IoError::NoSuchFileOrDirectory) || (error == IoError::AttrNotFound) || (error == IoError::AccessDenied);
}
} // namespace

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
        const ssize_t bufferLength =
                getxattr(path.native().c_str(), attrName.c_str(), NULL, 0, 0, isSymlink ? XATTR_NOFOLLOW : 0);
        if (bufferLength == -1) {
            ioError = posixError2ioError(errno);
            return _isXAttrValueExpectedError(ioError);
        }

        value.resize(static_cast<size_t>(bufferLength));
        if (getxattr(path.native().c_str(), attrName.c_str(), value.data(), static_cast<size_t>(bufferLength), 0,
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

bool IoHelper::removeXAttrs(const SyncPath &path, const std::vector<std::string> &attrNames, IoError &ioError) noexcept {
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

    for (const auto &attrName: attrNames) {
        if (removexattr(path.native().c_str(), attrName.c_str(), isSymlink ? XATTR_NOFOLLOW : 0) == -1) {
            ioError = posixError2ioError(errno);
            return _isXAttrValueExpectedError(ioError);
        }
    }

    // XAttr has been removed
    ioError = IoError::Success;
    return true;
}

bool IoHelper::removeLiteSyncXAttrs(const SyncPath &path, IoError &ioError) noexcept {
    const std::vector<std::string> liteSyncAttrName = {EXT_ATTR_STATUS, EXT_ATTR_PIN_STATE};
    return removeXAttrs(path, liteSyncAttrName, ioError);
}

bool IoHelper::checkIfFileIsDehydrated(const SyncPath &itemPath, bool &isDehydrated, IoError &ioError) noexcept {
    isDehydrated = false;
    ioError = IoError::Success;

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

} // namespace KDC
