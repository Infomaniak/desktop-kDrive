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

#include "nodeinfojob.h"
#include "appserver.h"
#include "requests/serverrequests.h"
#include "server/comm/guijobmanager.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

// Input parameters keys
static const auto inParamsUserDbId = "userDbId";
static const auto inParamsDriveId = "driveId";
static const auto inParamsNodeId = "nodeId";
static const auto inParamsWithPath = "withPath";

// Output parameters keys
static const auto outParamsNodeInfo = "nodeInfo";

namespace KDC {

NodeInfoJob::NodeInfoJob(std::shared_ptr<CommManager> commManager, int requestId, const Poco::DynamicStruct &inParams,
                         std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::NODE_INFO;
}

ExitInfo NodeInfoJob::deserializeInputParms() {
    try {
        readParamValue(inParamsUserDbId, _userDbId);
        readParamValue(inParamsDriveId, _driveId);
        readParamValue(inParamsNodeId, _nodeId);
        readParamValue(inParamsWithPath, _withPath);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in AbstractGuiJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo NodeInfoJob::serializeOutputParms() {
    // Output parameters serialization
    std::function<Poco::Dynamic::Var(const NodeInfo &)> nodeInfo2DynamicVar = [](const NodeInfo &value) {
        Poco::DynamicStruct structValue;
        value.toDynamicStruct(structValue);
        return structValue;
    };
    writeParamValue(outParamsNodeInfo, _nodeInfo, nodeInfo2DynamicVar);

    return ExitCode::Ok;
}

ExitInfo NodeInfoJob::process() {
    return ServerRequests::getNodeInfo(_userDbId, _driveId, _nodeId, _nodeInfo, _withPath);
}

} // namespace KDC
