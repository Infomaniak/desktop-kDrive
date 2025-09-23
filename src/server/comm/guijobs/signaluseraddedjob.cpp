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

#include "signaluseraddedjob.h"
#include "libcommon/info/userinfo.h"
#include "libcommon/utility/utility.h"

static const auto outParamsUserInfo = "userInfo";

namespace KDC {

bool SignalUserAddedJob::serializeOutputParms() {
    // Output parameters serialization
    std::function<Poco::Dynamic::Var(const UserInfo &)> convertFct = [](const UserInfo &value) {
        Poco::DynamicStruct structValue;
        value.toStruct(structValue);
        return structValue;
    };
    writeParamValues(outParamsUserInfo, std::vector({_userInfo}), convertFct);

    if (!AbstractGuiJob::serializeOutputParms()) {
        _exitInfo = ExitCode::LogicError;
        return false;
    }

    return true;
}

} // namespace KDC
