/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "drivedeletejob.h"
#include "appserver.h"
#include "requests/serverrequests.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

// Input parameters keys
static const auto inParamsDriveDbId = "driveDbId";

// User action lock timeout duration
static const int userActionLockTimeoutMs = 5000;

namespace KDC {

DriveDeleteJob::DriveDeleteJob(std::shared_ptr<CommManager> commManager, int requestId, const Poco::DynamicStruct &inParams,
                               std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::DRIVE_DELETE;
}

ExitInfo DriveDeleteJob::deserializeInputParms() {
    try {
        readParamValue(inParamsDriveDbId, _driveDbId);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in DriveDeleteJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo DriveDeleteJob::serializeOutputParms() {
    return ExitCode::Ok;
}

ExitInfo DriveDeleteJob::process() {
    // Get syncs to delete
    std::vector<SyncDbId> syncDbIdList;
    std::list<UserActionScopedLock> locks;

    const std::scoped_lock lock(_commManager->appServer().syncPalMapMutex);
    for (const auto &[syncDbId, syncPal]: _commManager->appServer().syncPalMap) {
        if (!syncPal) continue;
        if (syncPal->driveDbId() == _driveDbId) {
            syncDbIdList.push_back(syncDbId);
            UserActionScopedLock &lock = locks.emplace_back();
            if (!lock.tryLock(syncPal, std::chrono::milliseconds(userActionLockTimeoutMs))) {
                LOG_WARN(_logger, "Could not acquire user action lock for syncDbId="
                                          << syncDbId << ". Another user action is running. Aborting DriveDeleteJob.");
                return ExitCode::OperationCanceled;
            }
        }

        // Stop syncs for this user and remove them from syncPalMap.
        _commManager->appServer().stopAllSyncsTask(syncDbIdList, SyncPal::DbBehaviorAfterStop::Remove);
        _commManager->appServer().deleteDrive(_driveDbId);
#if defined(KD_MACOS)
        Utility::restartFinderExtension();
#endif

        return ExitCode::Ok;
    }

} // namespace KDC
