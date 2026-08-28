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

#include "keychainmanager.h"
#include "log/log.h"

#include <chrono>
#include <future>
#include <log4cplus/loggingmacros.h>

namespace KDC {

static const std::string dummyKeychainKey("dummy_kdrive_keychain_key");
static const std::string dummyData("dummy");

std::shared_ptr<KeyChainManager> KeyChainManager::_instance = nullptr;

std::shared_ptr<KeyChainManager> KeyChainManager::instance(const std::shared_ptr<IKeyChainStorage> storage /*= nullptr*/) {
    if (_instance == nullptr) {
        try {
            if (storage) {
                _instance = std::shared_ptr<KeyChainManager>(new KeyChainManager(storage));
            } else {
                _instance = std::shared_ptr<KeyChainManager>(new KeyChainManager(std::make_shared<KeyChainStorage>()));
            }
        } catch (...) {
            return nullptr;
        }
    }

    return _instance;
}

KeyChainManager::KeyChainManager(const std::shared_ptr<IKeyChainStorage> storage) :
    _storage(storage) {}

bool KeyChainManager::writeDummyTest() {
    // First, we check that we can write into the keychain
    if (!writeData(dummyKeychainKey, dummyData)) {
        std::string error = "Test writing into the keychain failed. Token not refreshed.";
        LOG_WARN(Log::instance()->getLogger(), error);
        sentry::Handler::captureMessage(sentry::Level::Warning, "KeyChain::writeDummyTest", error);

        return false;
    }
    return true;
}

void KeyChainManager::clearDummyTest() {
    (void) deleteData(dummyKeychainKey);
}

bool KeyChainManager::writeData(const std::string &keychainKey, const std::string &rawData) {
    return _storage->writePassword(keychainKey, rawData);
}

bool KeyChainManager::readData(const std::string &keychainKey, std::string &data, bool &found) {
    constexpr auto keychainReadTimeout = std::chrono::seconds(60);

    auto future = std::async(std::launch::async, [this, keychainKey]() {
        std::string localData;
        bool localFound = false;
        const bool ok = _storage->readPassword(keychainKey, localData, localFound);
        return std::tuple<bool, std::string, bool>(ok, std::move(localData), localFound);
    });

    if (future.wait_for(keychainReadTimeout) != std::future_status::ready) {
        LOG_WARN(Log::instance()->getLogger(), "Timeout while reading data from keychain after 60 seconds");
        found = false;
        return false;
    }

    const auto [ok, readData, localFound] = future.get();
    if (!ok) {
        found = false;
        return false;
    }

    data = readData;
    found = localFound;
    return true;
}

bool KeyChainManager::readApiToken(const std::string &keychainKey, ApiToken &apiToken, bool &found) {
    std::string token;
    const bool returnValue = readData(keychainKey, token, found);
    if (returnValue && found) {
        apiToken = ApiToken(token);
    }

    return returnValue;
}

bool KeyChainManager::deleteData(const std::string &keychainKey) {
    return _storage->deletePassword(keychainKey);
}

} // namespace KDC
