
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
#include "jobmanagerdata.h"

#include "jobs/network/abstractnetworkjob.h"

#include "network/API_v2/downloadjob.h"
#include "network/API_v2/upload/upload_session/driveuploadsession.h"
#include "performance_watcher/performancewatcher.h"
#include "requests/parameterscache.h"

#include <log4cplus/loggingmacros.h>

#include "jobs/network/networkjobsparams.h"

#include "libcommon/utility/utility.h"
#include "libcommon/log/sentry/handler.h"

#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"

#include <Poco/Exception.h>
#include <Poco/Thread.h>
#include <Poco/ThreadPool.h>
#include <Poco/Net/HTTPSClientSession.h>

#include <algorithm> // std::max

namespace KDC {

template<class J>
class JobManager {
    public:
        static std::shared_ptr<JobManager> instance() noexcept;

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
        void queueAsyncJob(std::shared_ptr<J> job, Poco::Thread::Priority priority = Poco::Thread::PRIO_NORMAL) noexcept;

        bool isJobFinished(const UniqueId jobId) const;
        std::shared_ptr<J> getJob(const UniqueId jobId) const;

        void setPoolCapacity(int nbThread);
        void decreasePoolCapacity();

    private:
        JobManager();
        void startMainThreadIfNeeded();

        void run() noexcept;
        void startJob(std::shared_ptr<J> job, Poco::Thread::Priority priority);
        void eraseJob(UniqueId jobId);
        void addToPendingJobs(std::shared_ptr<J> job, Poco::Thread::Priority priority);
        void managePendingJobs();

        int availableThreadsInPool() const;
        bool canRunJob(const std::shared_ptr<J> job) const;

        static std::shared_ptr<JobManager> _instance;
        bool _stop{false};
        int _maxNbThread{0};

        log4cplus::Logger _logger{Log::instance()->getLogger()};
        std::unique_ptr<std::thread> _mainThread;

        JobManagerData<J> _data;

        friend class TestJobManager;
};

template<class J>
std::shared_ptr<JobManager<J>> JobManager<J>::_instance = nullptr;

template<class J>
std::shared_ptr<JobManager<J>> JobManager<J>::instance() noexcept {
    if (_instance == nullptr) {
        try {
            _instance = std::shared_ptr<JobManager<J>>(new JobManager<J>());
        } catch (...) {
            return nullptr;
        }
    }

    return _instance;
}

template<class J>
void JobManager<J>::startMainThreadIfNeeded() {
    if (!_mainThread) {
        const std::function<void()> runFunction = std::bind_front(&JobManager<J>::run, this);
        _mainThread = std::make_unique<std::thread>(runFunction);
    }
}

template<class J>
void JobManager<J>::stop() {
    _stop = true;
}

template<class J>
void JobManager<J>::clear() {
    Poco::ThreadPool::defaultPool().stopAll();
    if (_mainThread) {
        if (_mainThread->joinable()) _mainThread->join();
        _mainThread = nullptr;
    }

    _data.clear();
    _stop = false;

    if (_instance) {
        _instance.reset();
    }
}

template<class J>
void JobManager<J>::queueAsyncJob(const std::shared_ptr<J> job,
                                  const Poco::Thread::Priority priority /*= Poco::Thread::PRIO_NORMAL*/) noexcept {
    startMainThreadIfNeeded();
    const std::function<void(const UniqueId)> callback = std::bind_front(&JobManager<J>::eraseJob, this);
    job->setMainCallback(callback);
    _data.queue(job, priority);
}

template<class J>
bool JobManager<J>::isJobFinished(const UniqueId jobId) const {
    return !_data.isManaged(jobId);
}

template<class J>
std::shared_ptr<J> JobManager<J>::getJob(const UniqueId jobId) const {
    return _data.getJob(jobId);
}

template<class J>
void JobManager<J>::setPoolCapacity(const int nbThread) {
    // Poco::ThreadPool throws an exception if the capacity is set to a
    // value less than then minimum capacity (2 by default).
    static_assert(threadPoolMinCapacity >= 2 && "Thread pool min capacity is too low.");

    _maxNbThread = std::max(nbThread, threadPoolMinCapacity);
    Poco::ThreadPool::defaultPool().addCapacity(_maxNbThread - Poco::ThreadPool::defaultPool().capacity());
    LOG_DEBUG(_logger, "Max number of thread changed to " << _maxNbThread << " threads");
}

template<class J>
void JobManager<J>::decreasePoolCapacity() {
    if (_maxNbThread > threadPoolMinCapacity) {
        // Divide the maximum number of threads by 2 (rounded up) on each call.
        _maxNbThread -= static_cast<int>(std::ceil((_maxNbThread - threadPoolMinCapacity) / 2.0));
        setPoolCapacity(_maxNbThread);
    } else {
        const std::string jobManagerClassName = CommonUtility::getTypeName(*this);
        const std::string methodName = jobManagerClassName + "::decreasePoolCapacity";
        const std::string message = jobManagerClassName + " capacity cannot be decreased";
        sentry::Handler::captureMessage(sentry::Level::Warning, methodName, message);
    }
}

template<class J>
JobManager<J>::JobManager() {
    setPoolCapacity(std::min(static_cast<int>(std::thread::hardware_concurrency()), threadPoolMaxCapacity));

    LOG_DEBUG(_logger, "Job Manager started with max " << _maxNbThread << " threads");
}

template<class J>
void JobManager<J>::run() noexcept {
    while (true) {
        if (_stop) {
            break;
        }

        auto availableThreads = availableThreadsInPool();
        // Always keep 1 thread available for jobs with highest priority
        while (availableThreads > 1 && !_stop && _data.hasQueuedJob()) {
            const auto [job, priority] = _data.pop();
            if (canRunJob(job)) {
                startJob(job, priority);
            } else {
                addToPendingJobs(job, priority);
            }

            availableThreads = availableThreadsInPool();
        }

        // Start job with highest priority
        if (_data.hasHighestPriorityJob()) {
            const auto [job, priority] = _data.pop();
            startJob(job, priority);
        }

        managePendingJobs();

        Utility::msleep(100); // Sleep for 0.1 s
    }
}

template<class J>
void JobManager<J>::startJob(std::shared_ptr<J> job, Poco::Thread::Priority priority) {
    try {
        if (job->isAborted()) {
            LOG_DEBUG(_logger, "Job " << job->jobId() << " has been canceled");
            _data.erase(job->jobId());
        } else {
            LOG_DEBUG(_logger, "Starting job " << job->jobId() << " with priority " << priority);
            Poco::ThreadPool::defaultPool().startWithPriority(priority, *job);
            if (!_data.addToRunningJobs(job->jobId())) {
                LOG_WARN(_logger, "Failed to insert job " << job->jobId() << " in _runningJobs map");
            }
        }
    } catch (Poco::NoThreadAvailableException &) {
        LOG_DEBUG(_logger, "No more thread available, job " << job->jobId() << " queued");
        _data.queue(job, priority);
    } catch (Poco::Exception &e) {
        LOG_WARN(_logger, "Failed to start job: " << e.what());
    }
}

template<class J>
void JobManager<J>::eraseJob(const UniqueId jobId) {
    _data.erase(jobId);
}

template<class J>
void JobManager<J>::addToPendingJobs(const std::shared_ptr<J> job, const Poco::Thread::Priority priority) {
    if (!_data.addToPendingJobs(job->jobId(), job, priority)) {
        LOG_ERROR(_logger, "Failed to insert job " << job->jobId() << " in pending jobs list!");
        return;
    }
    LOG_DEBUG(_logger, "Job " << job->jobId() << " is pending (thread pool maximum capacity reached)");
}

template<class J>
int JobManager<J>::availableThreadsInPool() const {
    try {
        return static_cast<int>(Poco::ThreadPool::defaultPool().available());
    } catch (Poco::Exception &) {
        return 0;
    }
}

template<class J>
void JobManager<J>::managePendingJobs() {
    const auto pendingJobs = _data.pendingJobs();
    for (const auto &[jobId, jobInfo]: pendingJobs) {
        const auto &job = jobInfo.first;
        const auto priority = jobInfo.second;

        if (canRunJob(job)) {
            if (job->isAborted()) {
                // The job is aborted, remove it completely from job manager
                _data.erase(job->jobId());
            } else {
                LOGW_DEBUG(_logger, L"Queuing job " << job->jobId() << L" for execution");
                _data.queue(job, priority);
            }
            _data.removeFromPendingJobs(job->jobId());
        }
    }
}

namespace {

inline bool isBigFileDownloadJob(const std::shared_ptr<AbstractJob> job) {
    if (const auto &downloadJob = std::dynamic_pointer_cast<DownloadJob>(job);
        downloadJob && downloadJob->expectedSize() > bigFileThreshold) {
        return true;
    }

    return false;
}


inline bool isBigFileUploadJob(const std::shared_ptr<AbstractJob> job) {
    // Upload sessions are only for big files
    return bool{std::dynamic_pointer_cast<DriveUploadSession>(job)};
}

} // namespace


template<>
inline bool JobManager<AbstractJob>::canRunJob(const std::shared_ptr<AbstractJob> job) const {
    if (isBigFileUploadJob(job)) {
        for (const auto &runningJobId: _data.runningJobs()) {
            if (const auto &uploadSession = std::dynamic_pointer_cast<DriveUploadSession>(getJob(runningJobId)); uploadSession) {
                // An upload session is already running.
                return false;
            }
        }
        return true;
    }
    if (isBigFileDownloadJob(job) && availableThreadsInPool() < 0.5 * Poco::ThreadPool::defaultPool().capacity()) {
        // Allow big file download only if there is more than 50% of thread available in the pool.
        return false;
    }
    return true;
}

} // namespace KDC
