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

#include "oldsyncdb.h"
#include "libcommonserver/db/sqlitequery.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/utility/asserts.h"
#include "libcommonserver/log/log.h"

#include <3rdparty/sqlite3/sqlite3.h>

#define SELECT_ALL_SELECTIVESYNC_REQUEST_ID "select_selectivesync"
#define SELECT_ALL_SELECTIVESYNC_REQUEST "SELECT path, type FROM selectivesync;"

namespace KDC {

OldSyncDb::OldSyncDb(const SyncPath &dbPath) : Db(dbPath) {
    if (!checkConnect(std::string())) {
        throw std::runtime_error("Cannot open DB!");
    }

    LOG_INFO(_logger, "Old SyncDb initialization done");
}

bool OldSyncDb::create(bool &retry) {
    retry = false;
    return true;
}

bool OldSyncDb::prepare() {
    int errId = -1;
    std::string error;

    // Prepare request
    ASSERT(queryCreate(SELECT_ALL_SELECTIVESYNC_REQUEST_ID));
    if (!queryPrepare(SELECT_ALL_SELECTIVESYNC_REQUEST_ID, SELECT_ALL_SELECTIVESYNC_REQUEST, false, errId, error)) {
        queryFree(SELECT_ALL_SELECTIVESYNC_REQUEST_ID);
        return sqlFail(SELECT_ALL_SELECTIVESYNC_REQUEST_ID, error);
    }

    return true;
}

bool OldSyncDb::upgrade(const std::string &, const std::string &) {
    return true;
}

bool OldSyncDb::selectAllSelectiveSync(std::list<std::pair<std::string, SyncNodeType>> &selectiveSyncList) {
    const std::lock_guard<std::mutex> lock(_mutex);

    ASSERT(queryResetAndClearBindings(SELECT_ALL_SELECTIVESYNC_REQUEST_ID));
    bool found = false;
    for (;;) {
        if (!queryNext(SELECT_ALL_SELECTIVESYNC_REQUEST_ID, found)) {
            LOG_WARN(_logger, "Error getting query result: " << SELECT_ALL_SELECTIVESYNC_REQUEST_ID);
            return false;
        }
        if (!found) {
            break;
        }

        std::string path;
        ASSERT(queryStringValue(SELECT_ALL_SELECTIVESYNC_REQUEST_ID, 0, path));
        int type = -1;
        ASSERT(queryIntValue(SELECT_ALL_SELECTIVESYNC_REQUEST_ID, 1, type));

        selectiveSyncList.push_back(std::make_pair(path, fromInt<SyncNodeType>(type)));
    }
    ASSERT(queryResetAndClearBindings(SELECT_ALL_SELECTIVESYNC_REQUEST_ID));

    return true;
}

}  // namespace KDC
