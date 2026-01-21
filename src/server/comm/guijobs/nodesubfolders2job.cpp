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

#include "nodesubfolders2job.h"
#include "appserver.h"
#include "requests/serverrequests.h"
#include "server/comm/guijobmanager.h"
#include "libcommon/info/userinfo.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "../../../libcommon/log/log.h"

// Input parameters keys
static const auto inParamsDriveDbId = "driveDbId";
static const auto inParamsNodeId = "nodeId";
static const auto inParamsWithPath = "withPath";

// Output parameters keys
static const auto outParamsNodeSubFolderInfoList = "nodeSubFolderInfoList";

namespace KDC {

NodeSubFolders2Job::NodeSubFolders2Job(std::shared_ptr<CommManager> commManager, int requestId,
                                       const Poco::DynamicStruct &inParams, std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::NODE_SUBFOLDERS2;
}

ExitInfo NodeSubFolders2Job::deserializeInputParms() {
    constexpr auto logMessage = "Exception in NodeSubFolders2Job::readParamValue: error=";
    try {
        readParamValue(inParamsDriveDbId, _driveDbId);
        readParamValue(inParamsNodeId, _nodeId);
        readParamValue(inParamsWithPath, _withPath);
    } catch (const Poco::Exception &pocoException) {
        LOG_WARN(_logger, logMessage << pocoException.message());

        return ExitCode::LogicError;
    } catch (const CommonUtility::InvalidEnumerationValue &cuException) {
        LOG_WARN(_logger, logMessage << cuException.what());

        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo NodeSubFolders2Job::serializeOutputParms() {
    writeParamValues(outParamsNodeSubFolderInfoList, _nodeSubFolderInfoList, info2DynamicVar<NodeInfo>);

    return ExitCode::Ok;
}

ExitInfo NodeSubFolders2Job::process() {
    if (const auto exitInfo = ServerRequests::getSubFolders(_driveDbId, _nodeId, _nodeSubFolderInfoList, _withPath);
        exitInfo.code() != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in Requests::getSubFolders");
        AppServer::addError(Error(ERR_ID, exitInfo));

        return exitInfo;
    }

    return ExitCode::Ok;
}

} // namespace KDC
