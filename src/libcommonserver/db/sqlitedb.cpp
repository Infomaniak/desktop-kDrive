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

#include "sqlitedb.h"
#include "utility/utility.h"
#include "utility/logiffail.h"
#include "log/log.h"

#include <log4cplus/loggingmacros.h>

#include <sqlite3.h>

#include <filesystem>

#define PRAGMA_QUICK_CHECK_ID "sqlitedb1"
#define PRAGMA_QUICK_CHECK "PRAGMA quick_check;"

namespace KDC {

SqliteDb::SqliteDb() :
    _logger(Log::instance()->getLogger()),
    _sqlite3Db(nullptr),
    _errId(0),
    _autoDelete(false) {}

SqliteDb::~SqliteDb() {}

bool SqliteDb::isOpened() const {
    return _sqlite3Db != nullptr;
}

bool SqliteDb::openOrCreateReadWrite(const std::filesystem::path &dbPath) {
    if (isOpened()) {
        return true;
    }

    if (!openHelper(dbPath, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)) {
        return false;
    }

    auto checkResult = checkDb();
    if (checkResult != CheckDbResult::Ok) {
        if (checkResult == CheckDbResult::CantPrepare) {
            // When disk space is low, preparing may fail even though the db is fine.
            // Typically CANTOPEN or IOERR.
            int64_t freeSpace = Utility::getFreeDiskSpace(dbPath);
            if (freeSpace != -1 && freeSpace < 1000000) {
                LOG_WARN(_logger, "Can't prepare consistency check and disk space is low: " << freeSpace);
                close();
                return false;
            }

            // Even when there's enough disk space, it might very well be that the
            // file is on a read-only filesystem and can't be opened because of that.
            if (_errId == SQLITE_CANTOPEN) {
                LOG_WARN(_logger, "Can't open db to prepare consistency check, aborting");
                close();
                return false;
            }
        }

        LOGW_FATAL(_logger, L"Consistency check failed, removing broken db " << Path2WStr(dbPath));
        close();
        std::error_code ec;
        if (!std::filesystem::remove(dbPath, ec)) {
            if (ec.value() != 0) {
                LOGW_WARN(_logger, L"Failed to remove db file " << Utility::formatStdError(dbPath, ec));
                close();
                return false;
            }

            LOG_WARN(_logger, "Failed to remove db file");
            close();
            return false;
        }

        return openHelper(dbPath, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    }

    return true;
}

bool SqliteDb::openReadOnly(const std::filesystem::path &dbPath) {
    if (isOpened()) {
        return true;
    }

    if (!openHelper(dbPath, SQLITE_OPEN_READONLY)) {
        return false;
    }

    if (checkDb() != CheckDbResult::Ok) {
        LOGW_WARN(_logger, L"Consistency check failed in readonly mode, giving up " << Path2WStr(dbPath));
        close();
        return false;
    }

    return true;
}

bool SqliteDb::startTransaction() {
    if (!isOpened()) {
        return false;
    }
    SQLITE_DO(sqlite3_exec(_sqlite3Db.get(), "BEGIN", 0, 0, 0));
    return _errId == SQLITE_OK;
}

bool SqliteDb::commit() {
    if (!isOpened()) {
        return false;
    }
    SQLITE_DO(sqlite3_exec(_sqlite3Db.get(), "COMMIT", 0, 0, 0));
    return _errId == SQLITE_OK;
}

bool SqliteDb::rollback() {
    if (!isOpened()) {
        return false;
    }
    SQLITE_DO(sqlite3_exec(_sqlite3Db.get(), "ROLLBACK", 0, 0, 0));
    return _errId == SQLITE_OK;
}

void SqliteDb::close() {
    if (!isOpened()) {
        return;
    }

    _queries.clear();

    // sqlite3_close is done by the sqlite3 destructor
    _sqlite3Db.reset();

    if (_autoDelete) {
        // Delete DB
        std::error_code ec;
        if (!std::filesystem::remove(_dbPath, ec)) {
            if (ec) {
                LOGW_WARN(_logger, L"Failed to check if  " << Utility::formatSyncPath(_dbPath) << L" exists: "
                                                           << Utility::formatStdError(ec));
            }

            LOG_WARN(_logger, "Failed to remove db file.");
        }
    }
}

bool SqliteDb::queryCreate(const std::string &id) {
    if (_queries.find(id) != _queries.end()) {
        LOG_WARN(_logger, "Query ID " << id << " already exist.");
        return false;
    }

    _queries[id] = {std::shared_ptr<SqliteQuery>(new SqliteQuery(_sqlite3Db)), false, false, {false, false}};

    return true;
}

bool SqliteDb::queryPrepare(const std::string &id, const std::string sql, bool allow_failure, int &errId, std::string &error) {
    if (_queries.find(id) != _queries.end()) {
        QueryInfo &queryInfo = _queries.at(id);
        if (queryInfo._isPrepared) {
            queryInfo._query->resetAndClearBindings();
            queryInfo._isExecuted = false;
            queryInfo._result._ok = false;
            queryInfo._result._hasData = false;
        }
        queryInfo._isPrepared = (queryInfo._query->prepare(sql, allow_failure) == SQLITE_OK);
        errId = queryInfo._query->errorId();
        error = queryInfo._query->error();
        return queryInfo._isPrepared;
    }
    return false;
}

bool SqliteDb::queryResetAndClearBindings(const std::string &id) {
    if (_queries.find(id) != _queries.end()) {
        QueryInfo &queryInfo = _queries.at(id);
        queryInfo._query->resetAndClearBindings();
        queryInfo._isExecuted = false;
        queryInfo._result._ok = false;
        queryInfo._result._hasData = false;
        return true;
    }
    return false;
}

bool SqliteDb::queryBindValue(const std::string &id, int index, const dbtype &value) {
    if (_queries.find(id) != _queries.end()) {
        QueryInfo &queryInfo = _queries.at(id);
        return queryInfo._query->bindValue(index, value);
    }
    return false;
}

bool SqliteDb::queryExec(const std::string &id, int &errId, std::string &error) {
    if (_queries.find(id) != _queries.end()) {
        QueryInfo &queryInfo = _queries.at(id);
        if (queryInfo._isPrepared) {
            queryInfo._isExecuted = queryInfo._query->exec();
            errId = queryInfo._query->errorId();
            error = queryInfo._query->error();
            return queryInfo._isExecuted;
        }
    }
    return false;
}

bool SqliteDb::queryExecAndGetRowId(const std::string &id, int64_t &rowId, int &errId, std::string &error) {
    if (_queries.find(id) != _queries.end()) {
        QueryInfo &queryInfo = _queries.at(id);
        if (queryInfo._isPrepared) {
            queryInfo._isExecuted = queryInfo._query->execAndGetRowId(rowId);
            errId = queryInfo._query->errorId();
            error = queryInfo._query->error();
            return queryInfo._isExecuted;
        }
    }
    return false;
}

bool SqliteDb::queryNext(const std::string &id, bool &hasData) {
    if (_queries.find(id) != _queries.end()) {
        QueryInfo &queryInfo = _queries.at(id);
        if (queryInfo._isPrepared) {
            queryInfo._result = queryInfo._query->next();
            hasData = queryInfo._result._hasData;
            return queryInfo._result._ok;
        }
    }
    return false;
}

bool SqliteDb::queryIntValue(const std::string &id, int index, int &value) const {
    if (_queries.find(id) != _queries.end()) {
        const QueryInfo &queryInfo = _queries.at(id);
        if (queryInfo._result._hasData) {
            value = queryInfo._query->intValue(index);
            return true;
        }
    }
    return false;
}

bool SqliteDb::queryInt64Value(const std::string &id, int index, int64_t &value) const {
    if (_queries.find(id) != _queries.end()) {
        const QueryInfo &queryInfo = _queries.at(id);
        if (queryInfo._result._hasData) {
            value = queryInfo._query->int64Value(index);
            return true;
        }
    }
    return false;
}

bool SqliteDb::queryDoubleValue(const std::string &id, int index, double &value) const {
    if (_queries.find(id) != _queries.end()) {
        const QueryInfo &queryInfo = _queries.at(id);
        if (queryInfo._result._hasData) {
            value = queryInfo._query->doubleValue(index);
            return true;
        }
    }
    return false;
}

bool SqliteDb::queryStringValue(const std::string &id, int index, std::string &value) const {
    if (_queries.find(id) != _queries.end()) {
        const QueryInfo &queryInfo = _queries.at(id);
        if (queryInfo._result._hasData) {
            value = queryInfo._query->stringValue(index);
            return true;
        }
    }
    return false;
}

bool SqliteDb::querySyncNameValue(const std::string &id, int index, SyncName &value) const {
    if (_queries.find(id) != _queries.end()) {
        const QueryInfo &queryInfo = _queries.at(id);
        if (queryInfo._result._hasData) {
            value = queryInfo._query->syncNameValue(index);
            return true;
        }
    }
    return false;
}

bool SqliteDb::queryBlobValue(const std::string &id, int index, std::shared_ptr<std::vector<char>> &value) const {
    if (_queries.find(id) != _queries.end()) {
        const QueryInfo &queryInfo = _queries.at(id);
        if (queryInfo._result._hasData) {
            size_t blobSize = static_cast<size_t>(queryInfo._query->blobSize(index));
            if (blobSize) {
                const unsigned char *blob = (const unsigned char *) queryInfo._query->blobValue(index);
                value = std::shared_ptr<std::vector<char>>(new std::vector<char>(blob, blob + blobSize));
                if (!value) {
                    LOG_WARN(_logger, "Memory allocation error");
                    return false;
                }
            } else {
                value = nullptr;
            }
            return true;
        }
    }
    return false;
}

bool SqliteDb::queryIsNullValue(const std::string &id, int index, bool &ok) const {
    if (_queries.find(id) != _queries.end()) {
        const QueryInfo &queryInfo = _queries.at(id);
        if (queryInfo._result._hasData) {
            ok = queryInfo._query->nullValue(index);
            return true;
        }
    }
    return false;
}

void SqliteDb::queryFree(const std::string &id) {
    if (_queries.find(id) != _queries.end()) {
        _queries.erase(id);
    }
}

int SqliteDb::numRowsAffected() const {
    return sqlite3_changes(_sqlite3Db.get());
}

int SqliteDb::extendedErrorCode() const {
    return sqlite3_extended_errcode(_sqlite3Db.get());
}

bool SqliteDb::openHelper(const std::filesystem::path &dbPath, int sqliteFlags) {
    if (isOpened()) {
        return true;
    }

    _dbPath = dbPath;
    sqliteFlags |= SQLITE_OPEN_NOMUTEX;

    sqlite3 *db = nullptr;
    SQLITE_DO(sqlite3_open_v2(Path2Str(dbPath).c_str(), &db, sqliteFlags, 0));
    _sqlite3Db.reset(db, sqlite3_close);

    if (_errId != SQLITE_OK) {
        LOGW_WARN(_logger,
                  L"Opening database failed: " << _errId << L" " << Utility::s2ws(_error) << L" for " << Path2WStr(dbPath));
        if (_errId == SQLITE_CANTOPEN) {
            LOG_WARN(_logger, "CANTOPEN extended errcode: " << sqlite3_extended_errcode(_sqlite3Db.get()));
#if SQLITE_VERSION_NUMBER >= 3012000
            LOG_WARN(_logger, "CANTOPEN system errno: " << sqlite3_system_errno(_sqlite3Db.get()));
#endif
        }
        close();
        return false;
    }

    if (!_sqlite3Db) {
        LOGW_WARN(_logger, L"Error: no database for " << Path2WStr(dbPath));
        return false;
    }

    sqlite3_busy_timeout(_sqlite3Db.get(), 5000);

    return true;
}

SqliteDb::CheckDbResult SqliteDb::checkDb() {
    // quick_check can fail with a disk IO error when diskspace is low
    LOG_IF_FAIL(queryCreate(PRAGMA_QUICK_CHECK_ID));
    if (!queryPrepare(PRAGMA_QUICK_CHECK_ID, PRAGMA_QUICK_CHECK, true, _errId, _error)) {
        LOG_WARN(_logger, "Error preparing query: " << PRAGMA_QUICK_CHECK_ID);
        queryFree(PRAGMA_QUICK_CHECK_ID);
        return CheckDbResult::CantPrepare;
    }
    bool hasData;
    if (!queryNext(PRAGMA_QUICK_CHECK_ID, hasData) || !hasData) {
        LOG_WARN(_logger, "Error getting query result: " << PRAGMA_QUICK_CHECK_ID);
        queryFree(PRAGMA_QUICK_CHECK_ID);
        return CheckDbResult::NotOk;
    }
    std::string result;
    LOG_IF_FAIL(queryStringValue(PRAGMA_QUICK_CHECK_ID, 0, result));
    if (result != "ok") {
        LOG_WARN(_logger, "Bad query result: " << PRAGMA_QUICK_CHECK_ID << " - " << result);
        queryFree(PRAGMA_QUICK_CHECK_ID);
        return CheckDbResult::NotOk;
    }
    queryFree(PRAGMA_QUICK_CHECK_ID);

    return CheckDbResult::Ok;
}

namespace details {

SyncName makeSyncName(sqlite3_value *value) {
#if defined(KD_WINDOWS)
    auto wvalue = (wchar_t *) sqlite3_value_text16(value);
    return wvalue ? reinterpret_cast<const wchar_t *>(wvalue) : SyncName();
#else
    auto charValue = reinterpret_cast<const char *>(sqlite3_value_text(value));
    return charValue ? SyncName(charValue) : SyncName();
#endif
}

static void normalizeSyncName(sqlite3_context *context, int argc, sqlite3_value **argv) {
    if (argc == 1) {
        SyncName name(makeSyncName(argv[0]));
        SyncName normalizedName;
        if (!Utility::normalizedSyncName(name, normalizedName)) {
            // TODO: Is there a better solution?
            normalizedName = name;
        }

        if (!normalizedName.empty()) {
#if defined(KD_WINDOWS)
            sqlite3_result_text16(context, normalizedName.c_str(), -1, SQLITE_TRANSIENT);
#else
            sqlite3_result_text(context, normalizedName.c_str(), -1, SQLITE_TRANSIENT);
#endif
            return;
        }
    }
    sqlite3_result_null(context);
}
} // namespace details

int SqliteDb::createNormalizeSyncNameFunc() {
    return sqlite3_create_function(_sqlite3Db.get(), "normalizeSyncName", 1, SQLITE_UTF8, nullptr, &details::normalizeSyncName,
                                   nullptr, nullptr);
}

} // namespace KDC
