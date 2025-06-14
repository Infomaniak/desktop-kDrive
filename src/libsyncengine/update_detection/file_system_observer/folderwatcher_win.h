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

#include <windows.h>

#include <thread>
#include <mutex>

namespace KDC {

class LocalFileSystemObserverWorker;

class FolderWatcher_win : public FolderWatcher {
    public:
        FolderWatcher_win(LocalFileSystemObserverWorker *parent, const SyncPath &path);

        void changesLost();
        ExitInfo changeDetected(const SyncPath &path, OperationType opType);

    protected:
        void startWatching() override;
        void stopWatching() override;

    private:
        HANDLE _directoryHandle = nullptr;
        HANDLE _resultEventHandle = nullptr;
        HANDLE _stopEventHandle = nullptr;

        void watchChanges();
        void closeHandle();

        OperationType operationFromAction(DWORD action);
};

} // namespace KDC
