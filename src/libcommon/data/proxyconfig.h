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
        inline void setType(const ProxyType type) { _type = type; }
        inline const std::string &hostName() const { return _hostName; }
        inline void setHostName(const std::string &hostName) { _hostName = hostName; }
        inline int port() const { return _port; }
        inline void setPort(const int port) { _port = port; }
        inline bool needsAuth() const { return _needsAuth; }
        inline void setNeedsAuth(const bool needsAuth) { _needsAuth = needsAuth; }
        inline const std::string &user() const { return _user; }
        inline void setUser(const std::string &user) { _user = user; }

        inline const std::string &pwd() const { return _pwd; }
        inline void setPwd(const std::string &pwd) { _pwd = pwd; }

        inline const std::string &keychainKey() const { return _keychainKey; }
        inline void setKeychainKey(const std::string &keychainKey) { _keychainKey = keychainKey; }

        void toDynamicStruct(Poco::DynamicStruct &) const;
        void fromDynamicStruct(const Poco::DynamicStruct &);

        bool operator==(const ProxyConfig &other) const = default;

        /// TODO : to be removed once we moved to the new GUI ///
        friend QDataStream &operator>>(QDataStream &in, ProxyConfig &proxyConfig) {
            QString hostName;
            QString user;
            QString pwd;
            in >> proxyConfig._type >> hostName >> proxyConfig._port >> proxyConfig._needsAuth >> user >> pwd;
            proxyConfig._hostName = hostName.toStdString();
            proxyConfig._user = user.toStdString();
            proxyConfig._pwd = pwd.toStdString();
            return in;
        }
        friend QDataStream &operator<<(QDataStream &out, const ProxyConfig &proxyConfig) {
            out << proxyConfig._type << QString::fromStdString(proxyConfig._hostName) << proxyConfig._port
                << proxyConfig._needsAuth << QString::fromStdString(proxyConfig._user)
                << QString::fromStdString(proxyConfig._pwd);
            return out;
        }
        /////////////////////////////////////////////////////////

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
