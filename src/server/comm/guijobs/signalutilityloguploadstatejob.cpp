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

#include "signalutilityloguploadstatejob.h"

#include "libcommon/comm.h"

// Output parameters keys
static const auto outParamsLogUploadState = "state";
static const auto outParamsPercentage = "percentage";

namespace KDC {

SignalUtilityLogUploadStateJob::SignalUtilityLogUploadStateJob(const LogUploadState logUploadState, const int32_t percentage) :
    _state(logUploadState),
    _percentage(percentage) {
    _signalNum = SignalNum::UTILITY_LOG_UPLOAD_STATUS_UPDATED;
}

ExitInfo SignalUtilityLogUploadStateJob::serializeOutputParms() {
    writeParamValue(outParamsLogUploadState, _state);
    writeParamValue(outParamsPercentage, _percentage);

    return ExitCode::Ok;
}

} // namespace KDC
