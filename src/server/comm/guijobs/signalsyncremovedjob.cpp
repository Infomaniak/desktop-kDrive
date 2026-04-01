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

#include "signalsyncremovedjob.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"

// Output parameters keys
static const auto outParamsSyncDbId = "syncDbId";

namespace KDC {

SignalSyncRemovedJob::SignalSyncRemovedJob(int syncDbId) :
    _syncDbId(syncDbId) {
    _signalNum = SignalNum::SYNC_REMOVED;
}

ExitInfo SignalSyncRemovedJob::serializeOutputParms() {
    writeParamValue(MSG_PARAM_SYNC_DB_ID, _syncDbId);
    return ExitCode::Ok;
}

} // namespace KDC
