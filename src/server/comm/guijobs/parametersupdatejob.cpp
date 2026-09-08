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

#include "parametersupdatejob.h"

#include "appserver.h"
#include "libcommon/comm.h"

// Output parameters keys
static const auto inParamsParametersInfo = "parametersInfo";

namespace KDC {

ParametersUpdateJob::ParametersUpdateJob(std::shared_ptr<CommManager> commManager, int requestId,
                                         const Poco::DynamicStruct &inParams, std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::PARAMETERS_UPDATE;
}

ExitInfo ParametersUpdateJob::deserializeInputParms() {
    constexpr auto logMessage = "Exception in ParametersUpdateJob::readParamValue: error=";
    try {
        readParamValue(inParamsParametersInfo, _parametersInfo, dynamicVar2Struct<ParametersInfo>);
    } catch (const Poco::Exception &pocoException) {
        LOG_WARN(_logger, logMessage << pocoException.message());

        return ExitCode::LogicError;
    } catch (const CommonUtility::InvalidEnumerationValue &cuException) {
        LOG_WARN(_logger, logMessage << cuException.what());

        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo ParametersUpdateJob::process() {
    return _commManager->appServer().updateParametersAndPropagateChanges(_parametersInfo);
}

} // namespace KDC
