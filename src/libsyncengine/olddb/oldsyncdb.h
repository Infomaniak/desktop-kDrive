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

#include "libcommonserver/db/db.h"

#include <log4cplus/loggingmacros.h>

#include <mutex>
#include <list>

namespace KDC {

class OldSyncDb : public Db {
    public:
        OldSyncDb(const SyncPath &dbPath);

        bool create(bool &retry) override;
        bool prepare() override;
        bool upgrade(const std::string &fromVersion, const std::string &toVersion) override;

        bool selectAllSelectiveSync(std::list<std::pair<std::string, SyncNodeType>> &selectiveSyncList);
};

} // namespace KDC
