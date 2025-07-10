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

#include <queue>

namespace KDC {

/**
 * @brief A thread safe class to access JobManager data.
 */
template<class Job>
class JobManagerData {
    public:
        /**
         * @brief Add a job to queue. The job will be automatically started, according to its given priority as soon as there are
         * available threads in the pool.
         * @param job The job to be run asynchronously.
         * @param priority The job's priority.
         */
        void queue(std::shared_ptr<Job> job, Poco::Thread::Priority priority = Poco::Thread::PRIO_NORMAL);

        /**
         * @brief Remove the top job from the queue.
         * @return The removed job and its associated priority.
         */
        std::pair<std::shared_ptr<Job>, Poco::Thread::Priority> pop();

        /**
         * @brief Check is there are jobs in the queue waiting to be executed.
         * @return 'true' if the queue is not empty.
         */
        bool hasQueuedJob() const;

        /**
         * @brief Check is the top job of the queue has the highest possible priority.
         * @return 'true' if the top job has value 'PRIO_HIGHEST'.
         */
        bool hasHighestPriorityJob() const;

        /**
         * @brief Check if the job is currently handled by the JobManager, meaning the job is either queued, pending or running.
         * @param jobId The ID of the job to be checked.
         * @return 'true' if the job is currently handled.
         */
        bool isManaged(const UniqueId jobId) const;

        /**
         * @brief Add a job to the list of running jobs.
         * @param jobId The ID of the job.
         * @return 'true' if the job ID has been successfully inserted in the map.
         */
        bool addToRunningJobs(const UniqueId jobId);

        /**
         * @brief Add a job to the list of pending jobs.
         * @param jobId The ID of the job.
         * @param job A shared pointer to the job.
         * @param priority The job's priority
         * @return 'true' if the job ID has been successfully inserted in the map.
         */
        bool addToPendingJobs(const UniqueId jobId, std::shared_ptr<Job> job,
                              Poco::Thread::Priority priority = Poco::Thread::PRIO_NORMAL);
        /**
         * @brief Remove a job from the list of pending jobs.
         * @param jobId The ID of the job.
         */
        void removeFromPendingJobs(const UniqueId jobId);

        /**
         * @brief Remove the job from JobManager. The job will not be handled by JobManager afterward. This method is the main one
         * used in JobManager's callback.
         * @param jobId The ID of the job.
         */
        void erase(const UniqueId jobId);

        /**
         * @brief Get the IDs of the currently running jobs. A copy of the list is returned in order to avoid concurrency
         * issues while using the returned list.
         * @return A copy of the list of IDs of the running jobs.
         */
        std::unordered_set<UniqueId> runningJobs() const;

        /**
         * @brief Get a copy of the list of jobs currently pending.
         * @return A copy of the list of pending jobs.
         */
        using PrioritizedJobMap = std::unordered_map<UniqueId, std::pair<std::shared_ptr<Job>, Poco::Thread::Priority>>;
        const PrioritizedJobMap pendingJobs() const;

        /**
         * @brief Retrieve a job based on its ID.
         * @param jobId The ID of the job.
         * @return A shared pointer to the job if found. 'nullptr' otherwise.
         */
        std::shared_ptr<Job> getJob(const UniqueId &jobId) const;

        void clear();

    private:
        std::unordered_map<UniqueId, std::shared_ptr<Job>> _managedJobs; // queued + running + pending jobs.

        using JobPriority = Poco::Thread::Priority;
        using PioritizedJob = std::pair<std::shared_ptr<Job>, JobPriority>;
        class JobPriorityComparator {
            public:
                bool operator()(const PioritizedJob &j1, const PioritizedJob &j2) const {
                    if (j1.second == j2.second) {
                        // Same thread priority, use the job ID to define priority
                        return j1.first->jobId() > j2.first->jobId();
                    }
                    return j1.second < j2.second;
                }
        };

        using PrioritizedJobQueue = std::priority_queue<PioritizedJob, std::vector<PioritizedJob>, JobPriorityComparator>;
        PrioritizedJobQueue _queuedJobs; // jobs waiting for an available thread.

        std::unordered_set<UniqueId> _runningJobs; // jobs currently running in a dedicated thread.

        PrioritizedJobMap _pendingJobs; // jobs waiting to be able to start.
        mutable std::mutex _mutex;

        friend class TestJobManager;
};

template<class Job>
void JobManagerData<Job>::queue(std::shared_ptr<Job> job, Poco::Thread::Priority priority /*= Poco::Thread::PRIO_NORMAL*/) {
    const std::scoped_lock lock(_mutex);
    _queuedJobs.emplace(job, priority);
    (void) _managedJobs.try_emplace(job->jobId(), job);
}

template<class Job>
std::pair<std::shared_ptr<Job>, Poco::Thread::Priority> JobManagerData<Job>::pop() {
    const std::scoped_lock lock(_mutex);
    if (_queuedJobs.empty()) {
        return std::make_pair(nullptr, Poco::Thread::PRIO_NORMAL);
    }
    const auto ret = _queuedJobs.top();
    _queuedJobs.pop();
    return ret;
}

template<class Job>
bool JobManagerData<Job>::hasQueuedJob() const {
    return !_queuedJobs.empty();
}

template<class Job>
bool JobManagerData<Job>::hasHighestPriorityJob() const {
    const std::scoped_lock lock(_mutex);
    if (_queuedJobs.empty()) {
        return false;
    }
    auto &[job, priority] = _queuedJobs.top();
    return priority == Poco::Thread::Priority::PRIO_HIGHEST;
}

template<class Job>
bool JobManagerData<Job>::isManaged(const UniqueId jobId) const {
    const std::scoped_lock lock(_mutex);
    return _managedJobs.contains(jobId);
}

template<class Job>
bool JobManagerData<Job>::addToRunningJobs(const UniqueId jobId) {
    const std::scoped_lock lock(_mutex);
    const auto [_, inserted] = _runningJobs.insert(jobId);
    return inserted;
}

template<class Job>
bool JobManagerData<Job>::addToPendingJobs(const UniqueId jobId, std::shared_ptr<Job> job, Poco::Thread::Priority priority) {
    const std::scoped_lock lock(_mutex);
    const auto [_, inserted] = _pendingJobs.try_emplace(jobId, job, priority);
    return inserted;
}

template<class Job>
void JobManagerData<Job>::removeFromPendingJobs(const UniqueId jobId) {
    const std::scoped_lock lock(_mutex);
    (void) _pendingJobs.erase(jobId);
}

template<class Job>
void JobManagerData<Job>::erase(const UniqueId jobId) {
    const std::scoped_lock lock(_mutex);
    (void) _managedJobs.erase(jobId);
    (void) _runningJobs.erase(jobId);
}

template<class Job>
std::unordered_set<UniqueId> JobManagerData<Job>::runningJobs() const {
    const std::scoped_lock lock(_mutex);
    return _runningJobs;
}

template<class Job>
const std::unordered_map<UniqueId, std::pair<std::shared_ptr<Job>, Poco::Thread::Priority>> JobManagerData<Job>::pendingJobs()
        const {
    const std::scoped_lock lock(_mutex);
    return _pendingJobs;
}

template<class Job>
std::shared_ptr<Job> JobManagerData<Job>::getJob(const UniqueId &jobId) const {
    const std::scoped_lock lock(_mutex);
    if (const auto jobPtrIt = _managedJobs.find(jobId); jobPtrIt != _managedJobs.end()) {
        return jobPtrIt->second;
    }
    return nullptr;
}

template<class Job>
void JobManagerData<Job>::clear() {
    const std::scoped_lock lock(_mutex);
    _pendingJobs.clear();
    while (!_queuedJobs.empty()) {
        _queuedJobs.pop();
    }

    for (const auto &[_, job]: _managedJobs) {
        job->setMainCallback(nullptr);
    }

    _managedJobs.clear();
    _runningJobs.clear();
}
} // namespace KDC
