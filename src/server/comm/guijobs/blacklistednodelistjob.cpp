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

#include "blacklistednodelistjob.h"
#include "appserver.h"
#include "requests/serverrequests.h"
#include "server/comm/guijobmanager.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

// Input parameters keys
static const auto inParamsSyncDbId = "syncDbId";

// Output parameters keys
static const auto outParamsNodeIdList = "nodeIdList";

namespace KDC {

BlacklistedNodeListJob::BlacklistedNodeListJob(std::shared_ptr<CommManager> commManager, int requestId,
                                               const Poco::DynamicStruct &inParams,
                                               std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::BLACKLISTED_NODE_LIST;
}

ExitInfo BlacklistedNodeListJob::deserializeInputParms() {
    try {
        readParamValue(inParamsSyncDbId, _syncDbId);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in AbstractGuiJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo BlacklistedNodeListJob::serializeOutputParms() {
    // Output parameters serialization
    writeParamValues(outParamsNodeIdList, _nodeIdList);

    return ExitCode::Ok;
}

ExitInfo BlacklistedNodeListJob::process() {
    std::shared_ptr<SyncPal> syncPal;
    if (ExitInfo exitInfo = getSyncPal(_syncDbId, syncPal); !exitInfo) {
        return exitInfo;
    }

    NodeSet nodeIdSet;
    if (ExitInfo exitInfo = syncPal->syncIdSet(SyncNodeType::BlackList, nodeIdSet); !exitInfo) {
        LOG_WARN(_logger, "Error in SyncPal::setSyncIdSet: " << exitInfo);
        addError(Error(ERR_ID, exitInfo.code(), exitInfo.cause()));
        return exitInfo;
    }
    _nodeIdList.assign(nodeIdSet.begin(), nodeIdSet.end());
    return ExitCode::Ok;
}

} // namespace KDC
