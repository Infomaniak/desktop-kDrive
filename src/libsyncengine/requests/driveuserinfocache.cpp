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

#include "driveuserinfocache.h"

namespace KDC {

DriveUserInfoCache &DriveUserInfoCache::instance() {
    static DriveUserInfoCache cache;
    return cache;
}

std::optional<DriveUserBasicInfo> DriveUserInfoCache::get(const int32_t driveId, const int32_t userId) const {
    const std::scoped_lock lock(_mutex);

    const CacheKey key{driveId, userId};
    const auto it = _cache.find(key);
    if (it == _cache.end()) {
        return std::nullopt;
    }

    if (std::chrono::steady_clock::now() - it->second.insertTime > _ttl) {
        (void) _cache.erase(it);
        return std::nullopt;
    }

    return it->second.info;
}

void DriveUserInfoCache::put(const int32_t driveId, const int32_t userId, const DriveUserBasicInfo &info) {
    const std::scoped_lock lock(_mutex);

    const CacheKey key{driveId, userId};
    (void) _cache.insert_or_assign(key, CacheEntry{info, std::chrono::steady_clock::now()});
}

void DriveUserInfoCache::clear() {
    const std::scoped_lock lock(_mutex);
    _cache.clear();
}

} // namespace KDC
