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

#include "keychainstorage.h"
#include "keychain/keychain.h"
#include "log/log.h"

#include <log4cplus/loggingmacros.h>

namespace KDC {

static const std::string package("com.infomaniak.drive");
static const std::string service("desktopclient");

bool KeyChainStorage::writePassword(const std::string &keychainKey, const std::string &rawData) {
    keychain::Error error{};
    keychain::setPassword(package, service, keychainKey, rawData, error);
    if (error) {
        LOG_DEBUG(KDC::Log::instance()->getLogger(),
                  "Failed to save authentication info to keychain: " << error.code << " - " << error.message);
        sentry::Handler::captureMessage(sentry::Level::Warning, "KeyChain::writeToken", error.message);

        return false;
    }

    return true;
}

bool KeyChainStorage::readPassword(const std::string &keychainKey, std::string &data, bool &found) {
    keychain::Error error{};
    data = keychain::getPassword(package, service, keychainKey, error);
    if (error.type == keychain::ErrorType::NotFound) {
        LOG_DEBUG(KDC::Log::instance()->getLogger(),
                  "Could not find data in keychain for key " << keychainKey << ": " << error.code << " - " << error.message);
        found = false;
        return true;
    } else if (error) {
        LOG_DEBUG(KDC::Log::instance()->getLogger(),
                  "Failed to retrieve data from keychain: " << error.code << " - " << error.message);
        return false;
    }

    found = true;
    return true;
}

bool KeyChainStorage::deletePassword(const std::string &keychainKey) {
    keychain::Error error{};
    keychain::deletePassword(package, service, keychainKey, error);
    if (error) {
        LOG_DEBUG(KDC::Log::instance()->getLogger(),
                  "Failed to delete authentication info from keychain: " << error.code << " - " << error.message);

        sentry::Handler::captureMessage(sentry::Level::Warning, "KeyChain::deleteToken", error.message);

        return false;
    }

    return true;
}

} // namespace KDC
