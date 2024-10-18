/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

/**
 * Thread safe implementation of a very simple non blocking lock.
 */

#include <mutex>

namespace KDC {

class LockUtility {
    public:
        /**
         * Acquire the lock.
         * @return Return false if already locked.
         */
        [[nodiscard]] bool lock() {
            const std::scoped_lock lock(_mutex);
            if (_isLocked) return false;
            _isLocked = true;
            return _isLocked;
        }
        /**
         * Release the lock.
         */
        void unlock() { _isLocked = false; }

    private:
        std::mutex _mutex;
        bool _isLocked{false};
};

} // namespace KDC
