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

#include "syncdbinmemory.h"

namespace KDC {
SyncDbInMemory::SyncDbInMemory(const std::string& dbPath, const std::string& version, const std::string& targetNodeId) :
    SyncDb(dbPath, version, targetNodeId) {}

bool SyncDbInMemory::openDb(const std::filesystem::path& dbPath) {
    std::shared_ptr<SqliteDb> tmpSqliteDb;
    if (!tmpSqliteDb->openOrCreateReadWrite(_dbPath)) {
        std::string error = tmpSqliteDb->error();
        LOG_WARN(_logger, "Error opening the db in: " << error.c_str());
        return false;
    }



    return false;
}
} // namespace KDC
