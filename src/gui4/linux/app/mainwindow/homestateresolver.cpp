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

using HomeStatus = HomeController::HomeStatus;

HomeStatus resolveHomeStatus(const bool hasSync, const bool offline, const std::optional<SyncStatus> runtimeStatus) {
    if (!hasSync) {
        return HomeStatus::SetupRequired;
    }
    if (!runtimeStatus.has_value()) {
        return HomeStatus::Loading;
    }

    switch (*runtimeStatus) {
        case SyncStatus::Running:
            return HomeStatus::Syncing;
        case SyncStatus::Idle:
            return offline ? HomeStatus::Offline : HomeStatus::UpToDate;
        case SyncStatus::Paused:
            return offline ? HomeStatus::Offline : HomeStatus::Paused;
        case SyncStatus::Stopped:
        case SyncStatus::Error:
            return HomeStatus::Paused;
        case SyncStatus::Starting:
        case SyncStatus::PauseAsked:
        case SyncStatus::StopAsked:
        case SyncStatus::Undefined:
        case SyncStatus::EnumEnd:
            return HomeStatus::Loading;
    }
    return HomeStatus::Loading;
}

} // namespace KDC
