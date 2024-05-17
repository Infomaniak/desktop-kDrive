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

#include "parmsdb.h"
#include "libcommonserver/utility/asserts.h"
#include "libcommon/utility/types.h"

constexpr char CREATE_APP_STATE_TABLE_ID[] = "create_app_state";
constexpr char CREATE_APP_STATE_TABLE[] = "CREATE TABLE IF NOT EXISTS app_state(key INTEGER PRIMARY KEY, value TEXT);";

constexpr char INSERT_APP_STATE_REQUEST_ID[] = "insert_app_state";
constexpr char INSERT_APP_STATE_REQUEST[] = "INSERT INTO app_state (key, value) VALUES (?1, ?2);";

constexpr char SELECT_APP_STATE_REQUEST_ID[] = "select_value_from_key";
constexpr char SELECT_APP_STATE_REQUEST[] = "SELECT value FROM app_state WHERE key=?1;";

constexpr char UPDATE_APP_STATE_REQUEST_ID[] = "update_value_with_key";
constexpr char UPDATE_APP_STATE_REQUEST[] = "UPDATE app_state SET value=?2 WHERE key=?1;";

constexpr char APP_STATE_KEY_DEFAULT_LastServerSelfRestartDate[] = "0";
constexpr char APP_STATE_KEY_DEFAULT_LastClientSelfRestartDate[] = "0";
constexpr char APP_STATE_KEY_DEFAULT_LastLogUploadDate[] = "0";
constexpr char APP_STATE_KEY_DEFAULT_LastLogUploadArchivePath[] = "";
constexpr char APP_STATE_KEY_DEFAULT_LogUploadState[] = {static_cast<char>(KDC::LogUploadState::None), '\0'};
constexpr char APP_STATE_KEY_DEFAULT_LogUploadPercent[] = "0";


namespace KDC {

bool ParmsDb::createAppState() {
    int errId = 0;
    std::string error;

    ASSERT(queryCreate(CREATE_APP_STATE_TABLE_ID));
    if (!queryPrepare(CREATE_APP_STATE_TABLE_ID, CREATE_APP_STATE_TABLE, false, errId, error)) {
        queryFree(CREATE_APP_STATE_TABLE_ID);
        return sqlFail(CREATE_APP_STATE_TABLE_ID, error);
    }
    if (!queryExec(CREATE_APP_STATE_TABLE_ID, errId, error)) {
        queryFree(CREATE_APP_STATE_TABLE_ID);
        return sqlFail(CREATE_APP_STATE_TABLE_ID, error);
    }
    queryFree(CREATE_APP_STATE_TABLE_ID);
    return true;
}

bool ParmsDb::prepareAppState() {
    int errId = 0;
    std::string error;

    ASSERT(queryCreate(INSERT_APP_STATE_REQUEST_ID));
    if (!queryPrepare(INSERT_APP_STATE_REQUEST_ID, INSERT_APP_STATE_REQUEST, false, errId, error)) {
        queryFree(INSERT_APP_STATE_REQUEST_ID);
        return sqlFail(INSERT_APP_STATE_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_APP_STATE_REQUEST_ID));
    if (!queryPrepare(SELECT_APP_STATE_REQUEST_ID, SELECT_APP_STATE_REQUEST, false, errId, error)) {
        queryFree(SELECT_APP_STATE_REQUEST_ID);
        return sqlFail(SELECT_APP_STATE_REQUEST_ID, error);
    }

    ASSERT(queryCreate(UPDATE_APP_STATE_REQUEST_ID));
    if (!queryPrepare(UPDATE_APP_STATE_REQUEST_ID, UPDATE_APP_STATE_REQUEST, false, errId, error)) {
        queryFree(UPDATE_APP_STATE_REQUEST_ID);
        return sqlFail(UPDATE_APP_STATE_REQUEST_ID, error);
    }
    return true;
}

bool ParmsDb::insertDefaultAppState() {
    if (!insertAppState(AppStateKey::LastServerSelfRestartDate, APP_STATE_KEY_DEFAULT_LastServerSelfRestartDate)) {
        LOG_WARN(_logger, "Error inserting default value for LastServerSelfRestartDate");
        return false;
    }

    if (!insertAppState(AppStateKey::LastClientSelfRestartDate, APP_STATE_KEY_DEFAULT_LastClientSelfRestartDate)) {
        LOG_WARN(_logger, "Error inserting default value for LastClientSelfRestartDate");
        return false;
    }

    if (!insertAppState(AppStateKey::LastSuccessfulLogUploadDate, APP_STATE_KEY_DEFAULT_LastLogUploadDate)) {
        LOG_WARN(_logger, "Error inserting default value for LastSuccessfulLogUploadDate");
        return false;
    }

    if (!insertAppState(AppStateKey::LastLogUploadArchivePath, APP_STATE_KEY_DEFAULT_LastLogUploadArchivePath)) {
        LOG_WARN(_logger, "Error inserting default value for LastLogUploadArchivePath");
        return false;
    }

    if (!insertAppState(AppStateKey::LogUploadState, APP_STATE_KEY_DEFAULT_LogUploadState)) {
        LOG_WARN(_logger, "Error inserting default value for LogUploadState");
        return false;
    }

    if (!insertAppState(AppStateKey::LogUploadPercent, APP_STATE_KEY_DEFAULT_LogUploadPercent)) {
        LOG_WARN(_logger, "Error inserting default value for LogUploadPercent");
        return false;
    }

    return true;
}

bool ParmsDb::insertAppState(AppStateKey key, const std::string &value) {
    const std::scoped_lock lock(_mutex);
    int errId = 0;
    std::string error;
    bool found = false;

    ASSERT(queryResetAndClearBindings(SELECT_APP_STATE_REQUEST_ID));
    ASSERT(queryBindValue(SELECT_APP_STATE_REQUEST_ID, 1, static_cast<int>(key)));
    if (!queryNext(SELECT_APP_STATE_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_APP_STATE_REQUEST_ID);
        return false;
    }
    ASSERT(queryResetAndClearBindings(SELECT_APP_STATE_REQUEST_ID));

    if (!found) {
        ASSERT(queryResetAndClearBindings(INSERT_APP_STATE_REQUEST_ID));
        ASSERT(queryBindValue(INSERT_APP_STATE_REQUEST_ID, 1, static_cast<int>(key)));
        ASSERT(queryBindValue(INSERT_APP_STATE_REQUEST_ID, 2, value));
        if (!queryExec(INSERT_APP_STATE_REQUEST_ID, errId, error)) {
            LOG_WARN(_logger, "Error running query: " << INSERT_APP_STATE_REQUEST_ID);
            return false;
        }
    }
    return true;
}

bool ParmsDb::selectAppState(AppStateKey key, std::string &value, bool &found) {
    const std::scoped_lock lock(_mutex);
    found = false;

    ASSERT(queryResetAndClearBindings(SELECT_APP_STATE_REQUEST_ID));
    ASSERT(queryBindValue(SELECT_APP_STATE_REQUEST_ID, 1, static_cast<int>(key)));
    if (!queryNext(SELECT_APP_STATE_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_APP_STATE_REQUEST_ID);
        return false;
    }

    if (!found) {
        LOG_WARN(_logger, "AppStateKey not found: " << static_cast<int>(key));
        return true;
    }
    ASSERT(queryStringValue(SELECT_APP_STATE_REQUEST_ID, 0, value));
    ASSERT(queryResetAndClearBindings(SELECT_APP_STATE_REQUEST_ID));
    return true;
}

bool ParmsDb::updateAppState(AppStateKey key, const std::string &value, bool &found) {
    std::string existingValue;
    int errId = 0;
    std::string error;
    if (!selectAppState(key, existingValue, found)) {
        return false;
    }

    if (!found) {
        return true;
    }

    const std::scoped_lock lock(_mutex);
    if (found) {
        ASSERT(queryResetAndClearBindings(UPDATE_APP_STATE_REQUEST_ID));
        ASSERT(queryBindValue(UPDATE_APP_STATE_REQUEST_ID, 1, static_cast<int>(key)));
        ASSERT(queryBindValue(UPDATE_APP_STATE_REQUEST_ID, 2, value));
        if (!queryExec(UPDATE_APP_STATE_REQUEST_ID, errId, error)) {
            LOG_WARN(_logger, "Error running query: " << UPDATE_APP_STATE_REQUEST_ID);
            return false;
        }
        ASSERT(queryResetAndClearBindings(UPDATE_APP_STATE_REQUEST_ID));
    }
    return true;
}

}  // namespace KDC