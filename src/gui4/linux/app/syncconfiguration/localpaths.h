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
 * Two selected drives never contend for the same proposed path: the server names the folder after the drive, and the
 * backend keeps drive names unique by suffixing the most recent one, so `kDrive Foo` and `kDrive Foo (1)` land in
 * distinct folders. What this guards against is a drive whose folder the user has already moved by hand to the very
 * place another selected drive is about to be given by default. `ServerRequests::findGoodPathForNewSync()` cannot see
 * that: during onboarding no `SYNC_ADD` has run, so the folder exists nowhere on disk and no sync has been persisted.
 * Only the client knows what the current selection has already claimed.
 *
 * `taken` reports the paths already reserved by the caller. Existence on disk is enough to rule a derived candidate
 * out here, because the resolver only runs with no sync configured at all: no persisted sync can hold a folder the
 * user has since deleted. Should the settings ever reuse this flow with syncs already in place, each derived
 * candidate would have to go through `IS_PATH_VALID_FOR_NEW_SYNC` instead. Returns an empty string when no free path
 * was found.
 */
[[nodiscard]] QString makeUniqueLocalPath(const QString &path, const std::function<bool(const QString &)> &taken);

} // namespace KDC
