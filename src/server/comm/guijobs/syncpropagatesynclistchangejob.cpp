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

#include "syncpropagatesynclistchangejob.h"
#include "appserver.h"
#include "requests/serverrequests.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

// Input parameters keys
static const auto inParamsSyncDbId = "syncDbId";
static const auto inParamsRestartSync = "restartSync";

namespace KDC {

SyncPropagateSyncListChangeJob::SyncPropagateSyncListChangeJob(std::shared_ptr<CommManager> commManager, int requestId,
                                                               const Poco::DynamicStruct &inParams,
                                                               std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::SYNC_PROPAGATE_SYNCLIST_CHANGE;
}

ExitInfo SyncPropagateSyncListChangeJob::deserializeInputParms() {
    try {
        readParamValue(inParamsSyncDbId, _syncDbId);
        readParamValue(inParamsRestartSync, _restartSync);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in AbstractGuiJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo SyncPropagateSyncListChangeJob::process() {
    const std::scoped_lock lock(AppServer::syncPalMapMutex);

    auto syncPalMapIt = _commManager->appServer().syncPalMap.find(_syncDbId);
    if (syncPalMapIt == _commManager->appServer().syncPalMap.end()) {
        LOG_WARN(_logger, "SyncPal not found in syncPalMap for syncDbId=" << _syncDbId);

        return ExitCode::DataError;
    }

    if (!syncPalMapIt->second) {
        LOG_WARN(_logger, "SyncPal not set in syncPalMap for syncDbId=" << _syncDbId);

        return ExitCode::DataError;
    }

    if (const auto exitCode = syncPalMapIt->second->syncListUpdated(_restartSync); exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in SyncPal::syncListUpdated for syncDbId=" << _syncDbId);

        return exitCode;
    }

    return ExitCode::Ok;
}

} // namespace KDC
