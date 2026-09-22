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

#pragma once

#include "libcommon/utility/types.h"

#include <mutex>
#include <string_view>

namespace KDC {

class CacheDirectory {
    public:
        explicit CacheDirectory(const SyncPath &localSyncPath);
        ~CacheDirectory();

        /**
         * @brief Returns the path to the cache directory. If the cache directory does not exist, it will be created.
         * @param cacheDirectory the path to the cache directory
         * @return an ExitInfo indicating the success or failure of the operation
         */
        ExitInfo path(SyncPath &cacheDirectory) noexcept;
        /**
         * @brief Returns the standard name of the cache directory.
         * @return the name of the cache directory
         */
        static std::string_view name() noexcept;

        /**
         * @brief Returns the path to the root directory of the sync. This is the directory that contains the cache directory.
         * @return the path to the root directory of the sync
         */
        [[nodiscard]] const SyncPath &syncRootDirectoryPath() const noexcept;

        // Shared naming contract used by creators and cleanup logic.
        static std::string createTmpFileName();
        static bool isTmpFileName(std::string_view fileName) noexcept;

    private:
        static constexpr std::string_view tmpFilePrefix = "kdrive_";
        static constexpr uint32_t tmpFileRandomPartLength = 10;

        ExitInfo initDirectory() noexcept;
        void cleanUp() const;

        const SyncPath _syncRootDirectoryPath;

        mutable std::mutex _mutex;
        SyncPath _cacheDirectoryPath;
};

} // namespace KDC
