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

#include "exclappgetlistjob.h"
#include "appserver.h"
#include "requests/serverrequests.h"

#include "libcommon/info/userinfo.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

// Input parameters keys
static const auto inParamsDefault = "default";

// Output parameters keys
static const auto outParamsApplicationList = "applicationList";


namespace KDC {

ExclAppGetListJob::ExclAppGetListJob(std::shared_ptr<CommManager> commManager, int requestId, const Poco::DynamicStruct &inParams,
                                     std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::EXCLAPP_GETLIST;
}

ExitInfo ExclAppGetListJob::deserializeInputParms() {
    try {
        readParamValue(inParamsDefault, _default);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in ExclAppGetListJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo ExclAppGetListJob::serializeOutputParms() {
    writeParamValues(outParamsApplicationList, _applicationList, info2DynamicVar<ExclusionAppInfo>);

    return ExitCode::Ok;
}

ExitInfo ExclAppGetListJob::process() {
    if (const auto exitCode = ServerRequests::getExclusionAppList(_default, _applicationList); exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in Requests::getExclusionAppList: code=" << exitCode);
        addError(Error(ERR_ID, exitCode));

        return exitCode;
    }

    return ExitCode::Ok;
}

} // namespace KDC
