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

// bool JobManager::_stop = false;
//
// int JobManager::_maxNbThread = 0;
//
// std::unordered_map<UniqueId, std::shared_ptr<AbstractJob>> JobManager::_managedJobs;
// std::priority_queue<std::pair<std::shared_ptr<AbstractJob>, Poco::Thread::Priority>,
//                     std::vector<std::pair<std::shared_ptr<AbstractJob>, Poco::Thread::Priority>>, JobPriorityCmp>
//         JobManager::_queuedJobs;
// std::unordered_set<UniqueId> JobManager::_runningJobs;
// std::unordered_map<UniqueId, std::pair<std::shared_ptr<AbstractJob>, Poco::Thread::Priority>> JobManager::_pendingJobs;
// std::recursive_mutex JobManager::_mutex;
//
// UniqueId JobManager::_uploadSessionJobId = 0;

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

void JobManager::start() {
    _thread = std::make_unique<std::thread>(executeFunc, this);
}

void JobManager::stop() {
    _stop = true;
}

void JobManager::clear() {
    if (_instance) {
        Poco::ThreadPool::defaultPool().stopAll();
        if (_instance->_thread) {
            if (_instance->_thread->joinable()) _instance->_thread->join();
            _instance->_thread = nullptr;
        }
    }

    const std::scoped_lock lock(_mutex);
    _pendingJobs.clear();
    while (!_queuedJobs.empty()) {
        _queuedJobs.pop();
    }
    _managedJobs.clear();
    _runningJobs.clear();
    _stop = false;
}

void JobManager::reset() {
    if (_instance) {
        _instance.reset();
    }
}

void defaultCallback(const UniqueId jobId) {
    JobManager::instance()->eraseJob(jobId);
}

void JobManager::queueAsyncJob(std::shared_ptr<AbstractJob> job,
                               Poco::Thread::Priority priority /*= Poco::Thread::PRIO_NORMAL*/) noexcept {
    const std::scoped_lock lock(_mutex);
    if (!_thread) {
        start();
    }
    job->setMainCallback(defaultCallback);
    _queuedJobs.emplace(job, priority);
    (void) _managedJobs.try_emplace(job->jobId(), job);
}

void JobManager::eraseJob(const UniqueId jobId) {
    const std::scoped_lock lock(_mutex);
    if (_uploadSessionJobId == jobId) {
        _uploadSessionJobId = 0;
    }
    (void) _managedJobs.erase(jobId);
    (void) _runningJobs.erase(jobId);
}

bool JobManager::isJobFinished(const UniqueId &jobId) const {
    const std::scoped_lock lock(_mutex);
    return !_managedJobs.contains(jobId);
}

std::shared_ptr<AbstractJob> JobManager::getJob(const UniqueId &jobId) {
    const std::scoped_lock lock(_mutex);
    if (const auto jobPtr = _managedJobs.find(jobId); jobPtr != _managedJobs.end()) {
        return jobPtr->second;
    }
    return nullptr;
}

void JobManager::setPoolCapacity(const int count) {
    _maxNbThread = std::max(count, 2); // Poco::ThreadPool throw an exception if the capacity is set to a value less than then
                                       // minimum capacity (2 by default)
    Poco::ThreadPool::defaultPool().addCapacity(_maxNbThread - Poco::ThreadPool::defaultPool().capacity());
    LOG_DEBUG(_logger, "Max number of thread changed to " << _maxNbThread << " threads");
}

void JobManager::decreasePoolCapacity() {
    // if (JobManager::instance()->maxNbThreads() > threadPoolMinCapacity) {
    //     // Decrease pool capacity
    //     // TODO: Store the pool capacity in DB?
    //     _maxNbThread -= static_cast<int>(std::ceil((JobManager::instance()->maxNbThreads() - threadPoolMinCapacity) / 2.0));
    //     Poco::ThreadPool::defaultPool().addCapacity(_maxNbThread - Poco::ThreadPool::defaultPool().capacity());
    //     LOG_DEBUG(Log::instance()->getLogger(), "Job Manager capacity set to " << _maxNbThread);
    // } else {
    //     sentry::Handler::captureMessage(sentry::Level::Warning, "JobManager::defaultCallback",
    //                                     "JobManager capacity cannot be decreased");
    // }
}

void JobManager::executeFunc(void *thisWorker) {
    ((JobManager *) thisWorker)->run();
    log4cplus::threadCleanup();
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
        while (availableThreads > 1 && !_stop && !_queuedJobs.empty()) {
            std::shared_ptr<AbstractJob> job;
            auto priority = Poco::Thread::PRIO_NORMAL;
            {
                const std::scoped_lock lock(_mutex);
                job = _queuedJobs.top().first;
                priority = _queuedJobs.top().second;
                _queuedJobs.pop();
            }

            if (canRunjob(job)) {
                startJob(job, priority);
            } else {
                if (const auto [_, inserted] = _pendingJobs.try_emplace(job->jobId(), job, priority); !inserted) {
                    LOG_ERROR(Log::instance()->getLogger(), "Failed to insert job " << job->jobId() << " in pending jobs list!");
                } else {
                    LOG_DEBUG(Log::instance()->getLogger(),
                              "Job " << job->jobId() << " is pending (thread pool maximum capacity reached)");
                }
            }

            availableThreads = availableThreadsInPool();
        }

        // Start job with highest priority
        if (!_queuedJobs.empty()) {
            if (auto &[job, priority] = _queuedJobs.top(); priority == Poco::Thread::Priority::PRIO_HIGHEST) {
                startJob(job, priority);
                _queuedJobs.pop();
            }
        }

        managePendingJobs();

        Utility::msleep(100); // Sleep for 0.1 s
    }
}

void JobManager::startJob(std::shared_ptr<AbstractJob> job, Poco::Thread::Priority priority) {
    const std::scoped_lock lock(_mutex);
    try {
        if (job->isAborted()) {
            LOG_DEBUG(Log::instance()->getLogger(), "Job " << job->jobId() << " has been canceled");
            _managedJobs.erase(job->jobId());
        } else {
            LOG_DEBUG(Log::instance()->getLogger(), "Starting job " << job->jobId() << " with priority " << priority);
            Poco::ThreadPool::defaultPool().startWithPriority(priority, *job);
            _runningJobs.insert(job->jobId());
            if (isBigFileUploadJob(job)) {
                _uploadSessionJobId = job->jobId();
            }
        }
    } catch (Poco::NoThreadAvailableException &) {
        LOG_DEBUG(Log::instance()->getLogger(), "No more thread available, job " << job->jobId() << " queued");
        _queuedJobs.emplace(job, priority);
    } catch (Poco::Exception &e) {
        LOG_WARN(Log::instance()->getLogger(), "Failed to start job: " << e.what());
    }
}

bool JobManager::canRunjob(const std::shared_ptr<AbstractJob> job) {
    if (isBigFileUploadJob(job)) {
        if (_uploadSessionJobId == 0) {
            // Allow only 1 upload session at a time.
            return true;
        }
        return false;
    }
    if (isBigFileDownloadJob(job) && availableThreadsInPool() < 0.5 * Poco::ThreadPool::defaultPool().capacity()) {
        // Allow big file download only if there is more than 50% of thread available in the pool.
        return false;
    }
    return true;
}

bool JobManager::isBigFileDownloadJob(const std::shared_ptr<AbstractJob> job) {
    if (const auto &downloadJob = std::dynamic_pointer_cast<DownloadJob>(job);
        downloadJob && downloadJob->expectedSize() > bigFileThreshold) {
        return true;
    }
    return false;
}

bool JobManager::isBigFileUploadJob(const std::shared_ptr<AbstractJob> job) {
    if (std::dynamic_pointer_cast<DriveUploadSession>(job)) {
        // Upload sessions are only for big files
        return true;
    }
    return false;
}

void JobManager::managePendingJobs() {
    const std::scoped_lock lock(_mutex);

    // Check if parent jobs of the pending jobs has finished
    auto it = _pendingJobs.begin();
    while (it != _pendingJobs.end()) {
        const auto &jobId = it->first;
        const auto &job = it->second.first;
        const auto priority = it->second.second;

        if (canRunjob(job)) {
            if (job->isAborted()) {
                // The job is aborted, remove it completely from job manager
                (void) _managedJobs.erase(jobId);
            } else {
                LOGW_DEBUG(Log::instance()->getLogger(),
                           L"The thread pool has recovered capacity, queuing job " << job->jobId() << L" for execution");
                _queuedJobs.emplace(job, priority);
            }
            it = _pendingJobs.erase(it);
            continue;
        }
        it++;
    }
}

int JobManager::availableThreadsInPool() {
    try {
        return Poco::ThreadPool::defaultPool().available();
    } catch (Poco::Exception &) {
        return 0;
    }
}

} // namespace KDC
