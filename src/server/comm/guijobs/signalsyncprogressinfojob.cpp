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
// serializeOutputParms() writes: {
//   MSG_PARAM_SYNC_DB_ID ("syncDbId"):       SyncDbId,
//   MSG_PARAM_SYNC_STATUS ("syncStatus"):    SyncStatus,
//   MSG_PARAM_SYNC_STEP ("syncStep"):        SyncStep,
//   MSG_PARAM_SYNC_PROGRESS ("SyncProgress"): {
//     MSG_PARAM_CURRENT_FILE ("currentFile"):                        int64_t,
//     MSG_PARAM_TOTAL_FILES ("totalFiles"):                          int64_t,
//     MSG_PARAM_COMPLETED_SIZE ("completedSize"):                    int64_t,
//     MSG_PARAM_TOTAL_SIZE ("totalSize"):                            int64_t,
//     MSG_PARAM_ESTIMATED_REMAINING_TIME ("estimatedRemainingTime"): int64_t
//   }
// }

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
    writeParamValue(MSG_PARAM_SYNC_DB_ID, _syncDbId);
    writeParamValue(MSG_PARAM_SYNC_STATUS, _syncStatus);
    writeParamValue(MSG_PARAM_SYNC_STEP, _syncStep);
    writeParamValue(MSG_PARAM_SYNC_PROGRESS, _syncProgress, info2DynamicVar<SyncProgress>);
    return ExitCode::Ok;
}

} // namespace KDC
