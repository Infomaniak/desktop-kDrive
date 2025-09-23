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

#include "loginrequesttokenjob.h"
#include "requests/serverrequests.h"
#include "signaluseraddedjob.h"
#include "signaluserupdatedjob.h"
#include "libcommon/info/userinfo.h"
#include "libcommon/utility/utility.h"

static const auto inParamsCode = "code";
static const auto inParamsCodeVerifier = "codeVerifier";

static const auto outParamsUserDbId = "userDbId";
static const auto outParamsError = "error";
static const auto outParamsErrorDescr = "errorDescr";

namespace KDC {

bool LoginRequestTokenJob::deserializeInputParms() {
    if (!AbstractGuiJob::deserializeInputParms()) {
        _exitInfo = ExitCode::LogicError;
        return false;
    }

    // Input parameters deserialization
    try {
        readParamValue(inParamsCode, _code);
        readParamValue(inParamsCodeVerifier, _codeVerifier);
    } catch (std::exception &) {
        _exitInfo = ExitCode::LogicError;
        return false;
    }

    return true;
}

bool LoginRequestTokenJob::serializeOutputParms() {
    // Output parameters serialization
    if (_exitInfo) {
        writeParamValue(outParamsUserDbId, _userDbId);
    } else {
        writeParamValue(outParamsError, CommonUtility::str2CommString(_error));
        writeParamValue(outParamsErrorDescr, CommonUtility::str2CommString(_errorDescr));
    }

    if (!AbstractGuiJob::serializeOutputParms()) {
        _exitInfo = ExitCode::LogicError;
        return false;
    }

    return true;
}

bool LoginRequestTokenJob::process() {
    UserInfo userInfo;
    bool userCreated;
    ExitCode exitCode =
            ServerRequests::requestToken(CommonUtility::commString2Str(_code), CommonUtility::commString2Str(_codeVerifier),
                                         userInfo, userCreated, _error, _errorDescr);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in Requests::requestToken: code=" << exitCode);
        _exitInfo = exitCode;
        return false;
    }
    _userDbId = userInfo.dbId();
    _commManager->updateSentryUserCbk();
    if (userCreated) {
        std::unique_ptr<SignalUserAddedJob> signalUserAddedJob =
                std::make_unique<SignalUserAddedJob>(nullptr, userInfo, _channel);
        signalUserAddedJob->runJob();
    } else {
        std::unique_ptr<SignalUserUpdatedJob> signalUserUpdatedJob =
                std::make_unique<SignalUserUpdatedJob>(nullptr, userInfo, _channel);
        signalUserUpdatedJob->runJob();
    }
    return true;
}

} // namespace KDC
