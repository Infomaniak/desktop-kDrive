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

#include "jobs/network/abstractnetworkjob.h"

#include "network/API_v2/downloadjob.h"
#include "network/API_v2/upload/upload_session/driveuploadsession.h"
#include "performance_watcher/performancewatcher.h"
#include "requests/parameterscache.h"

#include <algorithm> // std::max

#include <log4cplus/loggingmacros.h>

#include <Poco/Exception.h>
#include <Poco/ThreadPool.h>


namespace KDC {

namespace {

bool isBigFileDownloadJob(const std::shared_ptr<AbstractJob> job) {
    if (const auto &downloadJob = std::dynamic_pointer_cast<DownloadJob>(job);
        downloadJob && downloadJob->expectedSize() > bigFileThreshold) {
        return true;
    }

    return false;
}


bool isBigFileUploadJob(const std::shared_ptr<AbstractJob> job) {
    // Upload sessions are only for big files
    return bool{std::dynamic_pointer_cast<DriveUploadSession>(job)};
}

}; // namespace


template<>
bool JobManager<AbstractJob>::canRunJob(const std::shared_ptr<AbstractJob> job) const {
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
