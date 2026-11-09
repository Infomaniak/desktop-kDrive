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

#include "tccdatabase_mac.h"

#include <memory>

#include <sqlite3.h>

namespace KDC {

std::optional<int32_t> TccDatabase::authValue(const std::string &service, const std::string &client, std::string &errorDescr) {
    // Without SQLITE_OPEN_READONLY, SQLite still falls back to read-only access when the file is not writable.
    sqlite3 *rawDb = nullptr;
    const auto openResult =
            sqlite3_open_v2("/Library/Application Support/com.apple.TCC/TCC.db", &rawDb, SQLITE_OPEN_READWRITE, nullptr);
    const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(rawDb, &sqlite3_close);
    if (openResult != SQLITE_OK) {
        errorDescr = "Cannot open TCC database!";
        return std::nullopt;
    }

    // The system TCC daemon can briefly lock the database.
    static constexpr int32_t busyTimeoutMs = 5000;
    (void) sqlite3_busy_timeout(db.get(), busyTimeoutMs);

    const auto *sql = "SELECT auth_value FROM access WHERE service = ?1 AND client = ?2 AND client_type = 0";
    sqlite3_stmt *rawStatement = nullptr;
    const auto prepareResult = sqlite3_prepare_v2(db.get(), sql, -1, &rawStatement, nullptr);
    const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(rawStatement, &sqlite3_finalize);
    if (prepareResult != SQLITE_OK || sqlite3_bind_text(statement.get(), 1, service.c_str(), -1, SQLITE_STATIC) != SQLITE_OK ||
        sqlite3_bind_text(statement.get(), 2, client.c_str(), -1, SQLITE_STATIC) != SQLITE_OK ||
        sqlite3_step(statement.get()) != SQLITE_ROW) {
        errorDescr = "Unable to find the TCC authorization of " + client + " for " + service + "!";
        return std::nullopt;
    }

    return sqlite3_column_int(statement.get(), 0);
}

} // namespace KDC
