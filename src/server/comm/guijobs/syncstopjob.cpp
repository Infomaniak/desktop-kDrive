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

#include "syncstopjob.h"
#include "appserver.h"
#include "requests/serverrequests.h"
#include "server/comm/guijobmanager.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

// Input parameters keys
static const auto inParamsSyncDbId = "syncDbId";

namespace KDC {

SyncStopJob::SyncStopJob(std::shared_ptr<CommManager> commManager, int requestId, const Poco::DynamicStruct &inParams,
                         std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::SYNC_STOP;
}

ExitInfo SyncStopJob::deserializeInputParms() {
    try {
        readParamValue(inParamsSyncDbId, _syncDbId);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in SyncStopJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo SyncStopJob::serializeOutputParms() {
    return ExitCode::Ok;
}

ExitInfo SyncStopJob::process() {
    // Stop SyncPal
    if (const auto exitInfo = _commManager->appServer().stopSyncPal(_syncDbId, true); !exitInfo) {
        LOG_WARN(_logger, "Error in stopSyncPal for syncDbId=" << _syncDbId << " : " << exitInfo);
        return exitInfo;
    }

    // Note: we do not Stop Vfs in case of a pause

    return ExitCode::Ok;
}

} // namespace KDC
