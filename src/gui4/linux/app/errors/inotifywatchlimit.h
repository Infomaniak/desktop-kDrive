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

#include <QString>
#include <QtTypes>

#include <cstdint>

namespace KDC {

// Why the limit was not raised from the error row: the explanation dialog then shows the command to run instead.
enum class WatchLimitRaiseFailure : uint8_t {
    None,
    Cancelled, // The user dismissed the authentication dialog
    Failed,
};

/** Current inotify watches limit of the system, or 0 when it cannot be read. */
[[nodiscard]] qint64 readWatchLimit();
/** Limit to set, always above `currentLimit`. */
[[nodiscard]] qint64 proposedWatchLimit(qint64 currentLimit);
/** Shell command that sets `limit` for good, to be run by the user in a terminal. */
[[nodiscard]] QString buildWatchLimitCommand(qint64 limit);

} // namespace KDC
