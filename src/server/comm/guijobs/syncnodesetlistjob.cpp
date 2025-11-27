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

#include "syncnodesetlistjob.h"
#include "appserver.h"
#include "requests/serverrequests.h"
#include "server/comm/guijobmanager.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

// Input parameters keys
static const auto inParamsSyncDbId = "syncDbId";
static const auto inParamsSyncNodeType = "syncNodeType";
static const auto inParamsNodeIdList = "nodeIdList";

namespace KDC {

SyncNodeSetListJob::SyncNodeSetListJob(std::shared_ptr<CommManager> commManager, int requestId,
                                       const Poco::DynamicStruct &inParams, std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::SYNCNODE_SETLIST;
}

ExitInfo SyncNodeSetListJob::deserializeInputParms() {
    try {
        readParamValue(inParamsSyncDbId, _syncDbId);
        readParamValue(inParamsSyncNodeType, _syncNodeType);
        readParamValues(inParamsNodeIdList, _nodeIdList);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in SyncNodeSetListJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo SyncNodeSetListJob::process() {
    std::scoped_lock lock(_commManager->appServer().syncPalMapMutex);
    auto &syncPalMap = _commManager->appServer().syncPalMap;
    auto it = syncPalMap.find(_syncDbId);
    if (it == syncPalMap.end()) {
        LOG_WARN(_logger, "SyncNodeSetListJob::process: SyncPal not found for syncDbId=" << _syncDbId);
        return ExitCode::DataError;
    }

    NodeSet nodeIdSet;
    nodeIdSet.insert(_nodeIdList.begin(), _nodeIdList.end());
    if (ExitInfo exitInfo = it->second->setSyncIdSet(_syncNodeType, nodeIdSet); !exitInfo) {
        LOG_WARN(_logger, "SyncNodeSetListJob::process: setSyncIdSet failed for syncDbId=" << _syncDbId
                                                                                           << ", syncNodeType=" << _syncNodeType);
        AppServer::addError(Error(ERR_ID, exitInfo.code(), exitInfo.cause()));
        return exitInfo;
    }

    if (ExitInfo exitInfo = it->second->syncListUpdated(it->second->isRunning()); !exitInfo) {
        LOG_WARN(_logger, "SyncNodeSetListJob::process: syncListUpdated failed for syncDbId=" << _syncDbId << ", syncNodeType="
                                                                                              << _syncNodeType);
        AppServer::addError(Error(ERR_ID, exitInfo.code(), exitInfo.cause()));
        return exitInfo;
    }
    return ExitCode::Ok;
}
} // namespace KDC
