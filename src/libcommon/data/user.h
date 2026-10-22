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

#pragma once

#include "libcommon/utility/types.h"
#include "libcommon/utility/utility.h"

#include <QBuffer>
#include <QDataStream>
#include <QImage>
#include <QList>

#include <Poco/Dynamic/Struct.h>

#include <memory>
#include <string>
#include <vector>

namespace KDC {

class User {
    public:
        User() = default;
        User(UserDbId dbId, UserId userId, const std::string &keychainKey, const std::string &name = {},
             const std::string &firstName = {}, const std::string &email = {}, const std::string &avatarUrl = {},
             std::shared_ptr<std::vector<char>> avatar = nullptr, bool toMigrate = false);

        inline void setDbId(const UserDbId dbId) { _dbId = dbId; }
        [[nodiscard]] inline UserDbId dbId() const { return _dbId; }
        inline void setUserId(const UserId userId) { _userId = userId; }
        [[nodiscard]] inline UserId userId() const { return _userId; }
        [[nodiscard]] inline const std::string &keychainKey() const { return _keychainKey; }
        inline void setKeychainKey(const std::string &keychainKey) { _keychainKey = keychainKey; }
        [[nodiscard]] inline const std::string &name() const { return _name; }
        inline void setName(const std::string &name) { _name = name; }

        // User logged in a version of kDrive Desktop < 4.0 might not have the firstName field populated until they have network
        // connectivity. In this case, we can use the name field as a fallback to avoid showing an empty name in the UI.
        [[nodiscard]] inline const std::string &firstName() const { return _firstName.empty() ? _name : _firstName; }

        inline void setFirstName(const std::string &firstName) { _firstName = firstName; }
        [[nodiscard]] inline const std::string &email() const { return _email; }
        inline void setEmail(const std::string &email) { _email = email; }
        [[nodiscard]] inline const std::string &avatarUrl() const { return _avatarUrl; }
        inline void setAvatarUrl(const std::string &avatarUrl) { _avatarUrl = avatarUrl; }
        [[nodiscard]] inline std::shared_ptr<CommBLOB> avatar() const { return _avatar; }
        inline void setAvatar(std::shared_ptr<CommBLOB> avatar) { _avatar = avatar; }
        inline void setToMigrate(bool toMigrate) { _toMigrate = toMigrate; }
        [[nodiscard]] inline bool toMigrate() const { return _toMigrate; }
        [[nodiscard]] inline bool isStaff() const { return _isStaff; }
        inline void setIsStaff(const bool isStaff) { _isStaff = isStaff; }

        // Transient (non-DB) attributes used by the GUI
        [[nodiscard]] inline bool connected() const { return _connected; }
        inline void setConnected(const bool connected) { _connected = connected; }
        [[nodiscard]] inline bool credentialsAsked() const { return _credentialsAsked; }
        inline void setCredentialsAsked(const bool newCredentialsAsked) { _credentialsAsked = newCredentialsAsked; }

        void toDynamicStruct(Poco::DynamicStruct &dstruct) const;
        void fromDynamicStruct(const Poco::DynamicStruct &dstruct);

        /// TODO : to be removed once we moved to the new GUI ///
        friend void operator>>(QDataStream &in, User &user) {
            qint64 userDbId = 0;
            qint64 userId = 0;
            QString name;
            QString firstName;
            QString email;
            QImage avatar;
            bool connected = false;
            bool isStaff = false;
            QString avatarUrl;

            in >> userDbId >> userId >> name >> firstName >> email >> avatar >> connected >> isStaff >> avatarUrl;

            user.setDbId(static_cast<UserDbId>(userDbId));
            user.setUserId(static_cast<UserId>(userId));
            user.setName(name.toStdString());
            user.setFirstName(firstName.toStdString());
            user.setEmail(email.toStdString());
            user.setConnected(connected);
            user.setIsStaff(isStaff);
            user.setAvatarUrl(avatarUrl.toStdString());
            user.setAvatar(CommonUtility::toCommBlob(avatar));
        }
        friend QDataStream &operator<<(QDataStream &out, const User &user) {
            out << static_cast<qint64>(user.dbId()) << static_cast<qint64>(user.userId()) << QString::fromStdString(user.name())
                << QString::fromStdString(user.firstName()) << QString::fromStdString(user.email())
                << CommonUtility::toQImage(user.avatar()) << user.connected() << user.isStaff()
                << QString::fromStdString(user.avatarUrl());
            return out;
        }

        friend void operator>>(QDataStream &in, QList<User> &list) {
            qint64 count = 0;
            in >> count;
            for (qint64 i = 0; i < count; i++) {
                User user;
                in >> user;
                list.push_back(user);
            }
        }
        friend QDataStream &operator<<(QDataStream &out, const QList<User> &list) {
            const auto count = static_cast<qint64>(list.size());
            out << count;
            for (qint64 i = 0; i < count; i++) {
                out << list[static_cast<qsizetype>(i)];
            }
            return out;
        }
        /////////////////////////////////////////////////////////

        bool operator==(const User &other) const = default;

    private:
        UserDbId _dbId{0};
        UserId _userId{0};
        std::string _keychainKey;
        std::string _name;
        std::string _firstName;
        std::string _email;
        std::string _avatarUrl;
        std::shared_ptr<CommBLOB> _avatar;
        bool _toMigrate{false};

        // Non DB attributes
        bool _isStaff{false};
        bool _connected{false};
        bool _credentialsAsked{false};
};

using UserList = std::vector<User>;

} // namespace KDC
