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

#include "utility/types.h"

#include <Poco/Dynamic/Struct.h>

#include <string>

namespace KDC {

class ProxyConfig {
    public:
        ProxyConfig() = default;
        ProxyConfig(ProxyType type, const std::string &hostName, int port, bool needsAuth, const std::string &user = "",
                    const std::string &pwd = "");

        inline ProxyType type() const { return _type; }
        inline void setType(ProxyType type) { _type = type; }
        inline const std::string &hostName() const { return _hostName; }
        inline void setHostName(const std::string &hostName) { _hostName = hostName; }
        inline int port() const { return _port; }
        inline void setPort(int port) { _port = port; }
        inline bool needsAuth() const { return _needsAuth; }
        inline void setNeedsAuth(bool needsAuth) { _needsAuth = needsAuth; }
        inline const std::string &user() const { return _user; }
        inline void setUser(const std::string &user) { _user = user; }

        inline const std::string &pwd() const { return _pwd; }
        inline void setPwd(const std::string &pwd) { _pwd = pwd; }

        inline const std::string &keychainKey() const { return _keychainKey; }
        inline void setKeychainKey(const std::string &keychainKey) { _keychainKey = keychainKey; }

        void toDynamicStruct(Poco::DynamicStruct &) const;
        void fromDynamicStruct(const Poco::DynamicStruct &);

        bool operator==(const ProxyConfig &other) const {
            return (_type == other._type) && (_hostName == other._hostName) && (_port == other._port) &&
                   (_needsAuth == other._needsAuth) && (_user == other._user) && (_pwd == other._pwd) &&
                   (_keychainKey == other._keychainKey);
        }

        friend QDataStream &operator>>(QDataStream &in, ProxyConfig &proxyConfig);
        friend QDataStream &operator<<(QDataStream &out, const ProxyConfig &proxyConfig);

    private:
        ProxyType _type = ProxyType::None;
        std::string _hostName;
        int _port = 0;
        bool _needsAuth = false;
        std::string _user;
        std::string _pwd; // Password, stored in keystore
        std::string _keychainKey; // Keystore key, stored in DB
};

} // namespace KDC
