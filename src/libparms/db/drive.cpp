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

#include "drive.h"
#include "account.h"

namespace KDC {

Drive::Drive()
    : _logger(Log::instance()->getLogger()),
      _dbId(0),
      _driveId(0),
      _accountDbId(0),
      _name(std::string()),
      _size(0),
      _color(std::string()),
      _notifications(true),
      _admin(false),
      _maintenance(false),
      _maintenanceFrom(0),
      _locked(false),
      _usedSize(0),
      _accessDenied(false) {}

Drive::Drive(int dbId, int driveId, int accountDbId, const std::string &name, int64_t size, const std::string &color,
             bool notifications, bool admin)
    : _logger(Log::instance()->getLogger()),
      _dbId(dbId),
      _driveId(driveId),
      _accountDbId(accountDbId),
      _name(name),
      _size(size),
      _color(color),
      _notifications(notifications),
      _admin(admin),
      _maintenance(false),
      _maintenanceFrom(0),
      _locked(false),
      _usedSize(0),
      _accessDenied(false) {}

}  // namespace KDC
