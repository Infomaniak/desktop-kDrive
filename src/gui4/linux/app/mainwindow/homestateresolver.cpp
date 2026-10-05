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
        // As on macOS, a starting synchronization is presented as syncing, so a resume is acknowledged at once.
        case SyncStatus::Starting:
        case SyncStatus::Running:
            return HomeStatus::Syncing;
        // An undefined status means the server has not reported the synchronization yet: it only starts its synchronizations
        // a few seconds after launch. As on macOS, Home presents it as up to date rather than as an empty loading state.
        case SyncStatus::Undefined:
        case SyncStatus::Idle:
            return offline ? HomeStatus::Offline : HomeStatus::UpToDate;
        case SyncStatus::Paused:
            return offline ? HomeStatus::Offline : HomeStatus::Paused;
        // A pause request is presented as paused at once, as `Starting` is presented as syncing.
        case SyncStatus::PauseAsked:
        case SyncStatus::StopAsked:
        case SyncStatus::Stopped:
        case SyncStatus::Error:
            return HomeStatus::Paused;
        case SyncStatus::EnumEnd:
            return HomeStatus::Loading;
    }
    return HomeStatus::Loading;
}

} // namespace KDC
