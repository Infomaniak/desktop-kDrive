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

#include "libcommon/info/proxyconfiginfo.h"

namespace KDC {

ProxyConfigInfo::ProxyConfigInfo(ProxyType type, const QString &hostName, int port, bool needsAuth, const QString &user,
                                 const QString &pwd) :
    _type(type), _hostName(hostName), _port(port), _needsAuth(needsAuth), _user(user), _pwd(pwd) {}

QDataStream &operator>>(QDataStream &in, ProxyConfigInfo &proxyConfigInfo) {
    in >> proxyConfigInfo._type >> proxyConfigInfo._hostName >> proxyConfigInfo._port >> proxyConfigInfo._needsAuth >>
            proxyConfigInfo._user >> proxyConfigInfo._pwd;
    return in;
}

QDataStream &operator<<(QDataStream &out, const ProxyConfigInfo &proxyConfigInfo) {
    out << proxyConfigInfo._type << proxyConfigInfo._hostName << proxyConfigInfo._port << proxyConfigInfo._needsAuth
        << proxyConfigInfo._user << proxyConfigInfo._pwd;
    return out;
}

} // namespace KDC
