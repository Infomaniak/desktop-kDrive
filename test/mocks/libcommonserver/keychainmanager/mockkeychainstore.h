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

#include "libcommonserver/keychainmanager/ikeychainstore.h"
#include <unordered_map>

namespace KDC {

/**
 * @brief Mock implementation of IKeychainStore for unit testing.
 * Stores data in memory instead of using the system keychain.
 */
class MockKeychainStore : public IKeychainStore {
    public:
        MockKeychainStore() = default;
        ~MockKeychainStore() override = default;

        bool setPassword(const std::string &package, const std::string &service, const std::string &key,
                         const std::string &password, std::string &error) override {
            (void) error;
            const std::string fullKey = package + "/" + service + "/" + key;
            _data[fullKey] = password;

            return true;
        }

        bool getPassword(const std::string &package, const std::string &service, const std::string &key, std::string &password,
                         bool &found, std::string &error) override {
            (void) error;
            const std::string fullKey = package + "/" + service + "/" + key;
            if (const auto it = _data.find(fullKey); it != _data.end()) {
                password = it->second;
                found = true;
            } else {
                found = false;
            }

            return true;
        }

        bool deletePassword(const std::string &package, const std::string &service, const std::string &key,
                            std::string &error) override {
            (void) error;
            const std::string fullKey = package + "/" + service + "/" + key;
            _data.erase(fullKey);

            return true;
        }

        /**
         * @brief Clears all stored data (useful for test isolation)
         */
        void clear() { _data.clear(); }

        /**
         * @brief Returns whether a key exists
         */
        bool hasKey(const std::string &package, const std::string &service, const std::string &key) const {
            const std::string fullKey = package + "/" + service + "/" + key;

            return _data.contains(fullKey);
        }

        /**
         * @brief Pre-populate with data for testing
         */
        void setData(const std::string &package, const std::string &service, const std::string &key, const std::string &value) {
            const std::string fullKey = package + "/" + service + "/" + key;

            _data[fullKey] = value;
        }

        /**
         * @brief Configure behavior for simulating errors
         */
        void setFailNextOperation(bool fail, const std::string &errorMessage = "Mock error") {
            _failNext = fail;
            _errorMessage = errorMessage;
        }

    private:
        std::unordered_map<std::string, std::string> _data;
        bool _failNext = false;
        std::string _errorMessage;

        bool shouldFail(std::string &error) {
            if (_failNext) {
                error = _errorMessage;
                _failNext = false;
                return true;
            }
            return false;
        }
};

} // namespace KDC
