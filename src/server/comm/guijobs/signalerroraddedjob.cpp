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

#include "signalerroraddedjob.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"


// Signal: SignalNum::UTILITY_ERROR_ADDED

namespace KDC {

SignalErrorAddedJob::SignalErrorAddedJob(const ErrorInfo &errorInfo) :
    _errorInfo(errorInfo) {
    _signalNum = SignalNum::UTILITY_ERROR_ADDED;
}

ExitInfo SignalErrorAddedJob::serializeOutputParms() {
    writeParamValue(MSG_PARAM_ERROR_INFO, _errorInfo, info2DynamicVar<ErrorInfo>);
    return ExitCode::Ok;
}

} // namespace KDC
