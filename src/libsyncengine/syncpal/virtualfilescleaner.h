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

#pragma once

#include "utility/types.h"

#include <log4cplus/logger.h>

namespace KDC {

class SyncDb;

class VirtualFilesCleaner {
    public:
        VirtualFilesCleaner(const SyncPath &path, int syncDbId, std::shared_ptr<SyncDb> syncDb,
                            ExitInfo (*vfsStatus)(int, const SyncPath &, bool &, bool &, bool &, int &),
                            bool (*vfsClearFileAttributes)(int, const SyncPath &));

        VirtualFilesCleaner(const SyncPath &path, int syncDbId);

        bool run();
        bool removeDehydratedPlaceholders(std::vector<SyncPath> &failedToRemovePlaceholders);

        inline ExitCode exitCode() const { return _exitCode; }
        inline ExitCause exitCause() const { return _exitCause; }

    private:
        bool removePlaceholdersRecursively(const SyncPath &parentPath);
        bool recursiveDirectoryIterator(const SyncPath &path, std::filesystem::recursive_directory_iterator &dirIt);
        bool folderCanBeProcessed(std::filesystem::recursive_directory_iterator &dirIt);

        log4cplus::Logger _logger;

        SyncPath _rootPath;
        int _syncDbId{-1};
        std::shared_ptr<SyncDb> _syncDb = nullptr;
        ExitInfo (*_vfsStatus)(int syncDbId, const SyncPath &itemPath, bool &isPlaceholder, bool &isHydrated, bool &isSyncing,
                           int &progress) = nullptr;
        bool (*_vfsClearFileAttributes)(int syncDbId, const SyncPath &itemPath) = nullptr;

        ExitCode _exitCode = ExitCode::Unknown;
        ExitCause _exitCause = ExitCause::Unknown;
};

} // namespace KDC
