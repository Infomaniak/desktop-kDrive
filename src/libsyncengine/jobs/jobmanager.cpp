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

#include "jobmanager.h"
#include "jobs/network/networkjobsparams.h"
#include "jobs/network/abstractnetworkjob.h"
#include "log/log.h"
#include "libcommonserver/utility/utility.h"
#include "network/API_v2/downloadjob.h"
#include "network/API_v2/upload/upload_session/driveuploadsession.h"
#include "performance_watcher/performancewatcher.h"
#include "requests/parameterscache.h"

#include <algorithm> // std::max

#include <log4cplus/loggingmacros.h>

#include <Poco/Exception.h>
#include <Poco/ThreadPool.h>

namespace KDC {

std::shared_ptr<JobManager> JobManager::_instance = nullptr;

std::shared_ptr<JobManager> JobManager::instance() noexcept {
    if (_instance == nullptr) {
        try {
            _instance = std::shared_ptr<JobManager>(new JobManager());
        } catch (...) {
            return nullptr;
        }
    }

    return _instance;
}

void JobManager::startMainThreadIfNeeded() {
    if (!_mainThread) {
        const std::function<void()> runFunction = std::bind_front(&JobManager::run, this);
        _mainThread = std::make_unique<std::thread>(runFunction);
    }
}

void JobManager::stop() {
    _stop = true;
}

void JobManager::clear() {
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

void JobManager::queueAsyncJob(const std::shared_ptr<AbstractJob> job,
                               const Poco::Thread::Priority priority /*= Poco::Thread::PRIO_NORMAL*/) noexcept {
    startMainThreadIfNeeded();
    const std::function<void(const UniqueId)> callback = std::bind_front(&JobManager::eraseJob, this);
    job->setMainCallback(callback);
    _data.queue(job, priority);
}

bool JobManager::isJobFinished(const UniqueId jobId) const {
    return _data.isManaged(jobId);
}

std::shared_ptr<AbstractJob> JobManager::getJob(const UniqueId jobId) const {
    return _data.getJob(jobId);
}

void JobManager::setPoolCapacity(const int nbThread) {
    // Poco::ThreadPool throws an exception if the capacity is set to a
    // value less than then minimum capacity (2 by default).
    static_assert(threadPoolMinCapacity >= 2 && "Thread pool min capacity is too low.");

    _maxNbThread = std::max(nbThread, threadPoolMinCapacity);
    Poco::ThreadPool::defaultPool().addCapacity(_maxNbThread - Poco::ThreadPool::defaultPool().capacity());
    LOG_DEBUG(_logger, "Max number of thread changed to " << _maxNbThread << " threads");
}

void JobManager::decreasePoolCapacity() {
    if (_maxNbThread > threadPoolMinCapacity) {
        // Divide the maximum number of thread by 2 (rounded up) on each call.
        _maxNbThread -= static_cast<int>(std::ceil((_maxNbThread - threadPoolMinCapacity) / 2.0));
        setPoolCapacity(_maxNbThread);
    } else {
        sentry::Handler::captureMessage(sentry::Level::Warning, "JobManager::defaultCallback",
                                        "JobManager capacity cannot be decreased");
    }
}

JobManager::JobManager() :
    _logger(Log::instance()->getLogger()) {
    setPoolCapacity(std::min(static_cast<int>(std::thread::hardware_concurrency()), threadPoolMaxCapacity));

    LOG_DEBUG(_logger, "Network Job Manager started with max " << _maxNbThread << " threads");
}

void JobManager::run() noexcept {
    while (true) {
        if (_stop) {
            break;
        }

        auto availableThreads = availableThreadsInPool();
        // Always keep 1 thread available for jobs with highest priority
        while (availableThreads > 1 && !_stop && _data.hasQueuedJob()) {
            const auto [job, priority] = _data.pop();
            if (canRunjob(job)) {
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

void JobManager::startJob(std::shared_ptr<AbstractJob> job, Poco::Thread::Priority priority) {
    try {
        if (job->isAborted()) {
            LOG_DEBUG(Log::instance()->getLogger(), "Job " << job->jobId() << " has been canceled");
            _data.erase(job->jobId());
        } else {
            LOG_DEBUG(Log::instance()->getLogger(), "Starting job " << job->jobId() << " with priority " << priority);
            Poco::ThreadPool::defaultPool().startWithPriority(priority, *job);
            if (!_data.addToRunningJobs(job->jobId())) {
                LOG_WARN(Log::instance()->getLogger(), "Failed to insert job " << job->jobId() << " in _runningJobs map");
            }
        }
    } catch (Poco::NoThreadAvailableException &) {
        LOG_DEBUG(Log::instance()->getLogger(), "No more thread available, job " << job->jobId() << " queued");
        _data.queue(job, priority);
    } catch (Poco::Exception &e) {
        LOG_WARN(Log::instance()->getLogger(), "Failed to start job: " << e.what());
    }
}

void JobManager::eraseJob(const UniqueId jobId) {
    _data.erase(jobId);
}

void JobManager::addToPendingJobs(const std::shared_ptr<AbstractJob> job, const Poco::Thread::Priority priority) {
    if (!_data.addToPendingJobs(job->jobId(), job, priority)) {
        LOG_ERROR(Log::instance()->getLogger(), "Failed to insert job " << job->jobId() << " in pending jobs list!");
        return;
    }
    LOG_DEBUG(Log::instance()->getLogger(), "Job " << job->jobId() << " is pending (thread pool maximum capacity reached)");
}

int JobManager::availableThreadsInPool() const {
    try {
        return static_cast<int>(Poco::ThreadPool::defaultPool().available());
    } catch (Poco::Exception &) {
        return 0;
    }
}

bool JobManager::canRunjob(const std::shared_ptr<AbstractJob> job) const {
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

bool JobManager::isBigFileDownloadJob(const std::shared_ptr<AbstractJob> job) const {
    if (const auto &downloadJob = std::dynamic_pointer_cast<DownloadJob>(job);
        downloadJob && downloadJob->expectedSize() > bigFileThreshold) {
        return true;
    }
    return false;
}

bool JobManager::isBigFileUploadJob(const std::shared_ptr<AbstractJob> job) const {
    if (std::dynamic_pointer_cast<DriveUploadSession>(job)) {
        // Upload sessions are only for big files
        return true;
    }
    return false;
}

void JobManager::managePendingJobs() {
    const auto pendingJobs = _data.pendingJobs();
    for (const auto &[jobId, jobInfo]: pendingJobs) {
        const auto &job = jobInfo.first;
        const auto priority = jobInfo.second;

        if (canRunjob(job)) {
            if (job->isAborted()) {
                // The job is aborted, remove it completely from job manager
                _data.erase(job->jobId());
            } else {
                LOGW_DEBUG(Log::instance()->getLogger(), L"Queuing job " << job->jobId() << L" for execution");
                _data.queue(job, priority);
            }
            _data.removeFromPendingJobs(job->jobId());
        }
    }
}

} // namespace KDC
