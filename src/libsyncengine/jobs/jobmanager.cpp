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

#include "jobmanager.h"
#include "jobs/network/networkjobsparams.h"
#include "log/log.h"
#include "jobs/network/upload_session/abstractuploadsession.h"
#include "libcommonserver/utility/utility.h"
#include "performance_watcher/performancewatcher.h"
#include "requests/parameterscache.h"

#include <thread>
#include <algorithm>  // std::max

#include <log4cplus/loggingmacros.h>

#include <Poco/Exception.h>
#include <Poco/ThreadPool.h>

namespace KDC {

const int secondsBetweenCpuCalculation = 10;
const double cpuThreadsThreshold = 0.5;

JobManager *JobManager::_instance = nullptr;
bool JobManager::_stop = false;

int JobManager::_maxNbThread = 0;
double JobManager::_cpuUsageThreshold = 0.5;
int JobManager::_threadAdjustmentStep = 2;
std::chrono::time_point<std::chrono::steady_clock> JobManager::_maxNbThreadChrono = std::chrono::steady_clock::now();

std::unordered_map<UniqueId, std::shared_ptr<AbstractJob>> JobManager::_managedJobs;
std::priority_queue<std::pair<std::shared_ptr<AbstractJob>, Poco::Thread::Priority>,
                    std::vector<std::pair<std::shared_ptr<AbstractJob>, Poco::Thread::Priority>>, JobPriorityCmp>
    JobManager::_queuedJobs;
std::unordered_set<UniqueId> JobManager::_runningJobs;
std::unordered_map<UniqueId, std::pair<std::shared_ptr<AbstractJob>, Poco::Thread::Priority>> JobManager::_pendingJobs;
std::mutex JobManager::_mutex;

JobManager *JobManager::instance() {
    if (!_instance) {
        _instance = new JobManager();
    }
    return _instance;
}

void JobManager::stop() {
    _stop = true;
}

void JobManager::clear() {
    if (_instance) {
        Poco::ThreadPool::defaultPool().stopAll();
        _instance->_thread->join();
        _instance->_thread = nullptr;
    }

    const std::lock_guard<std::mutex> lock(_mutex);
    _pendingJobs.clear();
    while (!_queuedJobs.empty()) {
        _queuedJobs.pop();
    }
    _managedJobs.clear();
    _runningJobs.clear();
}

void JobManager::queueAsyncJob(std::shared_ptr<AbstractJob> job, Poco::Thread::Priority priority /*= Poco::Thread::PRIO_NORMAL*/,
                               std::function<void(UniqueId)> externalCallback /*= nullptr*/) {
    const std::lock_guard<std::mutex> lock(_mutex);
    _queuedJobs.push({job, priority});

    job->setMainCallback(defaultCallback);
    try {
        _managedJobs.insert({job->jobId(), job});
    } catch (std::exception &) {
        LOG_WARN(_logger, "Job not managed");
    }

    if (externalCallback) {
        job->setAdditionalCallback(externalCallback);
    }
}

bool JobManager::isJobFinished(const UniqueId &jobId) {
    const std::lock_guard<std::mutex> lock(_mutex);
    return _managedJobs.find(jobId) == _managedJobs.end();
}

std::shared_ptr<AbstractJob> JobManager::getJob(const UniqueId &jobId) {
    const std::lock_guard<std::mutex> lock(_mutex);
    auto jobPtr = _managedJobs.find(jobId);
    if (jobPtr != _managedJobs.end()) {
        return jobPtr->second;
    }
    return nullptr;
}

void JobManager::defaultCallback(UniqueId jobId) {
    const std::lock_guard<std::mutex> lock(_mutex);
    auto node = _managedJobs.extract(jobId);
    _runningJobs.erase(jobId);
}

JobManager::JobManager() : _logger(Log::instance()->getLogger()) {
    int jobPoolCapacityFactor = ParametersCache::instance()->parameters().jobPoolCapacityFactor();

    _maxNbThread = std::max(threadPoolMinCapacity, jobPoolCapacityFactor * (int)std::thread::hardware_concurrency());
    Poco::ThreadPool::defaultPool().addCapacity(_maxNbThread - Poco::ThreadPool::defaultPool().capacity());

    _cpuUsageThreshold = ParametersCache::instance()->parameters().maxAllowedCpu() / 100.0;

    _thread = std::unique_ptr<std::thread>(new std::thread(run));
    LOG_DEBUG(_logger, "Network Job Manager started with max " << _maxNbThread << " threads");
}

void JobManager::run() {
    while (true) {
        if (_stop) {
            break;
        }

        // Count UploadSessionJobs running
        int uploadSessionCount = countUploadSession();

        // Count UploadSessionJobs running
        while (Poco::ThreadPool::defaultPool().available()) {
            if (_stop) {
                break;
            }

            {
                const std::scoped_lock<std::mutex> lock(_mutex);

                if (_queuedJobs.empty()) {
                    break;
                }
                auto jobItem = _queuedJobs.top();
                const auto &job = jobItem.first;
                _queuedJobs.pop();

                if (!canStartJob(job, uploadSessionCount)) {
                    try {
                        _pendingJobs.try_emplace(job->jobId(), jobItem);
                        if (job->hasParentJob()) {
                            LOG_DEBUG(Log::instance()->getLogger(), "Job " << job->jobId()
                                                                           << " is pending (waiting for parent job "
                                                                           << job->parentJobId() << " to complete)");
                        } else {
                            LOG_DEBUG(Log::instance()->getLogger(),
                                      "Job " << job->jobId() << " is pending (thread pool maximum capacity reached)");
                        }
                    } catch (std::exception &) {
                        LOG_WARN(Log::instance()->getLogger(), "Job " << job->jobId() << " insertion in pending queue failed");
                    }
                } else {
                    if (std::dynamic_pointer_cast<AbstractUploadSession>(job)) {
                        uploadSessionCount++;
                    }

                    startJob(jobItem);
                }
            }

            // adjustMaxNbThread();
        }

        managePendingJobs(uploadSessionCount);

        if (_queuedJobs.empty()) {
            Utility::msleep(100);  // Sleep for 0.1s
        }
    }
}

void JobManager::startJob(std::pair<std::shared_ptr<AbstractJob>, Poco::Thread::Priority> nextJob) {
    try {
        if (nextJob.first->isAborted()) {
            LOG_DEBUG(Log::instance()->getLogger(), "Job " << nextJob.first->jobId() << " has been canceled");
            _managedJobs.erase(nextJob.first->jobId());
        } else {
            LOG_DEBUG(Log::instance()->getLogger(),
                      "Starting job " << nextJob.first->jobId() << " with priority " << nextJob.second);
            Poco::ThreadPool::defaultPool().startWithPriority(nextJob.second, *nextJob.first);
            _runningJobs.insert(nextJob.first->jobId());
        }
    } catch (Poco::NoThreadAvailableException &) {
        LOG_DEBUG(Log::instance()->getLogger(), "No more thread available, job " << nextJob.first->jobId() << " queued");
        _queuedJobs.push(nextJob);
    } catch (std::exception &) {
        LOG_WARN(Log::instance()->getLogger(), "Job not inserted in running queue.");
    }
}

bool JobManager::isParentPendingOrRunning(UniqueId jobIb) {
    if (_managedJobs.find(jobIb) == _managedJobs.end()) {
        return false;
    }

    const auto &job = _managedJobs.find(jobIb)->second;
    return job->hasParentJob() && (_runningJobs.find(job->parentJobId()) != _runningJobs.end() ||
                                   _pendingJobs.find(job->parentJobId()) != _pendingJobs.end());
}

bool JobManager::canStartJob(std::shared_ptr<AbstractJob> job, int uploadSessionCount) {
    return !isParentPendingOrRunning(job->jobId()) && !(std::dynamic_pointer_cast<AbstractUploadSession>(job) &&
                                                        uploadSessionCount >= Poco::ThreadPool::defaultPool().capacity() / 10);
}

void JobManager::adjustMaxNbThread() {
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = now - _maxNbThreadChrono;
    if (elapsed_seconds.count() < secondsBetweenCpuCalculation) {
        return;
    }
    _cpuUsageThreshold = ParametersCache::instance()->parameters().maxAllowedCpu() == 0
                             ? cpuThreadsThreshold
                             : ParametersCache::instance()->parameters().maxAllowedCpu() / 100.0;
    _maxNbThreadChrono = now;

    double cpuUsage = PerformanceWatcher::instance()->getMovingAverageCpuUsagePercent() / 100.0;
    int maxTmpNbThread = _maxNbThread;

    if (cpuUsage > _cpuUsageThreshold) {
        maxTmpNbThread -= _threadAdjustmentStep;
    } else {
        maxTmpNbThread += _threadAdjustmentStep;
    }

    maxTmpNbThread = std::max(maxTmpNbThread, threadPoolMinCapacity);
    int threadMultiplier = _cpuUsageThreshold * 10;
    _maxNbThread = std::min(maxTmpNbThread, threadMultiplier * static_cast<int>(std::thread::hardware_concurrency()));

    Poco::ThreadPool::defaultPool().addCapacity(_maxNbThread - Poco::ThreadPool::defaultPool().capacity());
    int pocoThreadCapacity = Poco::ThreadPool::defaultPool().capacity();
    int pocoThreadAllocated = Poco::ThreadPool::defaultPool().allocated();
    LOG_INFO(Log::instance()->getLogger(),
             "With cpu usage : " << cpuUsage * 100 << " % (threshold : " << _cpuUsageThreshold * 100 << " %)");
    LOG_INFO(Log::instance()->getLogger(), "Running threads : " << _runningJobs.size() << "/" << _maxNbThread << " ("
                                                                << pocoThreadAllocated << "/" << pocoThreadCapacity << ")");
}

int JobManager::countUploadSession() {
    const std::lock_guard<std::mutex> lock(_mutex);
    int uploadSessionCount = 0;
    for (UniqueId id : _runningJobs) {
        const auto &job = _managedJobs[id];
        if (std::dynamic_pointer_cast<AbstractUploadSession>(job)) {
            uploadSessionCount++;
        }
    }
    return uploadSessionCount;
}

void JobManager::managePendingJobs(int uploadSessionCount) {
    const std::scoped_lock<std::mutex> lock(_mutex);

    // Check if parent jobs of the pending jobs has finished
    std::erase_if(_pendingJobs, [&uploadSessionCount](const auto &item) {
        if (const auto &job = item.second.first; canStartJob(job, uploadSessionCount)) {
            if (job->isAborted()) {
                // The job is aborted, remove it completly from job manager
                _managedJobs.erase(item.first);
            } else {
                if (job->hasParentJob()) {
                    LOG_DEBUG(Log::instance()->getLogger(), "Job " << job->parentJobId() << " has finished, queuing child job "
                                                                   << job->jobId() << " for execution");
                } else {
                    LOGW_DEBUG(Log::instance()->getLogger(),
                              "The thread pool has recovered capacity, queuing job " << job->jobId() << " for execution");
                }
                _queuedJobs.push(item.second);
            }
            return true;
        } else {
            return false;
        }
    });
}

}  // namespace KDC
