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

#include <QImage>
#include <QDataStream>
#include <QString>
#include <QList>

#include <Poco/Dynamic/Struct.h>

namespace KDC {

class UserInfo {
    public:
        UserInfo(int userDbId, int userId, const QString &name, const QString &email, const QImage &avatar, bool connected);
        UserInfo();

        inline void setDbId(int dbId) { _dbId = dbId; }
        inline int dbId() const { return _dbId; }
        inline void setUserId(int userId) { _userId = userId; }
        inline int userId() const { return _userId; }
        inline void setName(const QString &name) { _name = name; }
        inline const QString &name() const { return _name; }
        inline void setEmail(const QString &email) { _email = email; }
        inline const QString &email() const { return _email; }
        inline void setAvatar(const QImage &avatar) { _avatar = avatar; }
        inline const QImage &avatar() const { return _avatar; }
        inline void setConnected(bool connected) { _connected = connected; }
        inline bool connected() const { return _connected; }
        inline bool credentialsAsked() const { return _credentialsAsked; }
        inline void setCredentialsAsked(bool newCredentialsAsked) { _credentialsAsked = newCredentialsAsked; }
        [[nodiscard]] bool isStaff() const { return _isStaff; }
        void setIsStaff(const bool is_staff) { _isStaff = is_staff; }

        void toDynamicStruct(Poco::DynamicStruct &dstruct) const;
        void fromDynamicStruct(const Poco::DynamicStruct &dstruct);

        friend QDataStream &operator>>(QDataStream &in, UserInfo &userInfo);
        friend QDataStream &operator<<(QDataStream &out, const UserInfo &userInfo);

        friend QDataStream &operator>>(QDataStream &in, QList<UserInfo> &list);
        friend QDataStream &operator<<(QDataStream &out, const QList<UserInfo> &list);

    private:
        int _dbId{-1};
        int _userId{-1};
        QString _name;
        QString _email;
        QImage _avatar;
        bool _connected{false};

        // Non DB attributes
        bool _credentialsAsked{false};
        bool _isStaff{false};
};

} // namespace KDC
