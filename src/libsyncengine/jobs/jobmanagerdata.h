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

#include "abstractjob.h"

#include <Poco/Thread.h>

#include <memory>

namespace KDC {

class JobPriorityCmp {
    public:
        bool operator()(const std::pair<std::shared_ptr<AbstractJob>, Poco::Thread::Priority> &j1,
                        const std::pair<std::shared_ptr<AbstractJob>, Poco::Thread::Priority> &j2) const {
            if (j1.second == j2.second) {
                // Same thread priority, use the job ID to define priority
                return j1.first->jobId() > j2.first->jobId();
            }
            return j1.second < j2.second;
        }
};

/**
 * @brief A thread safe class to access JobManager data.
 */
class JobManagerData {
    public:
        void queue(std::shared_ptr<AbstractJob> job, Poco::Thread::Priority priority = Poco::Thread::PRIO_NORMAL);
        std::pair<std::shared_ptr<AbstractJob>, Poco::Thread::Priority> pop();
        bool hasQueuedJob() const;
        bool hasHighestPriorityJob() const;
        bool isManaged(const UniqueId jobId) const;

        bool addToRunningJobs(const UniqueId jobId);

        bool addToPendingJobs(const UniqueId jobId, std::shared_ptr<AbstractJob> job,
                              Poco::Thread::Priority priority = Poco::Thread::PRIO_NORMAL);
        void removeFromPendingJobs(const UniqueId jobId);

        void erase(const UniqueId jobId);

        /**
         * @brief Get the IDs of the currently running jobs. A copy of the list is returned in order to avoid concurrency
         * issues while using the returned list.
         * @return A copy of the list of IDs of the running jobs.
         */
        std::unordered_set<UniqueId> runningJobs() const;
        /**
         * @brief
         * @return A copy of the list of pending jobs..
         */
        const std::unordered_map<UniqueId, std::pair<std::shared_ptr<AbstractJob>, Poco::Thread::Priority>> pendingJobs() const;

        std::shared_ptr<AbstractJob> getJob(const UniqueId &jobId) const;


        void clear();

    private:
        std::unordered_map<UniqueId, std::shared_ptr<AbstractJob>> _managedJobs; // queued + running + pending jobs.
        std::priority_queue<std::pair<std::shared_ptr<AbstractJob>, Poco::Thread::Priority>,
                            std::vector<std::pair<std::shared_ptr<AbstractJob>, Poco::Thread::Priority>>, JobPriorityCmp>
                _queuedJobs; // jobs waiting for an available thread.
        std::unordered_set<UniqueId> _runningJobs; // jobs currently running in a dedicated thread.
        std::unordered_map<UniqueId, std::pair<std::shared_ptr<AbstractJob>, Poco::Thread::Priority>>
                _pendingJobs; // jobs waiting to be able to start.
        mutable std::mutex _mutex;

        friend class TestJobManager;
};
} // namespace KDC
