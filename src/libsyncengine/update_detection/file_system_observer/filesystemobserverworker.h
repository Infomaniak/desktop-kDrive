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

#include "syncpal/isyncworker.h"
#include "syncpal/syncpal.h"
#include "snapshot/snapshot.h"
#include "db/syncdb.h"
#include "libcommon/utility/types.h"

#include <list>

namespace KDC {

class FileSystemObserverWorker : public ISyncWorker {
    public:
        FileSystemObserverWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName,
                                 ReplicaSide side);
        ~FileSystemObserverWorker();

        void invalidateSnapshot();
        virtual void forceUpdate();
        virtual inline bool updating() const { return _updating; }

        std::shared_ptr<Snapshot> snapshot() const { return _snapshot; };

    protected:
        std::shared_ptr<SyncDb> _syncDb;
        std::shared_ptr<Snapshot> _snapshot;

        std::list<std::pair<SyncPath, OperationType>> _pendingFileEvents;

        bool _updating = false;
        std::mutex _mutex;

        virtual ExitCode generateInitialSnapshot() = 0;
        virtual ExitCode processEvents() { return ExitCode::Ok; }

        virtual bool isFolderWatcherReliable() const { return true; }

    private:
        static void *executeFunc(void *thisWorker);
        virtual ReplicaSide getSnapshotType() const = 0;

        friend class TestLocalFileSystemObserverWorker;
        friend class TestRemoteFileSystemObserverWorker;
};

}  // namespace KDC
