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

#include "proxy.h"

#include <stdexcept>

namespace KDC {

std::shared_ptr<Proxy> Proxy::_instance = nullptr;

std::shared_ptr<Proxy> Proxy::instance(const ProxyConfig &proxyConfig) {
    if (_instance == nullptr) {
        if (proxyConfig.type() == ProxyType::Undefined) {
            return nullptr;
        }

        try {
            _instance = std::shared_ptr<Proxy>(new Proxy(proxyConfig));
        } catch (...) {
            return nullptr;
        }
    }

    return _instance;
}

Proxy::Proxy(const ProxyConfig &proxyConfig) : _proxyConfig(proxyConfig) {}

} // namespace KDC
