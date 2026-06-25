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

#include <log4cplus/logger.h>

#include <memory>

namespace KDC {

class SyncDb;

class Migration {
    public:
        Migration(std::shared_ptr<SyncDb> synDbPtr);
        virtual ~Migration() = default;

        virtual bool migrate(const std::string &dbFromVersionNumber) = 0;
        log4cplus::Logger logger() const { return _logger; }
        std::shared_ptr<SyncDb> syncDb() { return _syncDbPtr; };

    private:
        std::shared_ptr<SyncDb> _syncDbPtr;
        log4cplus::Logger _logger;
};

} // namespace KDC
