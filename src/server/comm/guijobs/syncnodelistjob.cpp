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

#include "syncnodelistjob.h"
#include "appserver.h"
#include "requests/serverrequests.h"
#include "server/comm/guijobmanager.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

// Input parameters keys
static const auto inParamsSyncDbId = "syncDbId";
static const auto inParamsSyncNodeType = "syncNodeType";

// Output parameters keys
static const auto outParamsNodeIdList = "nodeIdList";

namespace KDC {

SyncNodeListJob::SyncNodeListJob(std::shared_ptr<CommManager> commManager, int requestId, const Poco::DynamicStruct &inParams,
                                 std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::SYNCNODE_LIST;
}

ExitInfo SyncNodeListJob::deserializeInputParms() {
    try {
        readParamValue(inParamsSyncDbId, _syncDbId);
        readParamValue(inParamsSyncNodeType, _syncNodeType);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in SyncNodeListJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo SyncNodeListJob::serializeOutputParms() {
    writeParamValues(outParamsNodeIdList, _nodeIdList);
    return ExitCode::Ok;
}

ExitInfo SyncNodeListJob::process() {
    std::scoped_lock lock(_commManager->appServer().syncPalMapMutex);
    auto &syncPalMap = _commManager->appServer().syncPalMap;
    auto it = syncPalMap.find(_syncDbId);
    if (it == syncPalMap.end()) {
        LOG_WARN(_logger, "SyncNodeListJob::process: SyncPal not found for syncDbId=" << _syncDbId);
        return ExitCode::DataError;
    }

    NodeSet nodeIdSet;
    if (ExitInfo exitInfo = it->second->syncIdSet(_syncNodeType, nodeIdSet); !exitInfo) {
        return exitInfo;
    }
    _nodeIdList.assign(nodeIdSet.begin(), nodeIdSet.end());
    return ExitCode::Ok;
}

} // namespace KDC
