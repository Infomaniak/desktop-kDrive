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

#include "syncenginelib.h"

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace KDC {

struct DriveUserBasicInfo {
        std::string name;
        std::string email;
        std::string avatarUrl;
};

class DriveUserInfoCache {
    public:
        static DriveUserInfoCache &instance();

        DriveUserInfoCache(const DriveUserInfoCache &) = delete;
        void operator=(const DriveUserInfoCache &) = delete;

        [[nodiscard]] std::optional<DriveUserBasicInfo> get(int driveId, int userId) const;
        void put(int driveId, int userId, const DriveUserBasicInfo &info);
        void clear();

    private:
        DriveUserInfoCache() = default;

        static constexpr std::chrono::minutes _ttl{60};

        struct CacheKey {
                int driveId;
                int userId;
                bool operator==(const CacheKey &) const = default;
        };

        struct CacheKeyHash {
                size_t operator()(const CacheKey &k) const noexcept {
                    return std::hash<int>()(k.driveId) ^ (std::hash<int>()(k.userId) << 16);
                }
        };

        struct CacheEntry {
                DriveUserBasicInfo info;
                std::chrono::steady_clock::time_point insertTime;
        };

        mutable std::mutex _mutex;
        mutable std::unordered_map<CacheKey, CacheEntry, CacheKeyHash> _cache;
};

} // namespace KDC
