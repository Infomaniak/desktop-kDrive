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

#include "signalaccountaddedjob.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"

// Signal: SignalNum::ACCOUNT_ADDED

namespace KDC {

SignalAccountAddedJob::SignalAccountAddedJob(const AccountInfo &accountInfo) :
    _accountInfo(accountInfo) {
    _signalNum = SignalNum::ACCOUNT_ADDED;
}

ExitInfo SignalAccountAddedJob::serializeOutputParms() {
    writeParamValue(MSG_PARAM_ACCOUNT_INFO, _accountInfo, info2DynamicVar<AccountInfo>);
    return ExitCode::Ok;
}

} // namespace KDC
