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
#include <condition_variable>
#include <mutex>
#include <thread>
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

ExitInfo KeyChainManager::readData(const std::string &keychainKey, std::string &data, bool &found) {
    constexpr auto keychainReadTimeout = std::chrono::seconds(60);

    struct ReadState {
            std::mutex mutex;
            std::condition_variable conditionVariable;
            bool done = false;
            bool ok = false;
            bool localFound = false;
            std::string localData;
    };

    const auto state = std::make_shared<ReadState>();

    std::thread([this, keychainKey, state]() {
        std::string tmpData;
        bool tmpFound = false;
        const bool ok = _storage->readPassword(keychainKey, tmpData, tmpFound);

        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->ok = ok;
            state->localFound = tmpFound;
            state->localData = std::move(tmpData);
            state->done = true;
        }
        state->conditionVariable.notify_one();
    }).detach();

    std::unique_lock<std::mutex> lock(state->mutex);
    if (!state->conditionVariable.wait_for(lock, keychainReadTimeout, [&state]() { return state->done; })) {
        LOG_WARN(Log::instance()->getLogger(), "Timeout while reading data from keychain");
        found = false;
        return {ExitCode::SystemError, ExitCause::KeychainAccessTimeout};
    }
    if (!state->ok) {
        found = false;
        return {ExitCode::SystemError, ExitCause::KeychainAccessError};
    }

    data = std::move(state->localData);
    found = state->localFound;
    return ExitCode::Ok;
}

ExitInfo KeyChainManager::readApiToken(const std::string &keychainKey, ApiToken &apiToken, bool &found) {
    std::string token;
    const auto returnValue = readData(keychainKey, token, found);
    if (returnValue && found) {
        apiToken = ApiToken(token);
    }

    return returnValue;
}

bool KeyChainManager::deleteData(const std::string &keychainKey) {
    return _storage->deletePassword(keychainKey);
}

} // namespace KDC
