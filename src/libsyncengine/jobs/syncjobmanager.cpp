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

#include "syncjobmanager.h"
#include "jobmanager.h"

#include <memory>
#include "jobs/network/kDrive_API/downloadjob.h"
#include "jobs/network/kDrive_API/upload/upload_session/driveuploadsession.h"


namespace KDC {
SyncJobManager::SyncJobManager() :
    JobManager() {};

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
    return static_cast<bool>(std::dynamic_pointer_cast<DriveUploadSession>(job));
}

} // namespace

bool SyncJobManager::canRunJob(std::shared_ptr<AbstractJob> job) const {
    if (isBigFileUploadJob(job)) {
        for (const auto &runningJobId: data().runningJobs()) {
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

std::shared_ptr<SyncJobManager> SyncJobManagerSingleton::_instance = nullptr;

std::shared_ptr<SyncJobManager> SyncJobManagerSingleton::instance() noexcept {
    if (_instance == nullptr) {
        try {
            _instance = std::make_shared<SyncJobManager>();
        } catch (...) {
            return nullptr;
        }
    }

    return _instance;
}

void SyncJobManagerSingleton::clear() {
    if (_instance) {
        _instance->clear();
        _instance.reset();
    }
}

} // namespace KDC
