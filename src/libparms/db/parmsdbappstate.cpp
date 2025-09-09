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

#include "parmsdb.h"
#include "libcommonserver/utility/utility.h"
#include "libcommon/utility/logiffail.h"
#include "libcommon/utility/types.h"
#include "libcommon/utility/utility.h"

constexpr char CREATE_APP_STATE_TABLE_ID[] = "create_app_state";
constexpr char CREATE_APP_STATE_TABLE[] = "CREATE TABLE IF NOT EXISTS app_state(key INTEGER PRIMARY KEY, value TEXT);";

constexpr char INSERT_APP_STATE_REQUEST_ID[] = "insert_app_state";
constexpr char INSERT_APP_STATE_REQUEST[] = "INSERT INTO app_state (key, value) VALUES (?1, ?2);";

constexpr char SELECT_APP_STATE_REQUEST_ID[] = "select_value_from_key";
constexpr char SELECT_APP_STATE_REQUEST[] = "SELECT value FROM app_state WHERE key=?1;";

constexpr char UPDATE_APP_STATE_REQUEST_ID[] = "update_value_with_key";
constexpr char UPDATE_APP_STATE_REQUEST[] = "UPDATE app_state SET value=?2 WHERE key=?1;";

// Default values for AppState (Empty string is not allowed, use APP_STATE_DEFAULT_IS_EMPTY instead)
constexpr char APP_STATE_DEFAULT_IS_EMPTY[] = "__DEFAULT_IS_EMPTY__";

constexpr char APP_STATE_KEY_DEFAULT_LastServerSelfRestartDate[] = "0";
constexpr char APP_STATE_KEY_DEFAULT_LastClientSelfRestartDate[] = "0";
constexpr char APP_STATE_KEY_DEFAULT_LastLogUploadDate[] = "0";
constexpr const char *APP_STATE_KEY_DEFAULT_LastLogUploadArchivePath = APP_STATE_DEFAULT_IS_EMPTY;
constexpr char APP_STATE_KEY_DEFAULT_LogUploadState[] = "0"; // KDC::LogUploadState::None
constexpr char APP_STATE_KEY_DEFAULT_LogUploadPercent[] = "0";
constexpr const char *APP_STATE_KEY_DEFAULT_LogUploadToken = APP_STATE_DEFAULT_IS_EMPTY;
constexpr char APP_STATE_KEY_DEFAULT_NoUpdate[] = "0";

namespace KDC {

bool ParmsDb::createAppState() {
    LOG_INFO(_logger, "Creating table app_state");
    if (!createAndPrepareRequest(CREATE_APP_STATE_TABLE_ID, CREATE_APP_STATE_TABLE)) return false;
    int errId = 0;
    std::string error;
    if (!queryExec(CREATE_APP_STATE_TABLE_ID, errId, error)) {
        queryFree(CREATE_APP_STATE_TABLE_ID);
        return sqlFail(CREATE_APP_STATE_TABLE_ID, error);
    }
    queryFree(CREATE_APP_STATE_TABLE_ID);
    return true;
}

bool ParmsDb::prepareAppState() {
    if (!createAndPrepareRequest(INSERT_APP_STATE_REQUEST_ID, INSERT_APP_STATE_REQUEST)) return false;
    if (!createAndPrepareRequest(SELECT_APP_STATE_REQUEST_ID, SELECT_APP_STATE_REQUEST)) return false;
    if (!createAndPrepareRequest(UPDATE_APP_STATE_REQUEST_ID, UPDATE_APP_STATE_REQUEST)) return false;
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

    if (!insertAppState(AppStateKey::LogUploadToken, APP_STATE_KEY_DEFAULT_LogUploadToken)) {
        LOG_WARN(_logger, "Error when inserting default value for LogUploadToken");
        return false;
    }

    if (!insertAppState(AppStateKey::AppUid, CommonUtility::generateRandomStringAlphaNum(25), true)) {
        LOG_WARN(_logger, "Error when inserting default value for LogUploadToken");
        return false;
    }

    if (!insertAppState(AppStateKey::NoUpdate, APP_STATE_KEY_DEFAULT_NoUpdate)) {
        LOG_WARN(_logger, "Error inserting default value for NoUpdate");
        return false;
    }

    return true;
}

bool ParmsDb::insertAppState(AppStateKey key, const std::string &value, const bool updateOnlyIfEmpty /*= false*/) {
    const std::scoped_lock lock(_mutex);
    std::string valueStr = value;
    if (valueStr.empty()) {
        LOG_WARN(_logger, "Value is empty for AppStateKey: " << CommonUtility::appStateKeyToString(key));
        return false;
    }
    if (valueStr == APP_STATE_DEFAULT_IS_EMPTY) {
        valueStr = "";
    }

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_APP_STATE_REQUEST_ID))
    LOG_IF_FAIL(queryBindValue(SELECT_APP_STATE_REQUEST_ID, 1, static_cast<int>(key)))
    bool found = false;
    if (!queryNext(SELECT_APP_STATE_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_APP_STATE_REQUEST_ID);
        return false;
    }
    std::string existingValue;
    if (found) {
        LOG_IF_FAIL(queryStringValue(SELECT_APP_STATE_REQUEST_ID, 0, existingValue))
    }
    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_APP_STATE_REQUEST_ID))

    if (!found || (updateOnlyIfEmpty && existingValue.empty())) {
        const auto requestId = found ? UPDATE_APP_STATE_REQUEST_ID : INSERT_APP_STATE_REQUEST_ID;
        LOG_IF_FAIL(queryBindValue(requestId, 1, static_cast<int>(key)))
        LOG_IF_FAIL(queryBindValue(requestId, 2, valueStr))
        int errId = 0;
        std::string error;
        if (!queryExec(requestId, errId, error)) {
            LOG_WARN(_logger, "Error running query: " << requestId);
            return false;
        }
        LOG_IF_FAIL(queryResetAndClearBindings(requestId))
    }
    return true;
}

bool ParmsDb::selectAppState(AppStateKey key, AppStateValue &value, bool &found) {
    const std::scoped_lock lock(_mutex);
    found = false;
    std::string valueStr;

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_APP_STATE_REQUEST_ID))
    LOG_IF_FAIL(queryBindValue(SELECT_APP_STATE_REQUEST_ID, 1, static_cast<int>(key)))
    if (!queryNext(SELECT_APP_STATE_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_APP_STATE_REQUEST_ID);
        return false;
    }

    if (!found) {
        sentry::Handler::captureMessage(sentry::Level::Error, "Missing AppStateKey",
                                        "AppStateKey::" + toString(key) + " not found in selectAppState.");
        LOG_WARN(_logger, "AppStateKey not found: " << key);
        return true;
    }
    LOG_IF_FAIL(queryStringValue(SELECT_APP_STATE_REQUEST_ID, 0, valueStr))
    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_APP_STATE_REQUEST_ID))

    if (!CommonUtility::stringToAppStateValue(valueStr, value)) {
        LOG_WARN(_logger, "Unable to convert value from string in selectAppState");
        return false;
    }

    return true;
};

bool ParmsDb::updateAppState(AppStateKey key, const AppStateValue &value, bool &found) {
    AppStateValue existingValue;
    int errId = 0;

    if (!selectAppState(key, existingValue, found)) {
        return false;
    }

    if (!found) {
        sentry::Handler::captureMessage(sentry::Level::Error, "Missing AppStateKey",
                                        "AppStateKey::" + toString(key) + " not found in updateAppState.");
        LOG_WARN(_logger, "AppStateKey not found: " << key);
        return true;
    }

    std::string valueStr;
    if (!CommonUtility::appStateValueToString(value, valueStr)) {
        LOG_WARN(_logger, "Unable to convert value to string in updateAppState");
        return false;
    }

    const std::scoped_lock lock(_mutex);
    if (found) {
        std::string error;
        LOG_IF_FAIL(queryResetAndClearBindings(UPDATE_APP_STATE_REQUEST_ID))
        LOG_IF_FAIL(queryBindValue(UPDATE_APP_STATE_REQUEST_ID, 1, static_cast<int>(key)))
        LOG_IF_FAIL(queryBindValue(UPDATE_APP_STATE_REQUEST_ID, 2, valueStr))
        if (!queryExec(UPDATE_APP_STATE_REQUEST_ID, errId, error)) {
            LOG_WARN(_logger, "Error running query: " << UPDATE_APP_STATE_REQUEST_ID);
            return false;
        }
        LOG_IF_FAIL(queryResetAndClearBindings(UPDATE_APP_STATE_REQUEST_ID))
    }
    return true;
};
} // namespace KDC
