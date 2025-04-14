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
#include <limits>

namespace KDC {

class SnapshotRevisionHandler {
    public:
        SnapshotRevisionHandler& operator=(const SnapshotRevisionHandler& other) {
            const std::scoped_lock lock(_mutex);
            _revision = other._revision;
            return *this;
        }
        const SnapshotRevision& revision() const {
            const std::scoped_lock lock(_mutex);
            return _revision;
        }

        SnapshotRevision nextVersion() {
            const std::scoped_lock lock(_mutex);
#pragma push_macro("max")
#undef max
            if (++_revision == std::numeric_limits<SnapshotRevision>::max()) {
                /* Throw an exception if the revision number is too high.This is
                 * acceptable because the revision number is a 64  bit integer.
                 * Even at the insane rate of 500,000 snapshot changes per second,
                 * it would take 1,169,884 years to reach this limit. */
                throw std::overflow_error("Snapshot revision number overflow");
            }
#pragma pop_macro("max")
            return _revision;
        }

    private:
        mutable std::mutex _mutex;
        SnapshotRevision _revision = 1; // Version 0 is reserved for the initial state
};

} // namespace KDC
