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

#pragma once

#include "app/mainwindow/homecontroller.h"
#include "app/mainwindow/synccontrolstate.h"
#include "libcommon/utility/cstypes.h"

#include <optional>

namespace KDC {

/**
 * Resolves the mutually exclusive central Home status from connectivity and synchronization runtime state.
 *
 * Structured synchronization errors are intentionally excluded because Home renders them independently in its error banner.
 */
[[nodiscard]] HomeController::HomeStatus resolveHomeStatus(bool hasSync, bool offline, std::optional<SyncStatus> runtimeStatus);

/**
 * Resolves the pause or resume control of one synchronization, shared by the selected synchronization and the actions on
 * every synchronization.
 *
 * A blocked synchronization is restarted from its blocking error screen, and a synchronization with a start or stop request
 * in flight waits for its reply. Connectivity is left to the caller.
 */
[[nodiscard]] SyncControlState resolveSyncControlState(std::optional<SyncStatus> runtimeStatus, bool actionPending, bool blocked);

} // namespace KDC
