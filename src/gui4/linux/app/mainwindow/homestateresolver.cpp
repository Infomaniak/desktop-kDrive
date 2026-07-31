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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "homestateresolver.h"

namespace KDC {

ResolvedHomeStatus resolveHomeStatus(const bool hasSync, const bool offline, const std::optional<SyncStatus> runtimeStatus) {
    if (!hasSync) {
        return ResolvedHomeStatus::SetupRequired;
    }
    if (!runtimeStatus.has_value()) {
        return ResolvedHomeStatus::Loading;
    }

    switch (*runtimeStatus) {
        case SyncStatus::Starting:
        case SyncStatus::Running:
        case SyncStatus::PauseAsked:
        case SyncStatus::StopAsked:
            return ResolvedHomeStatus::Syncing;
        case SyncStatus::Idle:
            return ResolvedHomeStatus::UpToDate;
        case SyncStatus::Paused:
        case SyncStatus::Stopped:
        case SyncStatus::Error:
            return offline ? ResolvedHomeStatus::Offline : ResolvedHomeStatus::Paused;
        case SyncStatus::Undefined:
        case SyncStatus::EnumEnd:
            return ResolvedHomeStatus::Loading;
    }
    return ResolvedHomeStatus::Loading;
}

} // namespace KDC
