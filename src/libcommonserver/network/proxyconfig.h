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
        inline const std::string &token() const { return _token; }
        inline void setToken(const std::string &token) { _token = token; }

    private:
        ProxyType _type = ProxyType::None;
        std::string _hostName;
        int _port = 0;
        bool _needsAuth = false;
        std::string _user;
        std::string _token;
};

}  // namespace KDC
