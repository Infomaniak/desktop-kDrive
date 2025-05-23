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

#include <log4cplus/loggingmacros.h>

#include <fcntl.h>
#include <Poco/File.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/xattr.h>
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
                sentry::Handler::captureMessage(sentry::Level::Warning, "Unsuported file system",
                                                "The file system does not support neither creation time nor extended attributes");
                _unsuportedFSLogged = true;
            }
        } else {
            LOG_ERROR(logger(), "Failed to get user.kDrive.birthtime extended attribute: " << strerror(errno));
        }
    }

    buf->modtime = sb.stx_mtime.tv_sec;
    buf->size = static_cast<int64_t>(sb.stx_size);
    if (S_ISLNK(sb.stx_mode)) {
        // The item is a symlink.
        struct stat sbTarget;
        if (stat(path.string().c_str(), &sbTarget) < 0) {
            // Cannot access target => undetermined
            buf->nodeType = NodeType::Unknown;
        } else {
            buf->nodeType = S_ISDIR(sbTarget.st_mode) ? NodeType::Directory : NodeType::File;
        }
    } else {
        buf->nodeType = S_ISDIR(sb.stx_mode) ? NodeType::Directory : NodeType::File;
    }

    return true;
}

IoError returnNoSuchFileOrDirectory(const SyncPath &filePath) {
    LOGW_WARN(Log::instance()->getLogger(), L"File not found : " << Utility::formatSyncPath(filePath));
    return IoError::NoSuchFileOrDirectory;
}

IoError returnAccessDenied(const SyncPath &filePath) {
    LOGW_WARN(Log::instance()->getLogger(), L"Access denied on file : " << Utility::formatSyncPath(filePath));
    return IoError::AccessDenied;
}

IoError IoHelper::setFileDates(const SyncPath &filePath, const SyncTime ,
                                 const SyncTime modificationDate, const bool ) noexcept {
    try {
        const Poco::Timestamp lastModifiedTimestamp(Poco::Timestamp::fromEpochTime(modificationDate));
        Poco::File(Path2Str(filePath)).setLastModified(lastModifiedTimestamp);
    }
    catch (Poco::NotFoundException &) {
        return returnNoSuchFileOrDirectory(filePath);
    }
    catch (Poco::FileNotFoundException &) {
        return returnNoSuchFileOrDirectory(filePath);
    }
    catch (Poco::FileExistsException &) {
        return returnNoSuchFileOrDirectory(filePath);
    }
    catch (Poco::NoPermissionException &) {
        return returnAccessDenied(filePath);
    }
    catch (Poco::FileAccessDeniedException &) {
        return returnAccessDenied(filePath);
    }
    catch (Poco::Exception &ex) {
        LOG_WARN(Log::instance()->getLogger(), "Error in setLastModified : " << ex.message() << " (" << ex.code() << ")");
        return IoError::Unknown;
    }

    return IoError::Success;
}

} // namespace KDC
