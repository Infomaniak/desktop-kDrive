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

#include "user.h"

#include <Poco/URIStreamOpener.h>

namespace KDC {

User::User()
    : _logger(Log::instance()->getLogger()),
      _dbId(0),
      _userId(0),
      _keychainKey(std::string()),
      _name(std::string()),
      _email(std::string()),
      _avatarUrl(std::string()),
      _avatar(nullptr),
      _toMigrate(false) {}

User::User(int dbId, int userId, const std::string &keychainKey, const std::string &name, const std::string &email,
           const std::string &avatarUrl, std::shared_ptr<std::vector<char>> avatar, bool toMigrate)
    : _logger(Log::instance()->getLogger()),
      _dbId(dbId),
      _userId(userId),
      _keychainKey(keychainKey),
      _name(name),
      _email(email),
      _avatarUrl(avatarUrl),
      _avatar(avatar),
      _toMigrate(toMigrate) {}

}  // namespace KDC
