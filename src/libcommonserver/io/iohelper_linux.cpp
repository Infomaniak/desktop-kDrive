/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "io/iohelper.h"

#include "io/filestat.h"
#include "log/log.h"
#include "libcommonserver/utility/utility.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <filesystem>
#include <mntent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <sys/time.h>

#include <log4cplus/loggingmacros.h>

#include <Poco/File.h>
#include "utility/utility.h"

namespace KDC {

bool IoHelper::checkIfFileIsDehydrated(const SyncPath &itemPath, bool &isDehydrated, IoError &ioError) noexcept {
    (void) (itemPath);

    isDehydrated = false;
    ioError = IoError::Success;

    return true;
}

bool IoHelper::_getFileStatFn(const SyncPath &path, FileStat *buf, IoError &ioError) noexcept {
    ioError = IoError::Success;

    struct statx sb;

    if (statx(AT_FDCWD, path.string().c_str(), AT_SYMLINK_NOFOLLOW, STATX_BASIC_STATS | STATX_BTIME, &sb) < 0) {
        ioError = posixError2ioError(errno);
        return isExpectedError(ioError);
    }

    if (!_checkIfIsHiddenFile(path, buf->isHidden, ioError)) {
        return false;
    }

    buf->inode = sb.stx_ino;

    /* Retrieve creation time
     * On Linux, some file systems do not support creation time (btime).
     * If this is the case, this function will try to get the creation time from an extended attribute previously set.
     * If the extended attribute is not set, it will be set with the current ctime of the file.
     * If the file system does not support extended attributes, a warning will be logged and the creation time will be set to the
     * ctime of the file.
     */
    buf->creationTime = sb.stx_ctime.tv_sec;
    if (sb.stx_mask & STATX_BTIME) {
        buf->creationTime = sb.stx_btime.tv_sec; // The file system supports creation time
    } else if (static const auto btimeXattrName = "user.kDrive.birthtime";
               lgetxattr(path.string().c_str(), btimeXattrName, &buf->creationTime, sizeof(buf->creationTime)) < 0) {
        if (const auto err = errno; err == ENODATA) {
            if (lsetxattr(path.string().c_str(), btimeXattrName, &buf->creationTime, sizeof(buf->creationTime), 0) < 0) {
                LOG_ERROR(logger(), "Failed to set user.kDrive.birthtime extended attribute: " << strerror(errno));
            }
        } else if (err == ENOTSUP) {
            if (!_unsuportedFSLogged) {
                LOG_ERROR(logger(), "The file system does not support extended attributes: " << strerror(errno));
                sentry::Handler::captureMessage(sentry::Level::Warning, "Unsupported file system",
                                                "The file system does not support neither creation time nor extended attributes");
                _unsuportedFSLogged = true;
            }
        } else {
            LOG_ERROR(logger(), "Failed to get 'user.kDrive.birthtime' extended attribute: " << strerror(errno));
        }
    }

    buf->modificationTime = sb.stx_mtime.tv_sec;
    buf->size = static_cast<int64_t>(sb.stx_size);
    buf->nodeType = S_ISDIR(sb.stx_mode) ? NodeType::Directory : NodeType::File;

    if (S_ISLNK(sb.stx_mode)) buf->nodeType = getTargetNodeType(path);

    return true;
}

IoError IoHelper::setFileDates(const SyncPath &filePath, const SyncTime /*creationDate*/, const SyncTime modificationDate,
                               const bool) noexcept {
    // /!\ It is not possible to update the Birth date of a file on Linux
    struct timeval times[2];
    times[0].tv_sec = modificationDate; // Access date
    times[0].tv_usec = 0;
    times[1].tv_sec = modificationDate; // Modify date
    times[1].tv_usec = 0;
    if (int rc = lutimes(filePath.c_str(), times); rc != 0) {
        return posixError2ioError(errno);
    }

    return IoError::Success;
}

IoError IoHelper::lock(const SyncPath &) noexcept {
    return IoError::Success; // Only on macOS
}

IoError IoHelper::unlock(const SyncPath &) noexcept {
    return IoError::Success; // Only on macOS
}

IoError IoHelper::isLocked(const SyncPath &, bool &locked) noexcept {
    locked = false;
    return IoError::Success; // Only on macOS
}

bool IoHelper::moveItemToTrash(const SyncPath &itemPath) {
    std::string desktopType;
    if (!Utility::getLinuxDesktopType(desktopType)) {
        desktopType = "GNOME";
    }

    std::string command;
    if (desktopType == "GNOME") {
        command = "gio trash \"" + itemPath.string() + "\"";
    } else if (desktopType == "KDE") {
        command = "kioclient move \"" + itemPath.string() + "\" trash:/files/";
    } else {
        // Not implemented for the others distros
        return true;
    }

    const SyncPath trashPath = Utility::getTrashPath();

    // Check if the trash/files & trash/info path exists and create it if needed
    if (std::error_code ec; !std::filesystem::exists(trashPath, ec)) {
        if (ec) {
            LOGW_WARN(Log::instance()->getLogger(), L"Error in std::filesystem::exists - " << Utility::formatStdError(ec));
            return false;
        }

        if (!std::filesystem::create_directories(trashPath)) {
            LOGW_WARN(Log::instance()->getLogger(), L"Failed to create directory - " << Utility::formatSyncPath(trashPath));
            return false;
        }
    }

    if (const auto result = system(command.c_str()); result) {
        LOG_WARN(Log::instance()->getLogger(), "Failed to move item to trash - err=" << std::to_string(result));
        return false;
    }

    return true;
}

bool IoHelper::isPathOnMountedDisk(const SyncPath &path, bool &isMounted, IoError &ioError) noexcept {
    isMounted = false;
    ioError = IoError::Success;
    std::error_code ec;
    std::string absPath = std::filesystem::absolute(path, ec).string();
    if (ec) {
        ioError = IoHelper::stdError2ioError(ec);
        LOGW_WARN(logger(), L"Error in std::filesystem::absolute - " << Utility::formatStdError(ec));
        return false;
    }

    FILE *mtab = setmntent("/proc/mounts", "r");
    if (!mtab) {
        ioError = posixError2ioError(errno);
        LOGW_WARN(logger(), L"Error in setmntent: " << Utility::formatIoError(path, ioError));
        return false;
    }

    struct mntent *ent = nullptr;
    size_t bestMatchLen = 0;
    std::string bestMount;

    while ((ent = getmntent(mtab)) != nullptr) {
        std::string mountPoint = ent->mnt_dir;
        bool matches = false;
        if (mountPoint == "/" || (absPath.compare(0, mountPoint.size(), mountPoint) == 0 &&
                   (absPath.size() == mountPoint.size() || absPath[mountPoint.size()] == '/'))) {
            matches = true;
        }

        if (matches && mountPoint.size() > bestMatchLen) {
            bestMatchLen = mountPoint.size();
            bestMount = std::move(mountPoint);
        }
    }

    if (endmntent(mtab) != 1) {
        LOGW_WARN(logger(), L"Error in endmntent: " << Utility::formatSyncPath(path));
    }

    if (bestMatchLen == 0) {
        isMounted = false;
        return true;
    }

    if (bestMount == "/" &&
        (absPath.starts_with("/mnt/") || absPath.starts_with("/media/") || absPath.starts_with("/run/media/"))) {
        isMounted = false;
        return true;
    }

    isMounted = true;
    return true;
}
} // namespace KDC
