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

#include "keychainmanager.h"
#include "keychainstore.h"
#include "log/sentry/handler.h"

#include <log4cplus/loggingmacros.h>

namespace KDC {

const std::string KeyChainManager::dummyKeychainKey("dummy_kdrive_keychain_key");
const std::string KeyChainManager::dummyData("dummy");

std::shared_ptr<KeyChainManager> KeyChainManager::_instance = nullptr;

KeyChainManager::KeyChainManager(std::unique_ptr<IKeychainStore> store) :
    _store(std::move(store)),
    _package(KeyChainManager::DEFAULT_PACKAGE),
    _service(KeyChainManager::DEFAULT_SERVICE) {
    if (!_store) {
        throw std::invalid_argument("KeychainStore cannot be null");
    }
}

std::shared_ptr<KeyChainManager> KeyChainManager::instance() {
    if (_instance == nullptr) {
        try {
            auto store = std::make_unique<KeychainStore>();
            _instance = std::make_shared<KeyChainManager>(std::move(store));
        } catch (...) {
            return nullptr;
        }
    }

    return _instance;
}

bool KeyChainManager::writeDummyTest() {
    std::string error;
    if (!_store->setPassword(_package, _service, dummyKeychainKey, dummyData, error)) {
        std::string errorMsg = "Test writing into the keychain failed. Token not refreshed.";
        LOG_WARN(Log::instance()->getLogger(), errorMsg);
        sentry::Handler::captureMessage(sentry::Level::Warning, "KeyChain::writeDummyTest", error);
        return false;
    }
    return true;
}

void KeyChainManager::clearDummyTest() {
    std::string error;
    _store->deletePassword(_package, _service, dummyKeychainKey, error);
}

bool KeyChainManager::writeToken(const std::string &keychainKey, const std::string &rawData) {
    std::string error;
    if (!_store->setPassword(_package, _service, keychainKey, rawData, error)) {
        LOG_DEBUG(Log::instance()->getLogger(), "Failed to save authentication info to keychain: " << error);
        sentry::Handler::captureMessage(sentry::Level::Warning, "KeyChain::writeToken", error);
        return false;
    }
    return true;
}

bool KeyChainManager::readDataFromKeystore(const std::string &keychainKey, std::string &data, bool &found) {
    std::string error;
    if (!_store->getPassword(_package, _service, keychainKey, data, found, error)) {
        LOG_DEBUG(Log::instance()->getLogger(), "Failed to retrieve data from keychain: " << error);
        return false;
    }
    return true;
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
    std::string error;
    if (!_store->deletePassword(_package, _service, keychainKey, error)) {
        LOG_DEBUG(KDC::Log::instance()->getLogger(), "Failed to delete authentication info from keychain: " << error);
        sentry::Handler::captureMessage(sentry::Level::Warning, "KeyChain::deleteToken", error);
        return false;
    }
    return true;
}

} // namespace KDC
