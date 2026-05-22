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
        explicit UserActionScopedLock(const std::shared_ptr<SyncPal> syncPal) { lock(syncPal); }

        UserActionScopedLock(const std::shared_ptr<SyncPal> syncPal, const std::chrono::milliseconds timeout) {
            (void) tryLock(syncPal, timeout);
        }

        void lock(const std::shared_ptr<SyncPal> syncPal) {
            if (_lock.has_value() && _lock->owns_lock()) return;
            _lock = std::unique_lock(syncPal->userActionsMutex());
        }

        bool tryLock(const std::shared_ptr<SyncPal> syncPal,
                     const std::chrono::milliseconds timeout = std::chrono::milliseconds(0)) {
            if (_lock.has_value() && _lock->owns_lock()) return true;

            if (std::unique_lock lock(syncPal->userActionsMutex(), std::defer_lock); lock.try_lock_for(timeout)) {
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
