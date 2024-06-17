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

#include "contentchecksumworker.h"
#include "computechecksumjob.h"
#include "libcommonserver/utility/utility.h"

#include <log4cplus/loggingmacros.h>

#include <thread>

namespace KDC {

std::mutex ContentChecksumWorker::_checksumMutex;
std::unordered_map<UniqueId, std::shared_ptr<ComputeChecksumJob>> ContentChecksumWorker::_runningJobs;

ContentChecksumWorker::ContentChecksumWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name,
                                             const std::string &shortName, std::shared_ptr<Snapshot> localSnapshot)
    : ISyncWorker(syncPal, name, shortName),
      _localSnapshot(localSnapshot),
      _threadPool(1, 5)  // Min 1 thread, max 5
{}

ContentChecksumWorker::~ContentChecksumWorker() {
    // Stop all threads
    _threadPool.stopAll();

    _runningJobs.clear();
}

void ContentChecksumWorker::computeChecksum(const NodeId &id, const SyncPath &file) {
    // Schedule content checksum computation
    _localSnapshot->clearContentChecksum(id);
    _toCompute.push({id, file});
}

void ContentChecksumWorker::callback(UniqueId jobId) {
    const std::lock_guard<std::mutex> lock(_checksumMutex);
    _runningJobs.extract(jobId);
}

void ContentChecksumWorker::execute() {
    ExitCode exitCode(ExitCode::Unknown);

    LOG_DEBUG(_logger, "Worker started: name=" << name().c_str());

    // Sync loop
    for (;;) {
        if (stopAsked()) {
            // Stop all threads
            try {
                _threadPool.stopAll();
            } catch (...) {
                // Do nothing
            }

            const std::lock_guard<std::mutex> lock(_checksumMutex);
            while (_runningJobs.begin() != _runningJobs.end()) {
                _runningJobs.extract(_runningJobs.begin()).mapped();
            }

            _runningJobs.clear();

            exitCode = ExitCode::Ok;
            break;
        }
        // We never pause this thread

        while (!_toCompute.empty()) {
            if (stopAsked()) {
                break;
            }

            if (_threadPool.available()) {
                const std::lock_guard<std::mutex> lock(_checksumMutex);
                std::shared_ptr<ComputeChecksumJob> job =
                    std::make_shared<ComputeChecksumJob>(_toCompute.front().first, _toCompute.front().second, _localSnapshot);
                _runningJobs.insert({job->jobId(), job});
                job->setMainCallback(callback);
                _threadPool.start(*job);
                _toCompute.pop();
            } else {
                // No thread available, wait
                break;
            }
        }

        Utility::msleep(10);
    }

    setDone(exitCode);
    LOG_DEBUG(_logger, "Worker stopped: name=" << name().c_str());
}

}  // namespace KDC
