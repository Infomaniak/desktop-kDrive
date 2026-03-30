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

#include "libparms/parmslib.h"
#include "libcommon/utility/types.h"

#include <string>
#include <memory>
#include <vector>

#include <log4cplus/logger.h>

namespace KDC {

class PARMS_EXPORT User {
    public:
        User();
        User(UserDbId dbId, UserId userId, const std::string &keychainKey, const std::string &name = {},
             const std::string &firstName = {}, const std::string &email = {}, const std::string &avatarUrl = {},
             std::shared_ptr<std::vector<char>> avatar = nullptr, bool toMigrate = false);

        inline void setDbId(const UserDbId dbId) { _dbId = dbId; }
        inline UserDbId dbId() const { return _dbId; }
        inline void setUserId(const UserId userId) { _userId = userId; }
        inline UserId userId() const { return _userId; }
        inline const std::string &keychainKey() const { return _keychainKey; }
        inline void setKeychainKey(const std::string &keychainKey) { _keychainKey = keychainKey; }
        inline const std::string &name() const { return _name; }
        inline void setName(const std::string &name) { _name = name; }

        // User logged in a version of kDrive Desktop < 4.0 might not have the firstName field populated until they have network
        // connectivity. In this case, we can use the name field as a fallback to avoid showing an empty name in the UI.
        inline const std::string &firstName() const { return _firstName.empty() ? _name : _firstName; }

        inline void setFirstName(const std::string &firstName) { _firstName = firstName; }
        inline const std::string &email() const { return _email; }
        inline void setEmail(const std::string &email) { _email = email; }
        inline const std::string &avatarUrl() const { return _avatarUrl; }
        inline void setAvatarUrl(const std::string &avatarUrl) { _avatarUrl = avatarUrl; }
        inline std::shared_ptr<CommBLOB> avatar() const { return _avatar; }
        inline void setAvatar(std::shared_ptr<CommBLOB> avatar) { _avatar = avatar; }
        inline void setToMigrate(bool toMigrate) { _toMigrate = toMigrate; }
        inline int toMigrate() const { return _toMigrate; }
        [[nodiscard]] bool isStaff() const { return _isStaff; }
        void setIsStaff(const bool isStaff) { _isStaff = isStaff; }

    private:
        log4cplus::Logger _logger;
        UserDbId _dbId{0};
        UserId _userId{0};
        std::string _keychainKey;
        std::string _name;
        std::string _firstName;
        std::string _email;
        std::string _avatarUrl;
        std::shared_ptr<std::vector<char>> _avatar;
        bool _toMigrate{false};

        // Non DB attributes
        bool _isStaff{false};
};

} // namespace KDC
