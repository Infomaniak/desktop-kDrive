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

#include "signalsyncfileprogressinfo.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"

static const auto outParamsSyncDbId = "syncDbId";
static const auto outParamsItemInfo = "itemInfo";
static const auto outParamsProgress = "progress";

namespace KDC {

SignalSyncFileProgressInfo::SignalSyncFileProgressInfo(int syncDbId, const SyncFileItemInfo &itemInfo, int progress) :
    _syncDbId(syncDbId),
    _itemInfo(itemInfo),
    _progress(progress) {
    _signalNum = SignalNum::SYNC_FILEPROGRESSINFO;
}

ExitInfo SignalSyncFileProgressInfo::serializeOutputParms() {
    writeParamValue(outParamsSyncDbId, _syncDbId);
    writeParamValue(outParamsItemInfo, _itemInfo, info2DynamicVar<SyncFileItemInfo>);
    writeParamValue(outParamsProgress, _progress);
    return ExitCode::Ok;
}

} // namespace KDC
