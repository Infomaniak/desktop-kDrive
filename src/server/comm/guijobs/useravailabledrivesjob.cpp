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

#include "useravailabledrivesjob.h"
#include "appserver.h"
#include "requests/serverrequests.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

// Input parameters keys
static const auto inParamsUserDbId = "userDbId";

// Output parameters keys
static const auto outParamsDriveAvailableInfoList = "driveAvailableInfoList";

namespace KDC {

UserAvailableDrivesJob::UserAvailableDrivesJob(std::shared_ptr<CommManager> commManager, int requestId,
                                               const Poco::DynamicStruct &inParams,
                                               std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::USER_AVAILABLEDRIVES;
}

ExitInfo UserAvailableDrivesJob::deserializeInputParms() {
    try {
        readParamValue(inParamsUserDbId, _userDbId);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in AbstractGuiJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo UserAvailableDrivesJob::serializeOutputParms() {
    writeParamValues(outParamsDriveAvailableInfoList, _driveAvailableInfoList, info2DynamicVar<DriveAvailableInfo>);

    return ExitCode::Ok;
}

ExitInfo UserAvailableDrivesJob::process() {
    ExitCode exitCode = ServerRequests::getUserAvailableDrives(_userDbId, _driveAvailableInfoList);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in Requests::getUserAvailableDrives");
        AppServer::addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
        return exitCode;
    }

    return ExitCode::Ok;
}

} // namespace KDC
