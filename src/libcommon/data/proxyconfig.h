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
        ProxyConfig(ProxyType type, const std::string &hostName, const Port port, const bool needsAuth,
                    const std::string &user = "", const std::string &pwd = "");

        [[nodiscard]] ProxyType type() const { return _type; }
        void setType(const ProxyType type) { _type = type; }
        [[nodiscard]] const std::string &hostName() const { return _hostName; }
        void setHostName(const std::string &hostName) { _hostName = hostName; }
        [[nodiscard]] Port port() const { return _port; }
        void setPort(const Port port) { _port = port; }
        [[nodiscard]] bool needsAuth() const { return _needsAuth; }
        void setNeedsAuth(const bool needsAuth) { _needsAuth = needsAuth; }
        [[nodiscard]] const std::string &user() const { return _user; }
        void setUser(const std::string &user) { _user = user; }

        [[nodiscard]] const std::string &pwd() const { return _pwd; }
        void setPwd(const std::string &pwd) { _pwd = pwd; }

        [[nodiscard]] const std::string &keychainKey() const { return _keychainKey; }
        void setKeychainKey(const std::string &keychainKey) { _keychainKey = keychainKey; }

        void clear();

        void toDynamicStruct(Poco::DynamicStruct &) const;
        void fromDynamicStruct(const Poco::DynamicStruct &);

        bool operator==(const ProxyConfig &other) const = default;

        /// TODO : to be removed once we moved to the new GUI ///
        friend QDataStream &operator>>(QDataStream &in, ProxyConfig &proxyConfig) {
            QString hostName;
            qint16 port = 0;
            QString user;
            QString pwd;
            in >> proxyConfig._type >> hostName >> port >> proxyConfig._needsAuth >> user >> pwd;
            proxyConfig.setHostName(hostName.toStdString());
            proxyConfig.setUser(user.toStdString());
            proxyConfig.setPwd(pwd.toStdString());
            proxyConfig.setPort(static_cast<Port>(port));
            return in;
        }
        friend QDataStream &operator<<(QDataStream &out, const ProxyConfig &proxyConfig) {
            out << proxyConfig._type << QString::fromStdString(proxyConfig._hostName) << static_cast<qint16>(proxyConfig._port)
                << proxyConfig._needsAuth << QString::fromStdString(proxyConfig._user)
                << QString::fromStdString(proxyConfig._pwd);
            return out;
        }
        /////////////////////////////////////////////////////////

    private:
        ProxyType _type = ProxyType::None;
        std::string _hostName;
        Port _port = 0;
        bool _needsAuth = false;
        std::string _user;
        std::string _pwd; // Password, stored in keystore
        std::string _keychainKey; // Keystore key, stored in DB
};

} // namespace KDC
