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

struct AccessRightsInfo {
        bool read{false};
        bool write{false};
        bool exec{false};
        bool locked{false};
        uint64_t count{0};
};
static std::unordered_map<SyncPath, AccessRightsInfo, PathHashFunction> heldPermissions;
static std::mutex mutex;

PermissionsHolder::PermissionsHolder(const SyncPath &path, const log4cplus::Logger logger) :
    _path(path),
    _logger(logger) {
    AccessRightsInfo accessRights;
    if (const auto ioError = IoHelper::getRights(path, accessRights.read, accessRights.write, accessRights.exec);
        ioError != IoError::Success) {
        LOGW_ERROR(_logger, L"Failed to get rights for " << Utility::formatSyncPath(path));
        return;
    }
    if (const auto ioError = IoHelper::isLocked(path, accessRights.locked); ioError != IoError::Success) {
        LOGW_ERROR(_logger, L"Failed to check if file is locked for " << Utility::formatSyncPath(path));
        return;
    }

    const std::scoped_lock scopedLock(mutex);
    if (!heldPermissions.contains(_path) && accessRights.read && accessRights.write && accessRights.exec &&
        !accessRights.locked) {
        // User has full access => nothing to do.
        return;
    }

    if (IoHelper::setFullAccess(_path) != IoError::Success) {
        LOGW_ERROR(_logger, L"Failed to set full access rights: " << Utility::formatSyncPath(_path));
    }
    (void) heldPermissions.try_emplace(_path, accessRights);
    heldPermissions[_path].count++;
    LOGW_DEBUG(_logger, L"PermissionsHolder set full access rights: " << Utility::formatSyncPath(_path) << L" / count:"
                                                                      << heldPermissions[_path].count);
}

PermissionsHolder::~PermissionsHolder() {
    const std::scoped_lock scopedLock(mutex);
    if (!heldPermissions.contains(_path)) return;

    AccessRightsInfo accessRights;
    try {
        heldPermissions[_path].count--;
        accessRights = heldPermissions[_path];
    } catch (...) {
        LOGW_ERROR(_logger, L"PermissionsHolder failed to get value for: " << Utility::formatSyncPath(_path));
        return;
    }

    LOGW_DEBUG(_logger, L"PermissionsHolder value: " << Utility::formatSyncPath(_path) << L" / count:" << accessRights.count);
    if (accessRights.count > 0) return; // Do not put back read-only rights yet.

    LOGW_DEBUG(_logger, L"PermissionsHolder setting back initial rights: " << Utility::formatSyncPath(_path));
    if (const auto ioError = IoHelper::setRights(_path, accessRights.read, accessRights.write, accessRights.exec);
        ioError != IoError::Success) {
        LOGW_ERROR(_logger,
                   L"Failed to set rights for " << Utility::formatSyncPath(_path) << L" - " << Utility::formatIoError(ioError));
    }
    if (accessRights.locked) {
        if (const auto ioError = IoHelper::lock(_path); ioError != IoError::Success) {
            LOGW_ERROR(_logger, L"Failed to lock item for " << Utility::formatSyncPath(_path) << L" - "
                                                            << Utility::formatIoError(ioError));
        }
    }
    (void) heldPermissions.erase(_path);
}

} // namespace KDC
