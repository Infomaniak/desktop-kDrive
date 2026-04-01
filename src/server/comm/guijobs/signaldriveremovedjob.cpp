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

#include "signaldriveremovedjob.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"


// Signal: SignalNum::DRIVE_REMOVED
// serializeOutputParms() writes: {
//   MSG_PARAM_DRIVE_DB_ID ("driveDbId"): DriveDbId
// }

namespace KDC {

SignalDriveRemovedJob::SignalDriveRemovedJob(int driveDbId) :
    _driveDbId(driveDbId) {
    _signalNum = SignalNum::DRIVE_REMOVED;
}

ExitInfo SignalDriveRemovedJob::serializeOutputParms() {
    writeParamValue(MSG_PARAM_DRIVE_DB_ID, _driveDbId);
    return ExitCode::Ok;
}

} // namespace KDC
