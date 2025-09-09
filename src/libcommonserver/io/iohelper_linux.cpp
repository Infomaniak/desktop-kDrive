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

#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <sys/time.h>

#include <log4cplus/loggingmacros.h>

#include <Poco/File.h>

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
    if (sb.stx_mask & STATX_BTIME) {
        buf->creationTime = sb.stx_btime.tv_sec; // The file system supports creation time
    } else if (static const std::string btimeXattrName = "user.kDrive.birthtime";
               lgetxattr(path.string().c_str(), btimeXattrName.c_str(), &buf->creationTime, sizeof(buf->creationTime)) < 0) {
        buf->creationTime = sb.stx_ctime.tv_sec;
        if (const auto err = errno; err == ENODATA) {
            if (lsetxattr(path.string().c_str(), btimeXattrName.c_str(), &buf->creationTime, sizeof(buf->creationTime), 0) < 0) {
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

    if (S_ISLNK(sb.stx_mode)) setTargetNodeType(path, buf->nodeType);

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

} // namespace KDC
