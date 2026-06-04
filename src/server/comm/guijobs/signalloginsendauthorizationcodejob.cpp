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

#include "signalloginsendauthorizationcodejob.h"

#include "libcommon/comm.h"

#include <utility>

// Signal: SignalNum::LOGIN_SEND_AUTHORIZATION_CODE

namespace KDC {

SignalLoginSendAuthorizationCodeJob::SignalLoginSendAuthorizationCodeJob(CommString code, CommString state) :
    _code(std::move(code)),
    _state(std::move(state)) {
    _signalNum = SignalNum::LOGIN_SEND_AUTHORIZATION_CODE;
}

ExitInfo SignalLoginSendAuthorizationCodeJob::serializeOutputParms() {
    writeParamValue(msgParamAuthCode, _code);
    writeParamValue(msgParamState, _state);

    return ExitCode::Ok;
}

} // namespace KDC
