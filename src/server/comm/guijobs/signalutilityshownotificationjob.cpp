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

#include "signalutilityshownotificationjob.h"

#include "libcommon/comm.h"


// Signal: SignalNum::UTILITY_SHOW_NOTIFICATION

namespace KDC {

SignalUtilityShowNotificationJob::SignalUtilityShowNotificationJob(CommString title, CommString message) :
    _title(std::move(title)),
    _message(std::move(message)) {
    _signalNum = SignalNum::UTILITY_SHOW_NOTIFICATION;
}

ExitInfo SignalUtilityShowNotificationJob::serializeOutputParms() {
    writeParamValue(MSG_PARAM_TITLE, _title);
    writeParamValue(MSG_PARAM_MESSAGE, _message);

    return ExitCode::Ok;
}

} // namespace KDC
