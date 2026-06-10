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

#include <log4cplus/loggingmacros.h>

namespace KDC {

static const std::string dummyKeychainKey("dummy_kdrive_keychain_key");
static const std::string dummyData("dummy");

std::shared_ptr<KeyChainManager> KeyChainManager::_instance = nullptr;

std::shared_ptr<KeyChainManager> KeyChainManager::instance(std::shared_ptr<IKeyChainStorage> storage /*= nullptr*/) {
    if (_instance == nullptr) {
        try {
            if (storage) {
                _instance = std::shared_ptr<KeyChainManager>(new KeyChainManager(storage));
                _instance->setIsTesting(true);
            } else {
                _instance = std::shared_ptr<KeyChainManager>(new KeyChainManager(std::make_shared<KeyChainStorage>()));
            }
        } catch (...) {
            return nullptr;
        }
    }

    return _instance;
}

KeyChainManager::KeyChainManager(std::shared_ptr<IKeyChainStorage> storage) :
    _storage(storage) {}

bool KeyChainManager::writeDummyTest() {
    // First, we check that we can write into the keychain
    if (!writeToken(dummyKeychainKey, dummyData)) {
        std::string error = "Test writing into the keychain failed. Token not refreshed.";
        LOG_WARN(Log::instance()->getLogger(), error);
        sentry::Handler::captureMessage(sentry::Level::Warning, "KeyChain::writeDummyTest", error);

        return false;
    }
    return true;
}

void KeyChainManager::clearDummyTest() {
    (void) deleteToken(dummyKeychainKey);
}

bool KeyChainManager::writeToken(const std::string &keychainKey, const std::string &rawData) {
    return _storage->writePassword(keychainKey, rawData);
}

bool KeyChainManager::readDataFromKeystore(const std::string &keychainKey, std::string &data, bool &found) {
    return _storage->readPassword(keychainKey, data, found);
}

bool KeyChainManager::readApiToken(const std::string &keychainKey, ApiToken &apiToken, bool &found) {
    std::string token;
    const bool returnValue = readDataFromKeystore(keychainKey, token, found);
    if (returnValue && found) {
        apiToken = ApiToken(token);
    }

    return returnValue;
}

bool KeyChainManager::deleteToken(const std::string &keychainKey) {
    return _storage->deletePassword(keychainKey);
}

} // namespace KDC
