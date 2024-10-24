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
        explicit LockUtility(std::mutex &mutex) : _mutex(mutex) { _ownLock = mutex.try_lock(); }
        ~LockUtility() {
            if (_ownLock) _mutex.unlock();
        }
        [[nodiscard]] bool ownLock() const { return _ownLock; }

    private:
        std::mutex &_mutex;
        bool _ownLock{false};
};

} // namespace KDC
