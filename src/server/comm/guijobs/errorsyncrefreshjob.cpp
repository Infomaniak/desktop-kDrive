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

#include "errorsyncrefreshjob.h"
#include "appserver.h"
#include "requests/serverrequests.h"
#include "requests/serverrequests.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"
#include "signalerrorremovedjob.h"

// Input parameters keys
static const auto inParamsSyncDbId = "syncDbId";

namespace KDC {

ErrorSyncRefreshJob::ErrorSyncRefreshJob(std::shared_ptr<CommManager> commManager, int32_t requestId,
                                         const Poco::DynamicStruct &inParams, std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::ERROR_SYNC_REFRESH;
}

ExitInfo ErrorSyncRefreshJob::deserializeInputParms() {
    try {
        readParamValue(inParamsSyncDbId, _syncDbId);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in ErrorSyncRefreshJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo ErrorSyncRefreshJob::serializeOutputParms() {
    return ExitCode::Ok;
}

ExitInfo ErrorSyncRefreshJob::process() {
    if (ExitInfo exitInfo = ServerRequests::deleteErrorsForSync(_syncDbId, false); !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ServerRequests::deleteErrorsForSync: " << exitInfo);
        return exitInfo;
    }

    if (ExitInfo exitInfo = ServerRequests::deleteErrorsForSync(_syncDbId, true); !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ServerRequests::deleteErrorsForSync: " << exitInfo);
        return exitInfo;
    }

    std::shared_ptr<SyncPal> syncpalPtr;

    if (ExitInfo exitInfo = getSyncPal(_syncDbId, syncpalPtr); !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Error in getSyncPal: " << exitInfo);
        return exitInfo;
    }

    if (!syncpalPtr) {
        LOG_WARN(Log::instance()->getLogger(), "SyncPal not found for syncDbId=" << _syncDbId);
        return ExitCode::DataError;
    }

    bool found = false;
    syncpalPtr->clearTmpBlacklist(found);

    if (found && syncpalPtr->isRunning()) {
        // Restart the sync to retry the synchronization of the items that were in the tmpblacklist
        // blacklist
        syncpalPtr->setRestart(true);

        const int maxWaitTime = 30000; // 30 seconds
        const auto startTime = std::chrono::steady_clock::now();

        if (syncpalPtr->step() == SyncStep::Idle) Utility::msleep(100);

        // Wait for the sync to be idle again before returning as the detection of the remaining errors is done in the executor
        while (syncpalPtr->step() != SyncStep::Idle &&
               std::chrono::steady_clock::now() - startTime < std::chrono::milliseconds(maxWaitTime)) {
            Utility::msleep(100);
        }
    }

    return ExitCode::Ok;
}

} // namespace KDC
