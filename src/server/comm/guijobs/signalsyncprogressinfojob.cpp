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

#include "signalsyncprogressinfojob.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"

// Output parameters keys
static const auto outParamsSyncDbId = "syncDbId";
static const auto outParamsSyncstatus = "syncStatus";
static const auto outParamsSyncStep = "syncStep";
static const auto outParamsSyncProgress = "SyncProgress";

namespace KDC {

SignalSyncProgressInfoJob::SignalSyncProgressInfoJob(int syncDbId, SyncStatus syncStatus, SyncStep syncStep,
                                                     const SyncProgress &syncProgress) :
    _syncDbId(syncDbId),
    _syncStatus(syncStatus),
    _syncStep(syncStep),
    _syncProgress(syncProgress) {
    _signalNum = SignalNum::SYNC_PROGRESSINFO;
}

ExitInfo SignalSyncProgressInfoJob::serializeOutputParms() {
    writeParamValue(outParamsSyncDbId, _syncDbId);
    writeParamValue(outParamsSyncstatus, _syncStatus);
    writeParamValue(outParamsSyncStep, _syncStep);
    writeParamValue(outParamsSyncProgress, _syncProgress, info2DynamicVar<SyncProgress>);
    return ExitCode::Ok;
}

} // namespace KDC
