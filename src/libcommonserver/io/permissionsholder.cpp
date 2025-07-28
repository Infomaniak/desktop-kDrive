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

#include "permissionsholder.h"

namespace KDC {

struct AccessRights {
        bool read{false};
        bool write{false};
        bool exec{false};
        bool locked{false};
};
static std::unordered_map<SyncPath, std::pair<uint64_t, AccessRights>, PathHashFunction> heldPermissions;
static std::mutex mutex;

PermissionsHolder::PermissionsHolder(const SyncPath &path) :
    _path(path) {
    AccessRights accessRights;
    if (const auto ioError = IoHelper::getRights(path, accessRights.read, accessRights.write, accessRights.exec);
        ioError != IoError::Success) {
        LOGW_DEBUG(Log::instance()->getLogger(), L"Fail to get rights for: " << Utility::formatSyncPath(path));
        return;
    }
    if (const auto ioError = IoHelper::isLocked(path, accessRights.locked); ioError != IoError::Success) {
        LOGW_DEBUG(Log::instance()->getLogger(), L"Fail to check if file is locked for: " << Utility::formatSyncPath(path));
        return;
    }
    if (accessRights.read && accessRights.write && accessRights.exec && !accessRights.locked) {
        // User has full access => nothing to do.
        return;
    }

    const std::scoped_lock scopedLock(mutex);
    if (IoHelper::setFullAccess(_path) != IoError::Success) {
        LOGW_WARN(Log::instance()->getLogger(), L"Failed to set full access rights: " << Utility::formatSyncPath(_path));
    }
    heldPermissions.try_emplace(_path, 0, accessRights);
    heldPermissions[_path].first++;
    LOGW_DEBUG(Log::instance()->getLogger(),
               L"PermissionsHolder unsetReadOnly: " << Utility::formatSyncPath(_path) << L" / " << heldPermissions[_path].first);
}

PermissionsHolder::~PermissionsHolder() {
    const std::scoped_lock scopedLock(mutex);
    if (!heldPermissions.contains(_path)) return;

    auto &[count, accessRights] = heldPermissions[_path];
    count--;
    LOGW_DEBUG(Log::instance()->getLogger(), L"PermissionsHolder value: " << Utility::formatSyncPath(_path) << L" / " << count);
    if (count > 0) return; // Do not put back read-only rights yet.

    LOGW_DEBUG(Log::instance()->getLogger(),
               L"PermissionsHolder setReadOnly: " << Utility::formatSyncPath(_path) << L" / " << count);
    if (accessRights.locked) {
        if (const auto ioError = IoHelper::lock(_path); ioError != IoError::Success) {
            LOGW_ERROR(Log::instance()->getLogger(),
                       L"PermissionsHolder setReadOnly: " << Utility::formatSyncPath(_path) << L" / " << count);
        }
    }
    if (const auto ioError = IoHelper::setRights(_path, accessRights.read, accessRights.write, accessRights.exec);
        ioError != IoError::Success) {
        LOGW_ERROR(Log::instance()->getLogger(), L"Fail to set rights for: " << Utility::formatSyncPath(_path));
    }
    (void) heldPermissions.erase(_path);
}

} // namespace KDC
