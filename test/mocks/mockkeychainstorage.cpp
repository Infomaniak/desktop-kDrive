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

#include "mockkeychainstorage.h"

namespace KDC {

bool MockKeyChainStorage::writePassword(const std::string &keychainKey, const std::string &rawData) {
    _storage[keychainKey] = rawData;
    return true;
}

bool MockKeyChainStorage::readPassword(const std::string &keychainKey, std::string &data, bool &found) {
    if (_storage.contains(keychainKey)) {
        data = _storage[keychainKey];
        found = true;
    } else {
        found = false;
    }
    return true;
}

bool MockKeyChainStorage::deletePassword(const std::string &keychainKey) {
    if (_storage.contains(keychainKey)) {
        (void) _storage.erase(keychainKey);
        return true;
    }
    return false;
}

} // namespace KDC
