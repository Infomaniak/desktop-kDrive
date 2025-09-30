
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

#include "syncjob.h"
#include "jobmanagerdata.h"
#include "libcommon/utility/utility.h"

#include <log4cplus/logger.h>

#include <Poco/Thread.h>
#include <Poco/ThreadPool.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/ThreadPool.h>

namespace KDC {

class JobManager {
    public:
        JobManager();
        JobManager(JobManager const &) = delete;
        void operator=(JobManager const &) = delete;

        void stop();
        void clear();

        /**
         * @brief Queue a job to be executed as soon as a thread is available in the default thread pool.
         * @param job The job to be run asynchronously.
         * @param priority Job's priority level.
         * @param externalCallback Callback to be run once the job is done.
         */
        void queueAsyncJob(std::shared_ptr<AbstractJob> job,
                           Poco::Thread::Priority priority = Poco::Thread::PRIO_NORMAL) noexcept;

        bool isJobFinished(const UniqueId jobId) const;
        std::shared_ptr<AbstractJob> getJob(const UniqueId jobId) const;

        void setPoolCapacity(int nbThread);
        void decreasePoolCapacity();

    protected:
        int availableThreadsInPool() const;
        virtual bool canRunJob(const std::shared_ptr<AbstractJob> job) const;
        JobManagerData _data;

    private:
        void startMainThreadIfNeeded();

        void run() noexcept;
        void startJob(std::shared_ptr<AbstractJob> job, Poco::Thread::Priority priority);
        void eraseJob(UniqueId jobId);
        void addToPendingJobs(std::shared_ptr<AbstractJob> job, Poco::Thread::Priority priority);
        void managePendingJobs();

        bool _stop{false};
        int _maxNbThread{0};
        Poco::ThreadPool _threadPool;

        log4cplus::Logger _logger;
        std::unique_ptr<std::thread> _mainThread;

        friend class TestSyncJobManagerSingleton;
};

} // namespace KDC
