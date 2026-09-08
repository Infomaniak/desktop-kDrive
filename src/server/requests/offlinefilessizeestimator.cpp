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

#include "offlinefilessizeestimator.h"

#include "io/iohelper.h"
#include "syncpal/syncpal.h"
#include "vfs/vfs.h"

namespace KDC {

OfflineFilesSizeEstimator::OfflineFilesSizeEstimator(const std::vector<std::shared_ptr<SyncPal>> &syncPals) :
    _syncPals(syncPals) {}

ExitInfo OfflineFilesSizeEstimator::runSynchronously() {
    _offlineFilesTotalSize = 0;

    for (const auto &syncPal: _syncPals) {
        const auto basePath = syncPal->localPath();
        IoError ioError = IoError::Unknown;
        IoHelper::DirectoryIterator dirIt(basePath, true, ioError);
        DirectoryEntry entry;
        bool endOfDir = false;
        while (dirIt.next(entry, endOfDir, ioError) && !endOfDir) {
            std::error_code ec;
            const auto isSymlink = entry.is_symlink(ec);
            if (ec.value()) {
                LOGW_WARN(Log::instance()->getLogger(),
                          L"Error in std::filesystem::directory_entry::is_symlink " << Utility::formatStdError(entry.path(), ec));
                continue; // We simply ignore the file.
            }

            const auto isDirectory = entry.is_directory(ec);
            if (ec.value()) {
                LOGW_WARN(Log::instance()->getLogger(), L"Error in std::filesystem::directory_entry::is_directory "
                                                                << Utility::formatStdError(entry.path(), ec));
                continue; // We simply ignore the file.
            }

            if (isSymlink || isDirectory) continue;

            VfsStatus vfsStatus;
            if (const auto exitInfo = syncPal->vfs()->status(entry.path(), vfsStatus); !exitInfo) {
                LOGW_WARN(Log::instance()->getLogger(),
                          L"Failed to get VFS status for file " << Utility::formatSyncPath(entry.path()));
                continue; // We simply ignore the file.
            }
            if (vfsStatus.isPlaceholder && !vfsStatus.isHydrated) continue;

            uint64_t entrySize = 0;
            if (!IoHelper::getFileSize(entry.path(), entrySize, ioError) || ioError != IoError::Success) {
                LOGW_WARN(Log::instance()->getLogger(), L"Error in IoHelper::getFileSize for "
                                                                << Utility::formatSyncPath(entry.path()) << L" - "
                                                                << Utility::formatIoError(ioError));
                continue; // We simply ignore the file.
            }

            _offlineFilesTotalSize += entrySize;
        }

        if (ioError != IoError::Success) {
            LOGW_WARN(Log::instance()->getLogger(),
                      L"Error in DirectoryIterator for " << Utility::formatIoError(basePath, ioError));
            return IoHelper::directoryIteratorExitCode(ioError);
        }
    }
    return ExitCode::Ok;
}

} // namespace KDC
