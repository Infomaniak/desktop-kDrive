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

#include "keychainmanager.h"
#include "keychain/keychain.h"
#include "theme/theme.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"

#include <log4cplus/loggingmacros.h>

#include <sentry.h>

#define PACKAGE "com.infomaniak.drive"
#define SERVICE "desktopclient"

namespace KDC {

static const std::string dummyKeychainKey("dummy_kdrive_keychain_key");
static const std::string dummyData("dummy");

std::shared_ptr<KeyChainManager> KeyChainManager::_instance = nullptr;

std::shared_ptr<KeyChainManager> KeyChainManager::instance(bool testing) {
    if (_instance == nullptr) {
        _instance = std::shared_ptr<KeyChainManager>(new KeyChainManager(testing));
    }

    return _instance;
}

KeyChainManager::KeyChainManager(bool testing) : _testing(testing), _testingMap(std::unordered_map<std::string, std::string>()) {}

bool KeyChainManager::writeDummyTest() {
    // First, we check that we can write into the keychain
    if (!KeyChainManager::instance()->writeToken(dummyKeychainKey, dummyData)) {
        std::string error = "Test writting into the keychain failed. Token not refreshed.";
        LOG_WARN(Log::instance()->getLogger(), error.c_str());
#ifdef NDEBUG
        sentry_capture_event(
            sentry_value_new_message_event(SENTRY_LEVEL_WARNING, "KeyChainManager::writeDummyTest", error.c_str()));
#endif

        return false;
    }
    return true;
}

void KeyChainManager::clearDummyTest() {
    KeyChainManager::instance()->deleteToken(dummyKeychainKey);
}

bool KeyChainManager::writeToken(const std::string &keychainKey, const std::string &rawData) {
    if (_testing) {
        LOG_DEBUG(KDC::Log::instance()->getLogger(), "Testing");
        _testingMap[keychainKey] = rawData;
        return true;
    }

    keychain::Error error{};
    keychain::setPassword(PACKAGE, SERVICE, keychainKey, rawData, error);
    if (error) {
        LOG_DEBUG(KDC::Log::instance()->getLogger(),
                  "Failed to save authentification info to keychain: " << error.code << " - " << error.message.c_str());

#ifdef NDEBUG
        sentry_capture_event(
            sentry_value_new_message_event(SENTRY_LEVEL_WARNING, "KeyChainManager::writeToken", error.message.c_str()));
#endif

        return false;
    }

    return true;
}

#if defined(__unix__) && !defined(__APPLE__)
std::string exec(const char *cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}
#endif

bool KeyChainManager::readDataFromKeystore(const std::string &keychainKey, std::string &data, bool &found) {
    keychain::Error error{};
    data = keychain::getPassword(PACKAGE, SERVICE, keychainKey, error);
    if (error.type == keychain::ErrorType::NotFound) {
        LOG_DEBUG(KDC::Log::instance()->getLogger(), "Could not find data in keychain for key " << keychainKey.c_str() << ": "
                                                                                                << error.code << " - "
                                                                                                << error.message.c_str());
        found = false;
        return true;
    } else if (error) {
        LOG_DEBUG(KDC::Log::instance()->getLogger(),
                  "Failed to retrieve data from keychain: " << error.code << " - " << error.message.c_str());
        return false;
    }

    found = true;
    return true;
}

bool KeyChainManager::readApiToken(const std::string &keychainKey, ApiToken &apiToken, bool &found) {
    if (_testing) {
        if (_testingMap.find(keychainKey) != _testingMap.end()) {
            apiToken = _testingMap[keychainKey];
            found = true;
        } else {
            found = false;
        }
        return true;
    }

    std::string token;
    bool returnValue = readDataFromKeystore(keychainKey, token, found);
    if (returnValue && found) {
        apiToken = ApiToken(token);
    }

    return returnValue;
}

bool KeyChainManager::deleteToken(const std::string &keychainKey) {
    keychain::Error error{};
    keychain::deletePassword(PACKAGE, SERVICE, keychainKey, error);
    if (error) {
        LOG_DEBUG(KDC::Log::instance()->getLogger(),
                  "Failed to delete authentification info to keychain: " << error.code << " - " << error.message.c_str());

#ifdef NDEBUG
        sentry_capture_event(
            sentry_value_new_message_event(SENTRY_LEVEL_WARNING, "KeyChainManager::deleteToken", error.message.c_str()));
#endif

        return false;
    }

    return true;
}

}  // namespace KDC
