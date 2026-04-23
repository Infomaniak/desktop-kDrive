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

#include "libcommon.h"
#include "ikeychainstore.h"
#include "apitoken.h"

#include <memory>
#include <string>

#include <QObject>

namespace KDC {

class COMMON_EXPORT KeyChainManager : public QObject {
        Q_OBJECT

    public:
        /**
         * @brief Creates KeyChainManager with dependency injection for the store.
         * @param store The keychain storage implementation (can be mocked for testing)
         */
        explicit KeyChainManager(std::unique_ptr<IKeychainStore> store);
        ~KeyChainManager() override = default;

        KeyChainManager(KeyChainManager const &) = delete;
        void operator=(KeyChainManager const &) = delete;

        bool writeDummyTest();
        void clearDummyTest();

        bool writeToken(const std::string &keychainKey, const std::string &rawData);
        bool readApiToken(const std::string &keychainKey, ApiToken &apiToken, bool &found);
        bool deleteToken(const std::string &keychainKey);

        bool readDataFromKeystore(const std::string &keychainKey, std::string &data, bool &found);

        static constexpr const char *DEFAULT_PACKAGE = "com.infomaniak.drive";
        static constexpr const char *DEFAULT_SERVICE = "desktopclient";

    private:
        std::unique_ptr<IKeychainStore> _store;
        std::string _package{DEFAULT_PACKAGE};
        std::string _service{DEFAULT_SERVICE};

        static constexpr const char *dummyKeychainKey = "dummy_kdrive_keychain_key";
        static constexpr const char *dummyData = "dummy";

        friend class TestKeyChainManager;
};

class KeyChainManagerSingleton {
    public:
        static std::shared_ptr<KeyChainManager> instance() noexcept;
        static void setStore(std::unique_ptr<IKeychainStore> store) { _store = std::move(store); };

        friend class TestKeyChainManager;

    private:
        static std::unique_ptr<IKeychainStore> _store;
};

} // namespace KDC
