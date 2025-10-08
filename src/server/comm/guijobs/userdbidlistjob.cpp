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

#include "userdbidlistjob.h"
#include "../guijobmanager.h"
#include "requests/serverrequests.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

// Output parameters keys
static const auto outParamsUserDbIdList = "userDbIdList";

namespace KDC {

UserDbIdListJob::UserDbIdListJob(std::shared_ptr<CommManager> commManager, int requestId, const Poco::DynamicStruct &inParams,
                                 const std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::USER_DBIDLIST;
}

bool UserDbIdListJob::deserializeInputParms() {
    return true;
}

bool UserDbIdListJob::serializeOutputParms() {
    // Output parameters serialization
    writeParamValue(outParamsUserDbIdList, _userDbIdList);

    return true;
}

bool UserDbIdListJob::process() {
    ExitCode exitCode = ServerRequests::getUserDbIdList(_userDbIdList);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in ServerRequests::getUserDbIdList: code=" << exitCode);
        _exitInfo = exitCode;
        return false;
    }

    return _exitInfo;
}

} // namespace KDC
