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

#include "testincludes.h"
#include "utility/types.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/db/db.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"

namespace KDC {

class MockDb : public Db {
    public:
        explicit MockDb(const std::filesystem::path &dbPath) :
            Db(dbPath) {}

        static std::string makeDbFileName(int userId, int accountId, int driveId, int syncDbId) {
            std::string fileName = Db::makeDbFileName(userId, accountId, driveId, syncDbId);
            (void) fileName.append(CommonUtility::generateRandomStringAlphaNum());
            return fileName;
        }

        static std::string makeDbMockFileName() { return makeDbFileName(0, 0, 0, 0); }

        static std::filesystem::path makeDbName(int userId, int accountId, int driveId, int syncDbId, bool &alreadyExist) {
            return Db::makeDbName(userId, accountId, driveId, syncDbId, alreadyExist, makeDbFileName);
        }

        static std::filesystem::path makeDbName(bool &alreadyExist) { return makeDbName(0, 0, 0, 0, alreadyExist); }
};

} // namespace KDC
