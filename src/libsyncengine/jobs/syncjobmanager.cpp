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
#include "jobs/network/kDrive_API/downloadjob.h"
#include "jobs/network/kDrive_API/upload/upload_session/driveuploadsession.h"

#include <memory>

namespace KDC {
SyncJobManager::SyncJobManager() :
    JobManager() {}

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

bool SyncJobManager::canRunJob(const std::shared_ptr<AbstractJob> job) const {
    const bool currentJobIsUploadSession = isBigFileUploadJob(job);
    const bool currentJobIsBigFileDownloadJob = isBigFileDownloadJob(job);
    if (!currentJobIsUploadSession && !currentJobIsBigFileDownloadJob) return true;

    uint64_t bigDownloadCounter = 0;
    for (const auto &runningJobId: data().runningJobs()) {
        if (currentJobIsUploadSession && isBigFileUploadJob(getJob(runningJobId))) {
            // An upload session is already running.
            return false;
        }
        if (currentJobIsBigFileDownloadJob && isBigFileDownloadJob(getJob(runningJobId))) {
            // Another big download job is running.
            bigDownloadCounter++;
            if (bigDownloadCounter >= maxNumberParallelBigDownloads) {
                // Allow max 3 big file download jobs in parallel.
                return false;
            }
        }
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
