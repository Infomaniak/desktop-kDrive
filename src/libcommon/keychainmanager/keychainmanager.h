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

#pragma once

#include "libcommon.h"
#include "apitoken.h"

#include <unordered_map>

#include <QObject>

namespace KDC {

class COMMON_EXPORT KeyChainManager : public QObject {
        Q_OBJECT

    public:
        static std::shared_ptr<KeyChainManager> instance(bool testing = false) noexcept;

        KeyChainManager(KeyChainManager const &) = delete;
        void operator=(KeyChainManager const &) = delete;

        bool writeDummyTest();
        void clearDummyTest();

        bool writeToken(const std::string &keychainKey, const std::string &rawData);
        bool readApiToken(const std::string &keychainKey, ApiToken &apiToken, bool &found);
        bool deleteToken(const std::string &keychainKey);

        bool readDataFromKeystore(const std::string &keychainKey, std::string &data, bool &found);

    private:
        static std::shared_ptr<KeyChainManager> _instance;
        bool _testing;
        std::unordered_map<std::string, std::string> _testingMap;

        KeyChainManager(bool testing);
};

} // namespace KDC
