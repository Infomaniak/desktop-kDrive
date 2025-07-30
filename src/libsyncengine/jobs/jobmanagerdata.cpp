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

#include "jobmanagerdata.h"

namespace KDC {

void JobManagerData::queue(std::shared_ptr<AbstractJob> job, Poco::Thread::Priority priority /*= Poco::Thread::PRIO_NORMAL*/) {
    const std::scoped_lock lock(_mutex);
    _queuedJobs.emplace(job, priority);
    (void) _managedJobs.try_emplace(job->jobId(), job);
}

std::pair<std::shared_ptr<AbstractJob>, Poco::Thread::Priority> JobManagerData::pop() {
    const std::scoped_lock lock(_mutex);
    if (_queuedJobs.empty()) {
        return std::make_pair(nullptr, Poco::Thread::PRIO_NORMAL);
    }
    const auto ret = _queuedJobs.top();
    _queuedJobs.pop();
    return ret;
}

bool JobManagerData::hasQueuedJob() const {
    return !_queuedJobs.empty();
}

bool JobManagerData::hasHighestPriorityJob() const {
    const std::scoped_lock lock(_mutex);
    if (_queuedJobs.empty()) {
        return false;
    }
    auto &[job, priority] = _queuedJobs.top();
    return priority == Poco::Thread::Priority::PRIO_HIGHEST;
}

bool JobManagerData::isManaged(const UniqueId jobId) const {
    const std::scoped_lock lock(_mutex);
    return _managedJobs.contains(jobId);
}

bool JobManagerData::addToRunningJobs(const UniqueId jobId) {
    const std::scoped_lock lock(_mutex);
    const auto [_, inserted] = _runningJobs.insert(jobId);
    return inserted;
}

bool JobManagerData::addToPendingJobs(const UniqueId jobId, std::shared_ptr<AbstractJob> job, Poco::Thread::Priority priority) {
    const std::scoped_lock lock(_mutex);
    const auto [_, inserted] = _pendingJobs.try_emplace(jobId, job, priority);
    return inserted;
}

void JobManagerData::removeFromPendingJobs(const UniqueId jobId) {
    const std::scoped_lock lock(_mutex);
    (void) _pendingJobs.erase(jobId);
}

void JobManagerData::erase(const UniqueId jobId) {
    const std::scoped_lock lock(_mutex);
    (void) _managedJobs.erase(jobId);
    (void) _runningJobs.erase(jobId);
}

std::unordered_set<UniqueId> JobManagerData::runningJobs() const {
    const std::scoped_lock lock(_mutex);
    return _runningJobs;
}

const std::unordered_map<UniqueId, std::pair<std::shared_ptr<AbstractJob>, Poco::Thread::Priority>> JobManagerData::pendingJobs()
        const {
    const std::scoped_lock lock(_mutex);
    return _pendingJobs;
}

std::shared_ptr<AbstractJob> JobManagerData::getJob(const UniqueId &jobId) const {
    const std::scoped_lock lock(_mutex);
    if (const auto jobPtrIt = _managedJobs.find(jobId); jobPtrIt != _managedJobs.end()) {
        return jobPtrIt->second;
    }
    return nullptr;
}

void JobManagerData::clear() {
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
