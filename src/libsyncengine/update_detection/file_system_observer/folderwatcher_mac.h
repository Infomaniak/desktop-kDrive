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

#include <CoreServices/CoreServices.h>

#include <mutex>

namespace KDC {

class LocalFileSystemObserverWorker;

class FolderWatcher_mac : public FolderWatcher {
    public:
        FolderWatcher_mac(LocalFileSystemObserverWorker *parent, const SyncPath &path);

        void startWatching() override;
        void stopWatching() override;

        void doNotifyParent(const std::list<std::pair<SyncPath, OperationType>> &changes);

        static OperationType getOpType(FSEventStreamEventFlags eventFlags);

        std::mutex _streamMutex;

    private:
        FSEventStreamRef _stream;
        CFRunLoopRef _ref;
};

} // namespace KDC
