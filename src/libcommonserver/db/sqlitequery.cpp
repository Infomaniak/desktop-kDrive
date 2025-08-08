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

#include "sqlitequery.h"
#include "utility/utility.h"
#include "utility/logiffail.h"
#include "log/log.h"

#include <log4cplus/loggingmacros.h>

#include <sqlite3.h>

#include <iostream>
#include <vector>

#define SQLITE_SLEEP_TIME_MSEC 100
#define SQLITE_REPEAT_COUNT 20

namespace KDC {

SqliteQuery::SqliteQuery() :
    _logger(Log::instance()->getLogger()) {}

SqliteQuery::SqliteQuery(std::shared_ptr<sqlite3> sqlite3Db) :
    _logger(Log::instance()->getLogger()),
    _sqlite3Db(sqlite3Db) {}

int SqliteQuery::prepare(const std::string &sql, bool allow_failure) {
    _sql = CommonUtility::trim(sql);
    if (_stmt) {
        sqlite3_finalize(_stmt.get());
    }
    if (!_sql.empty()) {
        int n = 0;
        int rc;
        do {
            sqlite3_stmt *stmt;
            rc = sqlite3_prepare_v2(_sqlite3Db.get(), _sql.c_str(), -1, &stmt, 0);
            _stmt.reset(stmt, sqlite3_finalize);

            if ((rc == SQLITE_BUSY) || (rc == SQLITE_LOCKED)) {
                n++;
                Utility::msleep(SQLITE_SLEEP_TIME_MSEC);
            }
        } while ((n < SQLITE_REPEAT_COUNT) && ((rc == SQLITE_BUSY) || (rc == SQLITE_LOCKED)));
        _errId = rc;

        if (_errId != SQLITE_OK) {
            _error = std::string(sqlite3_errmsg(_sqlite3Db.get()));
            LOG_WARN(_logger, "Sqlite prepare statement error: " << _error << " in " << _sql);
            LOG_MSG_IF_FAIL(allow_failure, "SQLITE Prepare error");
        } else {
            LOG_IF_FAIL(_stmt.get());
        }
    }
    return _errId;
}

void SqliteQuery::resetAndClearBindings() {
    if (_stmt) {
        SQLITE_DO(sqlite3_reset(_stmt.get()));
        SQLITE_DO(sqlite3_clear_bindings(_stmt.get()));
    }
}

bool SqliteQuery::bindValue(int index, const dbtype &value) {
    // LOG_DEBUG(_logger, "SQL bind: " << index << " - " << Utility::v2ws(value));

    int res = -1;
    if (!_stmt) {
        return false;
    }

    if (std::holds_alternative<int>(value)) {
        res = sqlite3_bind_int(_stmt.get(), index, std::get<int>(value));
    } else if (std::holds_alternative<int64_t>(value)) {
        res = sqlite3_bind_int64(_stmt.get(), index, std::get<int64_t>(value));
    } else if (std::holds_alternative<double>(value)) {
        res = sqlite3_bind_double(_stmt.get(), index, std::get<double>(value));
    } else if (std::holds_alternative<std::string>(value)) {
        res = sqlite3_bind_text(_stmt.get(), index, std::get<std::string>(value).c_str(), -1, SQLITE_TRANSIENT);
    } else if (std::holds_alternative<std::wstring>(value)) {
        res = sqlite3_bind_text16(_stmt.get(), index, std::get<std::wstring>(value).c_str(), -1, SQLITE_TRANSIENT);
    } else if (std::holds_alternative<std::shared_ptr<std::vector<char>>>(value)) {
        std::shared_ptr<std::vector<char>> valuePtr = std::get<std::shared_ptr<std::vector<char>>>(value);
        if (valuePtr) {
            char *buffer = (char *) malloc(valuePtr->size() * sizeof(char));
            std::uninitialized_copy(valuePtr->begin(), valuePtr->end(), buffer);
            res = sqlite3_bind_blob(_stmt.get(), index, buffer, static_cast<int>(valuePtr->size()), SQLITE_TRANSIENT);
            free(buffer);
        } else {
            // Do nothing
            res = SQLITE_OK;
        }
    } else if (std::holds_alternative<std::monostate>(value)) {
        res = sqlite3_bind_null(_stmt.get(), index);
    }

    if (res != SQLITE_OK) {
        LOGW_WARN(_logger, L"ERROR binding SQL value: " << Utility::v2ws(value) << L" error: " << res);
        return false;
    }

    return true;
}

bool SqliteQuery::exec() {
    if (isSelect() || isPragma()) {
        return false;
    }

    if (!_stmt) {
        LOG_WARN(_logger, "Can't exec query, statement unprepared.");
        return false;
    }

    int rc, n = 0;
    do {
        rc = sqlite3_step(_stmt.get());
        if (rc == SQLITE_LOCKED) {
            rc = sqlite3_reset(_stmt.get()); /* This will also return SQLITE_LOCKED */
            n++;
            Utility::msleep(SQLITE_SLEEP_TIME_MSEC);
        } else if (rc == SQLITE_BUSY) {
            Utility::msleep(SQLITE_SLEEP_TIME_MSEC);
            n++;
        }
    } while ((n < SQLITE_REPEAT_COUNT) && ((rc == SQLITE_BUSY) || (rc == SQLITE_LOCKED)));
    _errId = rc;

    if (_errId != SQLITE_DONE && _errId != SQLITE_ROW) {
        _error = sqlite3_errmsg(_sqlite3Db.get());
        LOG_WARN(_logger, "Sqlite exec statement error: " << _errId << " - " << _error << " in " << _sql);
        if (_errId == SQLITE_IOERR) {
            LOG_WARN(_logger, "IOERR extended errcode: " << sqlite3_extended_errcode(_sqlite3Db.get()));
#if SQLITE_VERSION_NUMBER >= 3012000
            LOG_WARN(_logger, "IOERR system errno: " << sqlite3_system_errno(_sqlite3Db.get()));
#endif
        }
    } else {
        // LOG_DEBUG(_logger, "Last exec affected " << sqlite3_changes(_sqlite3Db.get()) << " rows.");
    }
    return (_errId == SQLITE_DONE); // either SQLITE_ROW or SQLITE_DONE
}

// !!! Not thread safe - To use with lock_guard
bool SqliteQuery::execAndGetRowId(int64_t &rowId) {
    if (!isInsert()) {
        return false;
    }

    const auto ret = exec();
    rowId = sqlite3_last_insert_rowid(_sqlite3Db.get());
    return ret;
}

SqliteQuery::NextResult KDC::SqliteQuery::next() {
    const bool firstStep = !sqlite3_stmt_busy(_stmt.get());

    int n = 0;
    for (;;) {
        _errId = sqlite3_step(_stmt.get());
        if (n < SQLITE_REPEAT_COUNT && firstStep && (_errId == SQLITE_LOCKED || _errId == SQLITE_BUSY)) {
            sqlite3_reset(_stmt.get()); // not necessary after sqlite version 3.6.23.1
            n++;
            Utility::msleep(SQLITE_SLEEP_TIME_MSEC);
        } else {
            break;
        }
    }

    NextResult result;
    result._ok = _errId == SQLITE_ROW || _errId == SQLITE_DONE;
    result._hasData = _errId == SQLITE_ROW;
    if (!result._ok) {
        _error = sqlite3_errmsg(_sqlite3Db.get());
        LOG_WARN(_logger, "Sqlite step statement error: " << _errId << " - " << _error << " in " << _sql);
    }

    return result;
}

bool SqliteQuery::nullValue(int index) const {
    return sqlite3_column_type(_stmt.get(), index) == SQLITE_NULL;
}

std::string SqliteQuery::stringValue(const int index) const {
#if defined(KD_WINDOWS)
    auto value = reinterpret_cast<const char *>(sqlite3_column_text(_stmt.get(), index));
    return value ? value : std::string();
#else
    const char *value = reinterpret_cast<const char *>(sqlite3_column_text(_stmt.get(), index));
    return value ? std::string(value) : std::string();
#endif
}

SyncName SqliteQuery::syncNameValue(int index) const {
#if defined(KD_WINDOWS)
    auto value = static_cast<const wchar_t *>(sqlite3_column_text16(_stmt.get(), index));
    return value ? value : SyncName();
#else
    const char *value = reinterpret_cast<const char *>(sqlite3_column_text(_stmt.get(), index));
    return value ? SyncName(value) : SyncName();
#endif
}

int SqliteQuery::intValue(int index) const {
    return sqlite3_column_int(_stmt.get(), index);
}

int64_t SqliteQuery::int64Value(int index) const {
    return sqlite3_column_int64(_stmt.get(), index);
}

double SqliteQuery::doubleValue(int index) const {
    return sqlite3_column_double(_stmt.get(), index);
}

const void *SqliteQuery::blobValue(int index) const {
    return sqlite3_column_blob(_stmt.get(), index);
}

int SqliteQuery::blobSize(int index) const {
    return sqlite3_column_bytes(_stmt.get(), index);
}

bool SqliteQuery::isSelect() const {
    return CommonUtility::startsWithInsensitive(_sql, "SELECT");
}

bool SqliteQuery::isInsert() const {
    return CommonUtility::startsWithInsensitive(_sql, "INSERT");
}

bool SqliteQuery::isPragma() const {
    return CommonUtility::startsWithInsensitive(_sql, "PRAGMA");
}

} // namespace KDC
