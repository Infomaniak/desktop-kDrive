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

#include "user.h"

#include "libcommonserver/log/log.h"

#include <Poco/URIStreamOpener.h>

#include <log4cplus/loggingmacros.h>

namespace KDC {

User::User() :
    _logger(Log::instance()->getLogger()) {}

User::User(const UserDbId dbId, const UserId userId, const std::string &keychainKey, const std::string &name,
           const std::string &firstName, const std::string &email, const std::string &avatarUrl,
           const std::shared_ptr<std::vector<char>> avatar, const bool toMigrate) :
    _logger(Log::instance()->getLogger()),
    _dbId(dbId),
    _userId(userId),
    _keychainKey(keychainKey),
    _name(name),
    _firstName(firstName),
    _email(email),
    _avatarUrl(avatarUrl),
    _avatar(avatar),
    _toMigrate(toMigrate) {}

} // namespace KDC
