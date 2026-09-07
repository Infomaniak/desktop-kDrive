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

#include "parametersinfojob.h"
#include "appserver.h"

#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"


// Output parameters keys
static const auto outParamsParametersInfo = "parametersInfo";


namespace KDC {

ParametersInfoJob::ParametersInfoJob(std::shared_ptr<CommManager> commManager, int requestId, const Poco::DynamicStruct &inParams,
                                     std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::PARAMETERS_INFO;
}


ExitInfo ParametersInfoJob::serializeOutputParms() {
    writeParamValue(outParamsParametersInfo, _parametersInfo, info2DynamicVar<ParametersInfo>);

    return ExitCode::Ok;
}

ExitInfo ParametersInfoJob::process() {
    if (const auto exitCode = ServerRequests::getParameters(_parametersInfo); exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in Requests::getParameters");
        addError(Error(ERR_ID, exitCode));

        return exitCode;
    }

    return ExitCode::Ok;
}

} // namespace KDC
