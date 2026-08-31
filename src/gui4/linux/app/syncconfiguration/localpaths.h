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

#pragma once

#include <QString>

#include <functional>

namespace KDC {

/**
 * Converts a local path to its display form.
 *
 * The user home prefix is replaced with the `~` shorthand Linux users expect. The returned value is presentation only:
 * every filesystem operation and every validation must keep using the original absolute path.
 */
[[nodiscard]] QString displayLocalPath(const QString &localPath);

/** Reports whether two synchronization folders would collide, either by being equal or by containing each other. */
[[nodiscard]] bool localPathsOverlap(const QString &lhs, const QString &rhs);

/**
 * Derives a free synchronization folder from a path the server proposed.
 *
 * DUPLICATED LOGIC, ON PURPOSE. `ServerRequests::findGoodPathForNewSync()` already derives a free folder, but it can
 * only see what exists on disk and the synchronizations stored in its database. During onboarding neither exists yet:
 * no `SYNC_ADD` has run, so no folder has been created and no sync has been persisted. Two drives sharing a name
 * therefore receive the very same path from the server, and the collision only surfaces at the end of onboarding when
 * the second `SYNC_ADD` is rejected. The macOS and Windows clients have that bug. Reserving the path client-side is
 * what prevents it, because only the client knows what the current selection has already claimed.
 *
 * `taken` reports the paths already reserved by the caller. Returns an empty string when no free path was found.
 */
[[nodiscard]] QString makeUniqueLocalPath(const QString &path, const std::function<bool(const QString &)> &taken);

} // namespace KDC
