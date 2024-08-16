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

#pragma once

#include "libcommon/utility/types.h"

#include <QString>
#include <QDataStream>

namespace KDC {

class ProxyConfigInfo {
    public:
        ProxyConfigInfo() = default;
        ProxyConfigInfo(ProxyType type, const QString &hostName, int port, bool needsAuth, const QString &user = "",
                        const QString &pwd = "");

        inline ProxyType type() const { return _type; }
        inline void setType(ProxyType type) { _type = type; }
        inline const QString &hostName() const { return _hostName; }
        inline void setHostName(const QString &hostName) { _hostName = hostName; }
        inline int port() const { return _port; }
        inline void setPort(int port) { _port = port; }
        inline bool needsAuth() const { return _needsAuth; }
        inline void setNeedsAuth(bool needsAuth) { _needsAuth = needsAuth; }
        inline const QString &user() const { return _user; }
        inline void setUser(const QString &user) { _user = user; }
        inline const QString &pwd() const { return _pwd; }
        inline void setPwd(const QString &pwd) { _pwd = pwd; }

        friend QDataStream &operator>>(QDataStream &in, ProxyConfigInfo &proxyConfigInfo);
        friend QDataStream &operator<<(QDataStream &out, const ProxyConfigInfo &proxyConfigInfo);

    private:
        ProxyType _type = ProxyType::None;
        QString _hostName;
        int _port = 0;
        bool _needsAuth = false;
        QString _user;
        QString _pwd;
};

}  // namespace KDC
