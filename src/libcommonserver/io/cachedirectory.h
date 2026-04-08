/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 ** This program is free software: you can redistribute it and/or modify
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

namespace KDC {

class CacheDirectory {
    public:
        explicit CacheDirectory(const SyncPath &localSyncPath);
        ~CacheDirectory();

        const SyncPath &path() noexcept;
        void deleteDirectory() const noexcept;

        void setDeleteFolderUponDestruction(const bool deleteFolderUponDestruction) {
            _deleteFolderUponDestruction = deleteFolderUponDestruction;
        }

    private:
        void initDirectory(const SyncPath &localSyncPath) noexcept;
        void resetDirectoryPath() noexcept;

        mutable std::mutex _mutex;
        SyncPath _cacheDirectoryPath;
        bool _deleteFolderUponDestruction{false};
};

} // namespace KDC
