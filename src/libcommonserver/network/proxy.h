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

#include "proxyconfig.h"

#include <memory>
#include <string>

namespace KDC {

class Proxy {
    public:
        static std::shared_ptr<Proxy> instance(const ProxyConfig &proxyConfig = ProxyConfig());

        Proxy(Proxy const &) = delete;
        void operator=(Proxy const &) = delete;
        inline const ProxyConfig &proxyConfig() { return _proxyConfig; }
        inline void setProxyConfig(const ProxyConfig &proxyConfig) { _proxyConfig = proxyConfig; }

    private:
        static std::shared_ptr<Proxy> _instance;
        ProxyConfig _proxyConfig;

        Proxy(const ProxyConfig &proxyConfig);
};

}  // namespace KDC
