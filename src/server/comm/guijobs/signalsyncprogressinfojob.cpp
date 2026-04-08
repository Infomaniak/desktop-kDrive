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


// Signal: SignalNum::SYNC_PROGRESSINFO

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
    writeParamValue(msgParamSyncDbId, _syncDbId);
    writeParamValue(msgParamSyncStatus, _syncStatus);
    writeParamValue(msgParamSyncStep, _syncStep);
    writeParamValue(msgParamSyncProgress, _syncProgress, info2DynamicVar<SyncProgress>);
    return ExitCode::Ok;
}

} // namespace KDC
