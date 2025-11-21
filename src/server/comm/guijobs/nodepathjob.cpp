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

#include "nodepathjob.h"
#include "appserver.h"

#include "requests/serverrequests.h"
#include "requests/syncnodecache.h"

#include "server/comm/guijobmanager.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

// Input parameters keys
static const auto inParamsSyncDbId = "syncDbId";
static const auto inParamsNodeId = "nodeId";

// Output parameters keys
static const auto outParamsPath = "path";

namespace KDC {

NodePathJob::NodePathJob(std::shared_ptr<CommManager> commManager, int requestId, const Poco::DynamicStruct &inParams,
                         std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::SYNC_ADD;
}

ExitInfo NodePathJob::deserializeInputParms() {
    try {
        readParamValue(inParamsSyncDbId, _syncDbId);
        readParamValue(inParamsNodeId, _nodeId);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in NodePathJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo NodePathJob::serializeOutputParms() {
    writeParamValue(outParamsPath, _path);

    return ExitCode::Ok;
}

ExitInfo NodePathJob::process() {
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

    if (const auto exitInfo = ServerRequests::getPathByNodeId(syncPalMapIt->second->userDbId(), syncPalMapIt->second->driveId(),
                                                              _nodeId, _path);
        !exitInfo) {
        if (exitInfo.cause() == ExitCause::NotFound) {
            (void) SyncNodeCache::instance()->deleteSyncNode(_syncDbId, _nodeId);
        } else {
            LOG_WARN(_logger, "Error in AppServer::getPathByNodeId: " << exitInfo);
            AppServer::addError(Error(ERR_ID, exitInfo.code(), exitInfo.cause()));
        }
    }

    return ExitCode::Ok;
}

} // namespace KDC
