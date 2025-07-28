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

PermissionsHolder::PermissionsHolder(const SyncPath &path, log4cplus::Logger *_logger /*= nullptr*/) :
    _path(path) {
    AccessRights accessRights;
    if (const auto ioError = IoHelper::getRights(path, accessRights.read, accessRights.write, accessRights.exec);
        ioError != IoError::Success) {
        log(std::wstringstream() << L"Fail to get rights for: " << Utility::formatSyncPath(path), LogLevel::Error);
        return;
    }
    if (const auto ioError = IoHelper::isLocked(path, accessRights.locked); ioError != IoError::Success) {
        log(std::wstringstream() << L"Fail to check if file is locked for: " << Utility::formatSyncPath(path), LogLevel::Error);
        return;
    }

    const std::scoped_lock scopedLock(mutex);
    if (!heldPermissions.contains(_path) && accessRights.read && accessRights.write && accessRights.exec &&
        !accessRights.locked) {
        // User has full access => nothing to do.
        return;
    }

    if (IoHelper::setFullAccess(_path) != IoError::Success) {
        log(std::wstringstream() << L"Failed to set full access rights: " << Utility::formatSyncPath(_path), LogLevel::Error);
    }
    heldPermissions.try_emplace(_path, 0, accessRights);
    heldPermissions[_path].first++;
    log(std::wstringstream() << L"PermissionsHolder set full access rights: " << Utility::formatSyncPath(_path) << L" / count:"
                             << heldPermissions[_path].first,
        LogLevel::Debug);
}

PermissionsHolder::~PermissionsHolder() {
    const std::scoped_lock scopedLock(mutex);
    if (!heldPermissions.contains(_path)) return;

    auto &[count, accessRights] = heldPermissions[_path];
    count--;
    log(std::wstringstream() << L"PermissionsHolder value: " << Utility::formatSyncPath(_path) << L" / count:" << count,
        LogLevel::Debug);
    if (count > 0) return; // Do not put back read-only rights yet.

    log(std::wstringstream() << L"PermissionsHolder set back initial rights: " << Utility::formatSyncPath(_path) << L" / count:"
                             << count,
        LogLevel::Debug);
    if (accessRights.locked) {
        if (const auto ioError = IoHelper::lock(_path); ioError != IoError::Success) {
            log(std::wstringstream() << L"Failed to lock item for: " << Utility::formatSyncPath(_path) << L" / count:" << count,
                LogLevel::Error);
        }
    }
    if (const auto ioError = IoHelper::setRights(_path, accessRights.read, accessRights.write, accessRights.exec);
        ioError != IoError::Success) {
        log(std::wstringstream() << L"Failed to set rights for: " << Utility::formatSyncPath(_path), LogLevel::Error);
    }
    (void) heldPermissions.erase(_path);
}

void PermissionsHolder::log(const std::wstringstream &ss, LogLevel logLevel /*= LogLevel::Debug*/) {
    if (!_logger) return;
    switch (logLevel) {
        case LogLevel::Debug:
            LOGW_DEBUG(*_logger, ss.str());
            break;
        case LogLevel::Info:
            LOGW_INFO(*_logger, ss.str());
            break;
        case LogLevel::Warning:
            LOGW_WARN(*_logger, ss.str());
            break;
        case LogLevel::Error:
            LOGW_ERROR(*_logger, ss.str());
            break;
        case LogLevel::Fatal:
            LOGW_FATAL(*_logger, ss.str());
            break;
        default:
            break;
    }
}

} // namespace KDC
