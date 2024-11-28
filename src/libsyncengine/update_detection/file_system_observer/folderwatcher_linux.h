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

#include "folderwatcher.h"

#include <map>

struct inotify_event;

namespace KDC {

class LocalFileSystemObserverWorker;

class FolderWatcher_linux : public FolderWatcher {
    public:
        FolderWatcher_linux(LocalFileSystemObserverWorker *parent, const SyncPath &path);
        ~FolderWatcher_linux();

        void startWatching() override;
        void stopWatching() override;

        /// On linux the watcher is ready when the ctor finished.
        bool _ready = true;

    private:
        int _fileDescriptor = -1;

        bool findSubFolders(const SyncPath &dir, std::list<SyncPath> &fullList);
        bool inotifyRegisterPath(const SyncPath &path);
        bool addFolderRecursive(const SyncPath &path);
        void removeFoldersBelow(const SyncPath &dirPath);


        void changeDetected(const SyncPath &path, OperationType opType);

        std::unordered_map<int, SyncPath> _watchToPath;
        std::map<std::string, int> _pathToWatch;

        static SyncPath makeSyncPath(const SyncPath &path, const char *name);

        friend class TestFolderWatcherLinux;
};

} // namespace KDC
