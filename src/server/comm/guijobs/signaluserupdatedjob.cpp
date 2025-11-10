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

#include "signaluserupdatedjob.h"
#include "libcommon/info/userinfo.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"

// Output parameters keys
static const auto outParamsUserInfo = "userInfo";

namespace KDC {

SignalUserUpdatedJob::SignalUserUpdatedJob(std::shared_ptr<CommManager> commManager,
                                           const std::shared_ptr<AbstractCommChannel> channel, const UserInfo &userInfo) :
    AbstractGuiJob(commManager, channel),
    _userInfo(userInfo) {
    _signalNum = SignalNum::USER_UPDATED;
}

ExitInfo SignalUserUpdatedJob::serializeOutputParms([[maybe_unused]] bool hasError /*= false*/) {
    // Output parameters serialization
    std::function<Poco::Dynamic::Var(const UserInfo &)> userInfo2DynamicVar = [](const UserInfo &value) {
        Poco::DynamicStruct structValue;
        value.toDynamicStruct(structValue);
        return structValue;
    };
    writeParamValue(outParamsUserInfo, _userInfo, userInfo2DynamicVar);
    return ExitCode::Ok;
}

} // namespace KDC
