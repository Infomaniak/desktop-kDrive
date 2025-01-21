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

#include "libparms/parmslib.h"
#include "libcommon/utility/types.h"

#include <string>
#include <filesystem>

namespace KDC {

class MigrationSelectiveSync {
    public:
        MigrationSelectiveSync();
        MigrationSelectiveSync(int syncDbId, const std::filesystem::path &path, SyncNodeType type);

        void setSyncDbId(int syncDbId) { _syncDbId = syncDbId; }
        const int &syncDbId() const { return _syncDbId; }
        void setPath(const std::filesystem::path &path) { _path = path; }
        const std::filesystem::path &path() const { return _path; }
        inline void setType(SyncNodeType type) { _type = type; }
        inline SyncNodeType type() const { return _type; }

    private:
        int _syncDbId;
        std::filesystem::path _path;
        SyncNodeType _type;
};

} // namespace KDC
