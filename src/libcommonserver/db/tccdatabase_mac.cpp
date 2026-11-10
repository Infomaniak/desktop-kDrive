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

#include "log/log.h"

#include <array>
#include <cerrno>
#include <cstring>
#include <iomanip>
#include <memory>
#include <sstream>
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>

#include <log4cplus/loggingmacros.h>
#include <sqlite3.h>

namespace KDC {

namespace {

constexpr auto tccDbPath = "/Library/Application Support/com.apple.TCC/TCC.db";

std::string sqliteError(sqlite3 *db, const int32_t result) {
    std::ostringstream description;
    description << sqlite3_errstr(result) << " (code " << result;
    if (db) {
        description << ", extended code " << sqlite3_extended_errcode(db) << ", errno " << sqlite3_system_errno(db) << ": "
                    << sqlite3_errmsg(db);
    }
    description << ")";
    return description.str();
}

std::string ownerName(const uid_t uid) {
    struct passwd entry{};
    struct passwd *found = nullptr;
    std::array<char, 4096> buffer{};
    if (getpwuid_r(uid, &entry, buffer.data(), buffer.size(), &found) != 0 || !found) {
        return "unknown";
    }
    return found->pw_name;
}

std::string groupName(const gid_t gid) {
    struct group entry{};
    struct group *found = nullptr;
    std::array<char, 4096> buffer{};
    if (getgrgid_r(gid, &entry, buffer.data(), buffer.size(), &found) != 0 || !found) {
        return "unknown";
    }
    return found->gr_name;
}

std::string fileDescription(const char *path) {
    struct stat fileStat{};
    if (stat(path, &fileStat) != 0) {
        const int32_t error = errno;
        return "unavailable, " + std::string(std::strerror(error)) + " (errno " + std::to_string(error) + ")";
    }

    std::tm modificationTime{};
    (void) gmtime_r(&fileStat.st_mtime, &modificationTime);

    std::ostringstream description;
    description << "size " << fileStat.st_size << ", mode " << std::oct << (fileStat.st_mode & 07777) << std::dec << ", uid "
                << fileStat.st_uid << " (" << ownerName(fileStat.st_uid) << "), gid " << fileStat.st_gid << " ("
                << groupName(fileStat.st_gid) << "), modified " << std::put_time(&modificationTime, "%Y-%m-%d %H:%M:%S UTC");
    return description.str();
}

std::optional<int32_t> readAuthValue(sqlite3 *db, const std::string &service, const std::string &client, std::string &outcome) {
    sqlite3_stmt *rawStatement = nullptr;
    int32_t result =
            sqlite3_prepare_v2(db, "SELECT auth_value FROM access WHERE service = ?1 AND client = ?2 AND client_type = 0", -1,
                               &rawStatement, nullptr);
    const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(rawStatement, &sqlite3_finalize);
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement.get(), 1, service.c_str(), -1, SQLITE_STATIC);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement.get(), 2, client.c_str(), -1, SQLITE_STATIC);
    }
    if (result == SQLITE_OK) {
        result = sqlite3_step(statement.get());
    }

    switch (result) {
        case SQLITE_ROW: {
            const int32_t value = sqlite3_column_int(statement.get(), 0);
            outcome = "auth_value " + std::to_string(value);
            return value;
        }
        case SQLITE_DONE:
            outcome = "no matching row";
            return std::nullopt;
        default:
            outcome = "query failed, " + sqliteError(db, result);
            return std::nullopt;
    }
}

} // namespace

std::optional<int32_t> TccDatabase::authValue(const std::string &service, const std::string &client, std::string &errorDescr) {
    // Without SQLITE_OPEN_READONLY, SQLite still falls back to read-only access when the file is not writable.
    sqlite3 *rawDb = nullptr;
    const int32_t openResult = sqlite3_open_v2(tccDbPath, &rawDb, SQLITE_OPEN_READWRITE, nullptr);
    const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(rawDb, &sqlite3_close);

    std::optional<int32_t> value;
    std::string outcome;
    if (openResult != SQLITE_OK) {
        errorDescr = "Cannot open TCC database!";
        outcome = "open failed, " + sqliteError(db.get(), openResult);
    } else {
        // The system TCC daemon can briefly lock the database.
        static constexpr int32_t busyTimeoutMs = 5000;
        (void) sqlite3_busy_timeout(db.get(), busyTimeoutMs);

        value = readAuthValue(db.get(), service, client, outcome);
        if (!value.has_value()) {
            errorDescr = "Unable to find the TCC authorization of " + client + " for " + service + "!";
        }
    }

    LOG_INFO(Log::instance()->getLogger(), "TCC authorization of " << client << " for " << service << ": " << outcome << " | "
                                                                   << tccDbPath << ": " << fileDescription(tccDbPath));
    return value;
}

} // namespace KDC
