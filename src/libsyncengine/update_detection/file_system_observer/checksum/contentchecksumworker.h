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
#include "utility/types.h"
#include "computechecksumjob.h"
#include "update_detection/file_system_observer/snapshot/snapshot.h"

#include <Poco/ThreadPool.h>

#include <list>
#include <queue>

namespace KDC {

class ComputeChecksumJob;

// TODO : this worker should have only 1 instance (and 1 thread) for the whole app, not one instance per sync

class ContentChecksumWorker : public ISyncWorker {
    public:
        ContentChecksumWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName,
                              std::shared_ptr<Snapshot> localSnapshot);
        virtual ~ContentChecksumWorker();

        void computeChecksum(const NodeId &id, const SyncPath &file);
        static void callback(UniqueId jobId);

    protected:
        virtual void execute() override;

    private:
        std::shared_ptr<Snapshot> _localSnapshot;
        std::queue<std::pair<NodeId, SyncPath>> _toCompute;

        Poco::ThreadPool _threadPool;

        static std::mutex _checksumMutex;
        static std::unordered_map<UniqueId, std::shared_ptr<ComputeChecksumJob>> _runningJobs;
};

} // namespace KDC
