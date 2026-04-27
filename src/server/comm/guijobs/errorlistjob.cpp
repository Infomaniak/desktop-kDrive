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

#include "errorlistjob.h"
#include "appserver.h"
#include "requests/serverrequests.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

// Output parameters keys
static const auto outParamsError = "errorInfoList";

namespace KDC {

ErrorListJob::ErrorListJob(std::shared_ptr<CommManager> commManager, int requestId, const Poco::DynamicStruct &inParams,
                           std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::ERROR_INFOLIST;
}

ExitInfo ErrorListJob::deserializeInputParms() {
    try {
        readParamValue(msgParamLimit, _limit);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in ErrorListJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }
    return ExitCode::Ok;
}

ExitInfo ErrorListJob::serializeOutputParms() {
    writeParamValues(outParamsError, _errorList, info2DynamicVar<Error>);
    return ExitCode::Ok;
}

ExitInfo ErrorListJob::process() {
    ExitInfo exitInfo = ServerRequests::getErrorList(_limit, _errorList);
    if (!exitInfo) {
        LOG_WARN(_logger, "Error in ServerRequests::getErrorList: " << exitInfo);
        addError(Error(ERR_ID, exitInfo));
        return exitInfo;
    }

    return ExitCode::Ok;
}

} // namespace KDC
