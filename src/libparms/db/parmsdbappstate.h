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

#define CREATE_APP_STATE_TABLE_ID "create_app_state"
#define CREATE_APP_STATE_TABLE              \
    "CREATE TABLE IF NOT EXISTS app_state(" \
    "key INTEGER PRIMARY KEY,"              \
    "value TEXT);"

#define INSERT_APP_STATE_REQUEST_ID "insert_app_state"
#define INSERT_APP_STATE_REQUEST          \
    "INSERT INTO app_state (key, value) " \
    "VALUES (?1, ?2);"

#define SELECT_APP_STATE_REQUEST_ID "select_value_from_key"
#define SELECT_APP_STATE_REQUEST "SELECT value FROM app_state WHERE key=?1;"

#define UPDATE_APP_STATE_REQUEST_ID "update_value_with_key"
#define UPDATE_APP_STATE_REQUEST "UPDATE app_state SET value=?2 WHERE key=?1;"


#define APP_STATE_KEY_DEFAULT_AppStateKeyTest "Test"