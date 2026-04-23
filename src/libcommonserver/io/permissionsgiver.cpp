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

#include "permissionsgiver.h"

namespace KDC {

PermissionsGiver::PermissionsGiver(const SyncPath &path, const log4cplus::Logger logger) :
    _path(path),
    _logger(logger) {
    if (const auto ioError = IoHelper::setFullAccess(_path); ioError != IoError::Success) {
        LOGW_ERROR(_logger, L"Failed to set full access rights - " << Utility::formatIoError(path, ioError));
    } else {
        LOGW_DEBUG(_logger, L"PermissionsGiver set full access rights: " << Utility::formatSyncPath(_path));
    }
}

} // namespace KDC
