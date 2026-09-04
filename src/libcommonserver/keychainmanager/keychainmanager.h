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

#pragma once

#include "libcommon.h"
#include "keychainstorage.h"
#include "apitoken.h"
#include "libcommon/utility/types.h"

#include <atomic>
#include <memory>
#include <thread>

#include <QObject>

namespace KDC {

class COMMON_EXPORT KeyChainManager : public QObject {
        Q_OBJECT

    public:
        static std::shared_ptr<KeyChainManager> instance(std::shared_ptr<IKeyChainStorage> storage = nullptr);

        KeyChainManager(KeyChainManager const &) = delete;
        void operator=(KeyChainManager const &) = delete;

        bool writeDummyTest();
        void clearDummyTest();

        bool writeData(const std::string &keychainKey, const std::string &rawData);
        ExitInfo readData(const std::string &keychainKey, std::string &data, bool &found);
        bool deleteData(const std::string &keychainKey);

        ExitInfo readApiToken(const std::string &keychainKey, ApiToken &apiToken, bool &found);

        [[nodiscard]] bool isTesting() const { return _storage ? _storage->isTesting() : false; }


    private:
        static std::shared_ptr<KeyChainManager> _instance;
        static constexpr uint16_t maxConcurrentKeychainReads = 10;

        std::shared_ptr<IKeyChainStorage> _storage;
        std::atomic<uint16_t> _inFlightReadThreads = 0;

        explicit KeyChainManager(std::shared_ptr<IKeyChainStorage> storage);
};

} // namespace KDC
