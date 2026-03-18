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

#include <QImage>
#include <QDataStream>
#include <QString>
#include <QList>

#include <Poco/Dynamic/Struct.h>

namespace KDC {

class UserInfo {
    public:
        UserInfo(UserDbId userDbId, UserId userId, const QString &name, const QString &email, const QImage &avatar,
                 bool connected);
        UserInfo() = default;

        inline void setDbId(UserDbId dbId) { _dbId = dbId; }
        inline UserDbId dbId() const { return _dbId; }
        inline void setUserId(UserId userId) { _userId = userId; }
        inline UserId userId() const { return _userId; }
        inline void setName(const QString &name) { _name = name; }
        inline const QString &name() const { return _name; }
        inline void setEmail(const QString &email) { _email = email; }
        inline const QString &email() const { return _email; }

        // For temporary users, the avatar does not need to be available offline,
        // so `avatar` is not mandatory and `avatarUrl` can be used instead to display the avatar in the UI.
        inline void setAvatar(const QImage &avatar) { _avatar = avatar; }
        inline const QImage &avatar() const { return _avatar; }
        inline void setAvatarUrl(const QString &avatarUrl) { _avatarUrl = avatarUrl; }
        inline const QString &avatarUrl() const { return _avatarUrl; }
        inline void setConnected(bool connected) { _connected = connected; }
        inline bool connected() const { return _connected; }
        inline bool credentialsAsked() const { return _credentialsAsked; }
        inline void setCredentialsAsked(bool newCredentialsAsked) { _credentialsAsked = newCredentialsAsked; }
        [[nodiscard]] bool isStaff() const { return _isStaff; }
        void setIsStaff(const bool is_staff) { _isStaff = is_staff; }

        friend bool operator==(const UserInfo &lhs, const UserInfo &rhs) {
            return (lhs.dbId() == rhs.dbId()) && (lhs.userId() == rhs.userId()) && (lhs.name() == rhs.name()) &&
                   (lhs.email() == rhs.email()) && (lhs.avatar() == rhs.avatar()) && (lhs.avatarUrl() == rhs.avatarUrl()) &&
                   (lhs.connected() == rhs.connected()) && (lhs.credentialsAsked() == rhs.credentialsAsked()) &&
                   (lhs.isStaff() == rhs.isStaff());
        }

        void toDynamicStruct(Poco::DynamicStruct &dstruct) const;
        void fromDynamicStruct(const Poco::DynamicStruct &dstruct);

        friend QDataStream &operator>>(QDataStream &in, UserInfo &userInfo);
        friend QDataStream &operator<<(QDataStream &out, const UserInfo &userInfo);

        friend QDataStream &operator>>(QDataStream &in, QList<UserInfo> &list);
        friend QDataStream &operator<<(QDataStream &out, const QList<UserInfo> &list);

    private:
        UserDbId _dbId{-1};
        UserId _userId{-1};
        QString _name;
        QString _email;
        QImage _avatar;
        QString _avatarUrl;
        bool _connected{false};

        // Non DB attributes
        bool _credentialsAsked{false};
        bool _isStaff{false};
};

} // namespace KDC
