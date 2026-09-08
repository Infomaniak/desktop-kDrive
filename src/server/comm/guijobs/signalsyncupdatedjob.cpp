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

#include "signalsyncupdatedjob.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"

// Output parameters keys
static const auto outParamsSyncInfo = "syncInfo";

namespace KDC {

SignalSyncUpdatedJob::SignalSyncUpdatedJob(const SyncInfo &syncInfo) :
    _syncInfo(syncInfo) {
    _signalNum = SignalNum::SYNC_UPDATED;
}

ExitInfo SignalSyncUpdatedJob::serializeOutputParms() {
    writeParamValue(outParamsSyncInfo, _syncInfo, info2DynamicVar<SyncInfo>);

    return ExitCode::Ok;
}

} // namespace KDC
