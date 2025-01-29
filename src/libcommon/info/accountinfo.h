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

namespace KDC {

class AccountInfo {
    public:
        AccountInfo(int dbId, int userDbId);
        AccountInfo();

        inline void setDbId(int dbId) { _dbId = dbId; }
        inline int dbId() const { return _dbId; }
        inline void setUserDbId(int userDbId) { _userDbId = userDbId; }
        inline int userDbId() const { return _userDbId; }

        friend QDataStream &operator>>(QDataStream &in, AccountInfo &userInfo);
        friend QDataStream &operator<<(QDataStream &out, const AccountInfo &userInfo);

        friend QDataStream &operator>>(QDataStream &in, QList<AccountInfo> &list);
        friend QDataStream &operator<<(QDataStream &out, const QList<AccountInfo> &list);

    private:
        int _dbId;
        int _userDbId;
};

} // namespace KDC
