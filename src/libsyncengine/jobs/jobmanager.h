
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
#include "libcommon/utility/utility.h"

#include <log4cplus/logger.h>

#include <Poco/Thread.h>
#include <Poco/ThreadPool.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/ThreadPool.h>

#include <list>
#include <queue>
#include <thread>
#include <unordered_map>
#include <unordered_set>

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

class JobManager {
    public:
        static std::shared_ptr<JobManager> instance() noexcept;

        JobManager(JobManager const &) = delete;
        void operator=(JobManager const &) = delete;

        void stop();
        void clear();
        void reset();

        /**
         * @brief Queue a job to be executed as soon as a thread is available in the default thread pool.
         * @param job The job to be run asynchronously.
         * @param priority Job's priority level.
         * @param externalCallback Callback to be run once the job is done.
         */
        void queueAsyncJob(std::shared_ptr<AbstractJob> job,
                           Poco::Thread::Priority priority = Poco::Thread::PRIO_NORMAL) noexcept;

        void eraseJob(UniqueId jobId);

        bool isJobFinished(const UniqueId &jobId) const;

        std::shared_ptr<AbstractJob> getJob(const UniqueId &jobId);
        inline size_t countManagedJobs() const { return _managedJobs.size(); }
        inline int maxNbThreads() const { return _maxNbThread; }

        void setPoolCapacity(int nbThread);
        void decreasePoolCapacity();

    private:
        static void executeFunc(void *thisWorker);

        JobManager();
        void start();

        void run() noexcept;
        void startJob(std::shared_ptr<AbstractJob> job, Poco::Thread::Priority priority);
        void addToPendingJobs(std::shared_ptr<AbstractJob> job, Poco::Thread::Priority priority);
        void adjustMaxNbThread();
        void managePendingJobs();

        int availableThreadsInPool() const;
        bool canRunjob(const std::shared_ptr<AbstractJob> job) const;
        bool isBigFileDownloadJob(const std::shared_ptr<AbstractJob> job) const;
        bool isBigFileUploadJob(const std::shared_ptr<AbstractJob> job) const;

        static std::shared_ptr<JobManager> _instance;
        bool _stop{false};
        int _maxNbThread{0};

        log4cplus::Logger _logger;
        std::unique_ptr<std::thread> _thread;

        std::unordered_map<UniqueId, std::shared_ptr<AbstractJob>> _managedJobs; // queued + running + pending jobs.
        std::priority_queue<std::pair<std::shared_ptr<AbstractJob>, Poco::Thread::Priority>,
                            std::vector<std::pair<std::shared_ptr<AbstractJob>, Poco::Thread::Priority>>, JobPriorityCmp>
                _queuedJobs; // jobs waiting for an available thread.
        std::unordered_set<UniqueId> _runningJobs; // jobs currently running in a dedicated thread.
        std::unordered_map<UniqueId, std::pair<std::shared_ptr<AbstractJob>, Poco::Thread::Priority>>
                _pendingJobs; // jobs waiting to be able to start.
        mutable std::mutex _mutex;

        UniqueId _uploadSessionJobId{0};

        friend class TestJobManager;
};

} // namespace KDC
