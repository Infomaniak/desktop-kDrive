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

#include "userinfolistjob.h"
#include "../guijobmanager.h"
#include "requests/serverrequests.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

// Output parameters keys
static const auto outParamsUserInfoList = "userInfoList";

namespace KDC {

UserInfoListJob::UserInfoListJob(std::shared_ptr<CommManager> commManager, int requestId, const Poco::DynamicStruct &inParams,
                                 std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::USER_INFOLIST;
}

ExitInfo UserInfoListJob::deserializeInputParms() {
    return ExitCode::Ok;
}

ExitInfo UserInfoListJob::serializeOutputParms() {
    // Output parameters serialization
    std::function<Poco::Dynamic::Var(const UserInfo &)> userInfo2DynamicVar = [](const UserInfo &value) {
        Poco::DynamicStruct structValue;
        value.toDynamicStruct(structValue);
        return structValue;
    };
    writeParamValues(outParamsUserInfoList, _userInfoList, userInfo2DynamicVar);

    return ExitCode::Ok;
}

ExitInfo UserInfoListJob::process() {
    ExitCode exitCode = ServerRequests::getUserInfoList(_userInfoList);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in ServerRequests::getUserInfoList: code=" << exitCode);
    }

    return exitCode;
}

} // namespace KDC
