
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
        static JobManager *instance();
        static void stop();
        static void clear();

        /*
         * Queue a job to be executed as soon as a thread is available in the default thread pool
         * If a callback is passed as argument: JobManager will NOT keep a reference to the job. The caller MUST keep a reference
         * to this job otherwise it could be deleted before or during its execution. Otherwise: JobManager will keep a reference
         * to the job until it is finished. The caller should not keep a reference to this job.
         */
        void queueAsyncJob(std::shared_ptr<AbstractJob> job, Poco::Thread::Priority priority = Poco::Thread::PRIO_NORMAL,
                           std::function<void(UniqueId)> externalCallback = nullptr) noexcept;

        inline bool hasAvailableThread() { return Poco::ThreadPool::defaultPool().available() > 0; }
        bool isJobFinished(const UniqueId &jobId);

        std::shared_ptr<AbstractJob> getJob(const UniqueId &jobId);
        inline size_t countManagedJobs() { return _managedJobs.size(); }
        inline int maxNbThreads() { return _maxNbThread; }

        void setPoolCapacity(int count); // For testing purpose
        void decreasePoolCapacity();

    private:
        JobManager();

        static void defaultCallback(UniqueId jobId);

        static void run() noexcept;
        static void startJob(std::pair<std::shared_ptr<AbstractJob>, Poco::Thread::Priority> nextJob);
        static void adjustMaxNbThread();
        static int countUploadSession();
        static bool canRun(const std::shared_ptr<AbstractJob> job, int uploadSessionCount);
        static void managePendingJobs(int uploadSessionCount);

        static bool isParentPendingOrRunning(UniqueId jobIb);
        static bool canStartJob(std::shared_ptr<AbstractJob> job, int uploadSessionCount);

        static JobManager *_instance;
        static bool _stop;
        static int _maxNbThread;
        static double _cpuUsageThreshold;
        static int _threadAdjustmentStep;
        static std::chrono::time_point<std::chrono::steady_clock> _maxNbThreadChrono;

        log4cplus::Logger _logger;
        std::unique_ptr<StdLoggingThread> _thread;

        static std::unordered_map<UniqueId, std::shared_ptr<AbstractJob>> _managedJobs; // queued + running + pending jobs
        static std::priority_queue<std::pair<std::shared_ptr<AbstractJob>, Poco::Thread::Priority>,
                                   std::vector<std::pair<std::shared_ptr<AbstractJob>, Poco::Thread::Priority>>, JobPriorityCmp>
                _queuedJobs; // jobs waiting for an available thread
        static std::unordered_set<UniqueId> _runningJobs; // jobs currently running in a dedicated thread
        static std::unordered_map<UniqueId, std::pair<std::shared_ptr<AbstractJob>, Poco::Thread::Priority>>
                _pendingJobs; // jobs waiting for their parent job to be completed
        static std::recursive_mutex _mutex;

        friend class TestJobManager;
};

} // namespace KDC
