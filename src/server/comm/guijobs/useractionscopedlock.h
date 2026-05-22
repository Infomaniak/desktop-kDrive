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
#include <memory>
#include <optional>
#include <mutex>
#include <chrono>

namespace KDC {

class UserActionScopedLock {
    public:
        UserActionScopedLock() = default;
        explicit UserActionScopedLock(std::shared_ptr<SyncPal> syncPal) { lock(syncPal); }

        UserActionScopedLock(std::shared_ptr<SyncPal> syncPal, std::chrono::milliseconds timeout) { tryLock(syncPal, timeout); }

        void lock(std::shared_ptr<SyncPal> syncPal) {
            if (_lock.has_value() && _lock->owns_lock()) return;
            _lock = std::unique_lock(syncPal->userActionsMutex());
        }

        bool tryLock(std::shared_ptr<SyncPal> syncPal, std::chrono::milliseconds timeout = std::chrono::milliseconds(0)) {
            if (_lock.has_value() && _lock->owns_lock()) return true;

            std::unique_lock<std::timed_mutex> lock(syncPal->userActionsMutex(), std::defer_lock);
            if (lock.try_lock_for(timeout)) {
                _lock = std::move(lock);
                return true;
            }
            return false;
        }

        bool ownsLock() const { return _lock.has_value() && _lock->owns_lock(); }

    private:
        std::optional<std::unique_lock<std::timed_mutex>> _lock;
};
} // namespace KDC
