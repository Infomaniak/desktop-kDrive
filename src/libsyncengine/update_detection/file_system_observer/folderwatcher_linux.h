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

#pragma once

#include "folderwatcher.h"

#include <map>

struct inotify_event;

namespace KDC {

class LocalFileSystemObserverWorker;

class FolderWatcher_linux : public FolderWatcher {
    public:
        FolderWatcher_linux(LocalFileSystemObserverWorker *parent, const SyncPath &path);

        void startWatching() override;
        void stopWatching() override;

    private:
        std::int64_t _fileDescriptor = -1;

        bool findSubFolders(const SyncPath &dir, std::list<SyncPath> &fullList);
        ExitInfo inotifyRegisterPath(const SyncPath &path);
        ExitInfo addFolderRecursive(const SyncPath &path);
        void removeFoldersBelow(const SyncPath &dirPath);
        struct AddWatchOutCome {
                std::int64_t returnValue{0};
                std::int64_t errorNumber{0};
        };
        virtual AddWatchOutCome inotifyAddWatch(const SyncPath &path);

        ExitInfo changeDetected(const SyncPath &path, OperationType opType) const;

        std::unordered_map<int, SyncPath> _watchToPath;
        std::map<std::string, int> _pathToWatch;

        static SyncPath makeSyncPath(const SyncPath &path, const char *name);

        friend class TestFolderWatcherLinux;
};

} // namespace KDC
