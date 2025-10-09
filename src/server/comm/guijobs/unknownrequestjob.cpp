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

#include "unknownrequestjob.h"
#include "requests/serverrequests.h"
#include "signaluseraddedjob.h"
#include "signaluserupdatedjob.h"
#include "libcommon/info/userinfo.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

// Output parameters keys
static const auto outParamsError = "error";
static const auto outParamsErrorDescr = "errorDescr";

namespace KDC {

UnknownRequestJob::UnknownRequestJob(std::shared_ptr<CommManager> commManager, int requestId,
                                               const Poco::DynamicStruct &inParams,
                                               const std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::Unknown;
}

bool UnknownRequestJob::deserializeInputParms() {
    return true;
}

bool UnknownRequestJob::serializeOutputParms() {
    // Output parameters serialization
    writeParamValue(outParamsError, CommonUtility::str2CommString(_error));
    writeParamValue(outParamsErrorDescr, CommonUtility::str2CommString(_errorDescr));
    return true;
}

bool UnknownRequestJob::process() {
    _exitInfo = ExitCode::LogicError;
    _error = "unknown_request";
    _errorDescr = "The request is unknown by the server application.";
    return _exitInfo;
}

} // namespace KDC
