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

#include "filesystemobserverworker.h"
#include "update_detection/file_system_observer/folderwatcher.h"

namespace KDC {

class LocalFileSystemObserverWorker : public FileSystemObserverWorker {
    public:
        LocalFileSystemObserverWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName);
        ~LocalFileSystemObserverWorker() override;

        void start() override;
        void stop() override;

        virtual ExitInfo changesDetected(const std::list<std::pair<std::filesystem::path, OperationType>> &changes);
        virtual void forceUpdate() override;

    protected:
        virtual void execute() override;

        ExitInfo exploreDir(const SyncPath &absoluteParentDirPath, bool fromChangeDetected = false);

        SyncPath _rootFolder;
        //    std::unique_ptr<ContentChecksumWorker> _checksumWorker = nullptr;
        std::unique_ptr<FolderWatcher> _folderWatcher = nullptr;

    private:
        virtual ExitInfo generateInitialSnapshot() override;
        virtual ReplicaSide getSnapshotType() const override { return ReplicaSide::Local; }

        bool canComputeChecksum(const SyncPath &absolutePath);

#if defined(KD_MACOS)
        ExitCode isEditValid(const NodeId &nodeId, const SyncPath &path, SyncTime lastModifiedLocal, bool &valid) const;
#endif

        void sendAccessDeniedError(const SyncPath &absolutePath);

        std::chrono::steady_clock::time_point _needUpdateTimerStart = std::chrono::steady_clock::now();

        std::recursive_mutex _recursiveMutex;

        friend class TestLocalFileSystemObserverWorker;
};

} // namespace KDC
