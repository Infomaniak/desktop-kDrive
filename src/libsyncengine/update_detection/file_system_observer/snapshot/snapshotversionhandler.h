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

#pragma once
#include "libcommon/utility/types.h"
namespace KDC {

class SnapshotVersionHandler {
    public:
        SnapshotVersionHandler& operator=(const SnapshotVersionHandler& other) {
            std::scoped_lock lock(_mutex);
            _version = other._version;
            return *this;
        }
        const SnapshotVersion& version() const {
            std::scoped_lock lock(_mutex);
            return _version;
        }

        SnapshotVersion nextVersion() {
            std::scoped_lock lock(_mutex);
            return ++_version;
        }

    private:
        mutable std::mutex _mutex;
        SnapshotVersion _version = 1; // Version 0 is reserved for the initial state
};

} // namespace KDC
