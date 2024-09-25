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

#include <unordered_map>
#include <mutex>
#include <thread>
#include <cassert>
#include <chrono>
#include "utility/types.h"
#include "log/sentry/sentryhandler.h"

constexpr int maxExclusiveAcessTime = 1000; // in ms

namespace KDC {

/// Thread-safe unordered map for any type of data
template<typename Key, typename Value>
class UnorderedMapTS {
    private: /// Locker
        class Locker {
            public:
                explicit Locker(UnorderedMapTS &map) : _map(map) { _map.lock(); }
                ~Locker() { _map.unlock(); }

            private:
                UnorderedMapTS &_map;
        };
        friend UnorderedMapTS::Locker;

    public:
        UnorderedMapTS() = default;
        UnorderedMapTS(const UnorderedMapTS &) = delete;
        UnorderedMapTS &operator=(const UnorderedMapTS &) = delete;

        std::unordered_map<Key, Value> &getRawReference(std::thread::id threadId) {
            if (hasExclusiveAccess(threadId)) {
                giveExclusiveAccess(threadId);
                return _map;
            }
            lock();
            giveExclusiveAccess(threadId);
            return _map;
        }

        void releaseOneRawReference(std::thread::id threadId) {
            if (!hasExclusiveAccess(threadId)) {
                assert(false && "The current thread does not have exclusive access");
                // LOG_ERROR("The current thread does not have exclusive access");
                SentryHandler::instance()->captureMessage(SentryLevel::Error, "Multi-thread management error",
                                                          "The current thread does not have exclusive access");
                return;
            }
            handleExclusiveAccessRelease(threadId);
            _ExclusiveAccessThreadId = std::thread::id();
            unlock();
        }

        void insert(const std::pair<Key, Value> &pair) {
            const Locker lock(*this);
            _map.insert(pair);
        }

        void erase(const Key &key) {
            const Locker lock(*this);
            _map.erase(key);
        }

        [[nodiscard]] bool empty() {
            const Locker lock(*this);
            return _map.empty();
        }

        [[nodiscard]] size_t size() {
            const Locker lock(*this);
            return _map.size();
        }

        [[nodiscard]] bool contains(const Key &key) {
            const Locker lock(*this);
            return _map.contains(key);
        }

        [[nodiscard]] Value extract(const Key &key, bool &found) {
            const Locker lock(*this);
            auto extracted = _map.extract(key);
            found = !extracted.empty();
            return extracted.mapped();
        }

    private:
        std::unordered_map<Key, Value> _map;
        mutable std::mutex _mutex;

        /// Exclusive access management
        mutable std::mutex _ExclusiveAccessMutexHandler;
        std::thread::id _ExclusiveAccessThreadId;
        unsigned int _ExclusiveAccessCount = 0;
        std::chrono::system_clock::time_point _ExclusiveAccessTime;
        bool alertLongExclusiveAccess = false;

        bool hasExclusiveAccess(std::thread::id threadId) {
            std::scoped_lock lock(_ExclusiveAccessMutexHandler);
            return _ExclusiveAccessThreadId == threadId;
        }

        bool giveExclusiveAccess(std::thread::id threadId) {
            std::scoped_lock lock(_ExclusiveAccessMutexHandler);
            if (_ExclusiveAccessThreadId == threadId) {
                _ExclusiveAccessCount++;
                return true;
            }

            _ExclusiveAccessThreadId = threadId;
            _ExclusiveAccessCount = 1;
            _ExclusiveAccessTime = std::chrono::system_clock::now();
            return true;
        }

        bool handleExclusiveAccessRelease(std::thread::id threadId) {
            std::scoped_lock lock(_ExclusiveAccessMutexHandler);
            if (_ExclusiveAccessThreadId != threadId) {
                assert(false && "The current thread does not have exclusive access");
                // LOG_ERROR("The current thread does not have exclusive access");
                SentryHandler::instance()->captureMessage(SentryLevel::Error, "Multi-thread management error",
                                                          "The current thread does not have exclusive access");
                return false;
            }

            _ExclusiveAccessCount--;
            return _ExclusiveAccessCount == 0;
        }

        void lock() {
            using std::chrono::system_clock;
            while (true) {
                if (_mutex.try_lock()) {
                    return;
                }
                longExclusiveAccessDetector();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        void unlock() { _mutex.unlock(); }

        void longExclusiveAccessDetector() {
            if (!alertLongExclusiveAccess && _ExclusiveAccessThreadId != std::thread::id() &&
                std::chrono::system_clock::now() - _ExclusiveAccessTime > std::chrono::milliseconds(maxExclusiveAcessTime)) {
                // LOG_ERROR("A thread has thread has exclusive access for too long");
                assert(false && "A thread has exclusive access for too long");
                SentryHandler::instance()->captureMessage(SentryLevel::Error, "Multi-thread management error",
                                                          "A thread has exclusive access for too long");
                alertLongExclusiveAccess = true;
            }
        }
};

} // namespace KDC
