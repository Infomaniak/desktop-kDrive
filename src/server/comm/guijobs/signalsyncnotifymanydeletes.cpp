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

#include "signalsyncnotifymanydeletes.h"

// Output parameters keys
static const auto outParamsSyncDbId = "syncDbId";
static const auto outParamsSyncNotificationType = "notificationType";

namespace KDC {

SignalSyncNotifyManyDeletes::SignalSyncNotifyManyDeletes(const SyncDbId syncDbId,
                                                         const TooManyDeletesNotificationType notificationType) :
    _syncDbId(syncDbId),
    _notificationType(notificationType) {
    _signalNum = SignalNum::SYNC_NOTIFY_MANY_DELETES;
}

ExitInfo SignalSyncNotifyManyDeletes::serializeOutputParms() {
    writeParamValue(outParamsSyncDbId, _syncDbId);
    writeParamValue(outParamsSyncNotificationType, _notificationType);
    return ExitCode::Ok;
}

} // namespace KDC
