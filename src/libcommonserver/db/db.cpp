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

#include "db.h"
#include "utility/utility.h"
#include "utility/logiffail.h"
#include "log/log.h"
#include "db/sqlitedb.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/io/iohelper.h"

#include <sqlite3.h>

#include <filesystem>
#include <fstream>

#include <Poco/MD5Engine.h>

#define SELECT_SQLITE_VERSION_ID "db1"
#define SELECT_SQLITE_VERSION "SELECT sqlite_version();"

#define PRAGMA_LOCKING_MODE_ID "db2"
// #define PRAGMA_LOCKING_MODE             "PRAGMA locking_mode=EXCLUSIVE;"
#define PRAGMA_LOCKING_MODE "PRAGMA locking_mode=NORMAL;" // For debugging

#define PRAGMA_JOURNAL_MODE_ID "db3"
#define PRAGMA_JOURNAL_MODE "PRAGMA journal_mode="

#define PRAGMA_SYNCHRONOUS_ID "db4"
#define PRAGMA_SYNCHRONOUS "PRAGMA synchronous="

#define PRAGMA_CASE_SENSITIVE_LIKE_ID "db5"
#define PRAGMA_CASE_SENSITIVE_LIKE "PRAGMA case_sensitive_like=ON;"

#define PRAGMA_FOREIGN_KEYS_ID "db6"
#define PRAGMA_FOREIGN_KEYS "PRAGMA foreign_keys=ON;"

//
// version
//
#define CREATE_VERSION_TABLE_ID "create_version"
#define CREATE_VERSION_TABLE              \
    "CREATE TABLE IF NOT EXISTS version(" \
    "value TEXT);"

#define INSERT_VERSION_REQUEST_ID "insert_version"
#define INSERT_VERSION_REQUEST     \
    "INSERT INTO version (value) " \
    "VALUES (?1);"

#define UPDATE_VERSION_REQUEST_ID "update_version"
#define UPDATE_VERSION_REQUEST "UPDATE version SET value=?1;"

#define SELECT_VERSION_REQUEST_ID "select_version"
#define SELECT_VERSION_REQUEST \
    "SELECT value "            \
    "FROM version;"

// Item existence
// Check if a table exists
#define CHECK_TABLE_EXISTENCE_REQUEST_ID "check_table_existence"
#define CHECK_TABLE_EXISTENCE_REQUEST "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?1;"

#define CHECK_COLUMN_EXISTENCE_REQUEST_ID "check_column_existence"
#define CHECK_COLUMN_EXISTENCE_REQUEST "SELECT COUNT(*) AS CNTREC FROM pragma_table_info(?1) WHERE name=?2;"

namespace KDC {

static std::string defaultJournalMode(const std::string &dbPath) {
#if defined(KD_MACOS)
    if (Utility::startsWith(dbPath, "/Volumes/")) {
        return "DELETE";
    }
#elif defined(KD_WINDOWS)
    std::string fsName = Utility::fileSystemName(dbPath);
    if (fsName.find("FAT") != std::string::npos) {
        return "DELETE";
    }
#else
    (void) dbPath;
#endif

    return "WAL";
}

Db::Db(const std::filesystem::path &dbPath) :
    _logger(Log::instance()->getLogger()),
    _sqliteDb(new SqliteDb()),
    _dbPath(dbPath),
    _transaction(false),
    _journalMode(defaultJournalMode(dbPath.string())),
    _fromVersion(std::string()) {}

Db::~Db() {
    close();
}

std::string Db::makeDbFileName(int userId, int accountId, int driveId, int syncDbId) {
    std::string fileName;
    if (!userId && !accountId && !driveId && !syncDbId) {
        // ParmsDb
        (void) fileName.append(".parms");
    } else {
        // SyncDb
        (void) fileName.append(".sync_");

        std::string key(std::to_string(userId));
        key.append(":");
        key.append(std::to_string(accountId));
        key.append(":");
        key.append(std::to_string(driveId));
        key.append(":");
        key.append(std::to_string(syncDbId));
        Poco::MD5Engine md5;
        md5.update(key.c_str());
        const std::string keyHash = Poco::DigestEngine::digestToHex(md5.digest()).substr(0, 6);
        std::string keyHashHex;
        Utility::str2hexstr(keyHash, keyHashHex);
        (void) fileName.append(keyHashHex);
    }
    return fileName;
}

std::filesystem::path Db::makeDbName(bool &alreadyExist) {
    return makeDbName(0, 0, 0, 0, alreadyExist);
}

std::filesystem::path Db::makeDbName(int userId, int accountId, int driveId, int syncDbId, bool &alreadyExist,
                                     std::function<std::string(int, int, int, int)> dbFileName) {
    // App support dir
    std::filesystem::path dbPath(CommonUtility::getAppSupportDir());

    bool exists = false;
    IoError ioError = IoError::Success;

    if (!IoHelper::checkIfPathExists(dbPath, exists, ioError)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(dbPath, ioError));
        return std::filesystem::path();
    }

    if (!exists) {
        if (!IoHelper::createDirectory(dbPath, false, ioError)) {
            LOGW_WARN(Log::instance()->getLogger(), L"Failed to create directory: " << Utility::formatIoError(dbPath, ioError));
            return std::filesystem::path();
        }
    }

    // Db file name
    std::string fileName = dbFileName(userId, accountId, driveId, syncDbId);
    (void) fileName.append(".db");

    // Db file path
    (void) dbPath.append(fileName);

    // If it exists already, the path is clearly usable
    if (!IoHelper::checkIfPathExists(dbPath, exists, ioError)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(dbPath, ioError));
        return std::filesystem::path();
    }

    if (!exists) {
        // Try to create a file there
        std::ofstream dbFileStream(dbPath.native());
        if (dbFileStream.is_open()) {
            // Ok, all good.
            dbFileStream.close();
            std::filesystem::remove(dbPath);

            alreadyExist = false;
            return dbPath;
        }
    } else {
        alreadyExist = true;
        return dbPath;
    }

    return std::filesystem::path();
}

bool Db::exists() {
    const std::scoped_lock lock(_mutex);

    if (_dbPath.empty()) {
        return false;
    } else {
        bool exists = false;
        IoError ioError = IoError::Success;
        if (!IoHelper::checkIfPathExists(_dbPath, exists, ioError)) {
            LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_dbPath, ioError));
            return false;
        }

        if (!exists) {
            return false;
        }
    }

    return true;
}

std::filesystem::path Db::dbPath() const {
    return _dbPath;
}

void Db::close() {
    if (!_sqliteDb || !_sqliteDb->isOpened()) {
        return;
    }

    const std::scoped_lock lock(_mutex);

    LOGW_DEBUG(_logger, L"Closing DB " << Path2WStr(_dbPath));

    commitTransaction();
    _sqliteDb->close();
}

bool Db::queryCreate(const std::string &id) {
    return _sqliteDb->queryCreate(id);
}

bool Db::queryPrepare(const std::string &id, const std::string sql, bool allow_failure, int &errId, std::string &error) {
    return _sqliteDb->queryPrepare(id, sql, allow_failure, errId, error);
}

bool Db::queryResetAndClearBindings(const std::string &id) {
    return _sqliteDb->queryResetAndClearBindings(id);
}

bool Db::queryBindValue(const std::string &id, int index, const dbtype &value) {
    return _sqliteDb->queryBindValue(id, index, value);
}

bool Db::queryExec(const std::string &id, int &errId, std::string &error) {
    bool ret = _sqliteDb->queryExec(id, errId, error);
    LOG_IF_FAIL(_sqliteDb->queryResetAndClearBindings(id));
    return ret;
}

bool Db::queryExecAndGetRowId(const std::string &id, int64_t &rowId, int &errId, std::string &error) {
    bool ret = _sqliteDb->queryExecAndGetRowId(id, rowId, errId, error);
    LOG_IF_FAIL(_sqliteDb->queryResetAndClearBindings(id));
    return ret;
}

bool Db::queryNext(const std::string &id, bool &hasData) {
    bool ret = _sqliteDb->queryNext(id, hasData);
    if (!ret || !hasData) {
        LOG_IF_FAIL(_sqliteDb->queryResetAndClearBindings(id));
    }
    return ret;
}

bool Db::queryIntValue(const std::string &id, int index, int &value) const {
    return _sqliteDb->queryIntValue(id, index, value);
}

bool Db::queryInt64Value(const std::string &id, int index, int64_t &value) const {
    return _sqliteDb->queryInt64Value(id, index, value);
}

bool Db::queryDoubleValue(const std::string &id, int index, double &value) const {
    return _sqliteDb->queryDoubleValue(id, index, value);
}

bool Db::queryStringValue(const std::string &id, int index, std::string &value) const {
    return _sqliteDb->queryStringValue(id, index, value);
}

bool Db::querySyncNameValue(const std::string &id, int index, SyncName &value) const {
    return _sqliteDb->querySyncNameValue(id, index, value);
}

bool Db::queryBlobValue(const std::string &id, int index, std::shared_ptr<std::vector<char>> &value) const {
    return _sqliteDb->queryBlobValue(id, index, value);
}

bool Db::queryIsNullValue(const std::string &id, int index, bool &ok) {
    return _sqliteDb->queryIsNullValue(id, index, ok);
}

void Db::queryFree(const std::string &id) {
    return _sqliteDb->queryFree(id);
}

int Db::numRowsAffected() const {
    return _sqliteDb->numRowsAffected();
}

int Db::extendedErrorCode() const {
    return _sqliteDb->extendedErrorCode();
}

bool Db::init(const std::string &version) {
#ifndef SQLITE_IOERR_SHMMAP
#define SQLITE_IOERR_SHMMAP (SQLITE_IOERR | (21 << 8))
#endif

    if (!createAndPrepareRequest(CHECK_TABLE_EXISTENCE_REQUEST_ID, CHECK_TABLE_EXISTENCE_REQUEST)) return false;
    if (!createAndPrepareRequest(CHECK_COLUMN_EXISTENCE_REQUEST_ID, CHECK_COLUMN_EXISTENCE_REQUEST)) return false;

    if (!version.empty()) {
        // Check if DB is already initialized
        bool dbExists = false;
        if (!tableExists("version", dbExists)) {
            return false;
        }

        if (dbExists) {
            // Check version
            LOG_DEBUG(_logger, "Check DB version");
            if (!createAndPrepareRequest(SELECT_VERSION_REQUEST_ID, SELECT_VERSION_REQUEST)) return false;

            bool found = false;
            if (!selectVersion(_fromVersion, found)) {
                LOG_WARN(_logger, "Error in Db::selectVersion");
                return false;
            }
            if (!found) {
                LOG_WARN(_logger, "Version not found");
                return false;
            }

            queryFree(SELECT_VERSION_REQUEST_ID);

            // Upgrade DB
            if (!upgrade(_fromVersion, version)) {
                LOG_WARN(_logger, "Error in Db::upgrade");
                return false;
            }

            // Update version
            if (!createAndPrepareRequest(UPDATE_VERSION_REQUEST_ID, UPDATE_VERSION_REQUEST)) return false;
            if (!updateVersion(version, found)) {
                LOG_WARN(_logger, "Error in Db::updateVersion");
                return false;
            }
            if (!found) {
                LOG_WARN(_logger, "Version not found");
                return false;
            }

            queryFree(UPDATE_VERSION_REQUEST_ID);
        } else {
            // Create version table
            LOG_DEBUG(_logger, "Create version table");
            if (!createAndPrepareRequest(CREATE_VERSION_TABLE_ID, CREATE_VERSION_TABLE)) return false;

            int errId = -1;
            if (std::string error; !queryExec(CREATE_VERSION_TABLE_ID, errId, error)) {
                queryFree(CREATE_VERSION_TABLE_ID);
                return sqlFail(CREATE_VERSION_TABLE_ID, error);
            }
            queryFree(CREATE_VERSION_TABLE_ID);

            // Insert version
            LOG_DEBUG(_logger, "Insert version " << version);
            if (!createAndPrepareRequest(INSERT_VERSION_REQUEST_ID, INSERT_VERSION_REQUEST)) return false;
            if (!insertVersion(version)) {
                LOG_WARN(_logger, "Error in Db::insertVersion");
                return false;
            }

            queryFree(INSERT_VERSION_REQUEST_ID);

            // Create DB
            LOG_INFO(_logger, "Create " << dbType() << " DB");
            if (bool retry = false; !create(retry)) {
                if (retry) {
                    LOG_WARN(_logger, "Error in Db::create - Retry");
                    _sqliteDb->close();
                    return checkConnect(version);
                } else {
                    LOG_WARN(_logger, "Error in Db::create");
                    return false;
                }
            }
        }
    }

    // Prepare DB
    LOG_INFO(_logger, "Prepare " << dbType() << " DB");
    if (!prepare()) {
        LOG_WARN(_logger, "Error in Db::prepare");
        return false;
    }

    return true;
}

void Db::startTransaction() {
    if (!_transaction) {
        if (!_sqliteDb->startTransaction()) {
            LOG_WARN(_logger, "ERROR starting transaction: " << _sqliteDb->error());
            return;
        }
        _transaction = true;
    } else {
        LOG_DEBUG(_logger, "Database Transaction is running, not starting another one!");
    }
}

void Db::commitTransaction() {
    if (_transaction) {
        if (!_sqliteDb->commit()) {
            LOG_WARN(_logger, "ERROR committing to the database: " << _sqliteDb->error());
            return;
        }
        _transaction = false;
    } else {
        LOG_DEBUG(_logger, "No database Transaction to commit");
    }
}

void Db::rollbackTransaction() {
    if (_transaction) {
        if (!_sqliteDb->rollback()) {
            LOG_WARN(_logger, "ERROR rollbacking to the database: " << _sqliteDb->error());
            return;
        }
        _transaction = false;
    } else {
        LOG_DEBUG(_logger, "No database Transaction to rollback");
    }
}

bool Db::sqlFail(const std::string &log, const std::string &error) {
    commitTransaction();
    LOG_WARN(_logger, "SQL Error - " << log << " - " << error);
    _sqliteDb->close();
    LOG_IF_FAIL(false);
    return false;
}

bool Db::checkConnect(const std::string &version) {
    (void) version;

    if (_sqliteDb && _sqliteDb->isOpened()) {
        // Unfortunately the sqlite isOpen check can return true even when the underlying storage
        // has become unavailable - and then some operations may cause crashes.
        bool exists = false;
        IoError ioError = IoError::Success;
        if (!IoHelper::checkIfPathExists(_dbPath, exists, ioError)) {
            LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_dbPath, ioError));
            close();
            return false;
        }

        if (!exists) {
            LOGW_WARN(_logger, L"Database is opened, but file " << Path2WStr(_dbPath) << L" does not exist");
            close();
            return false;
        }

        return true;
    }

    if (_dbPath.empty()) {
        LOGW_WARN(_logger, L"Database filename" << Path2WStr(_dbPath) << L" is empty");
        return false;
    }

    // The database file is created by this call (SQLITE_OPEN_CREATE)
    if (!_sqliteDb->openOrCreateReadWrite(_dbPath)) {
        std::string error = _sqliteDb->error();
        LOG_WARN(_logger, "Error opening the db: " << error);
        return false;
    }

    bool exists = false;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(_dbPath, exists, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists for path=" << Utility::formatIoError(_dbPath, ioError));
        return false;
    }

    if (!exists) {
        LOGW_WARN(_logger, L"Database file" << Path2WStr(_dbPath) << L" does not exist");
        return false;
    }

    // SELECT_SQLITE_VERSION
    if (!createAndPrepareRequest(SELECT_SQLITE_VERSION_ID, SELECT_SQLITE_VERSION)) return false;
    bool hasData;
    if (!queryNext(SELECT_SQLITE_VERSION_ID, hasData) || !hasData) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_SQLITE_VERSION_ID);
        queryFree(SELECT_SQLITE_VERSION_ID);
        return false;
    }
    std::string result;
    LOG_IF_FAIL(queryStringValue(SELECT_SQLITE_VERSION_ID, 0, result));
    queryFree(SELECT_SQLITE_VERSION_ID);
    LOG_DEBUG(_logger, "sqlite3 version=" << result);

    // PRAGMA_LOCKING_MODE
    if (!createAndPrepareRequest(PRAGMA_LOCKING_MODE_ID, PRAGMA_LOCKING_MODE)) return false;
    if (!queryNext(PRAGMA_LOCKING_MODE_ID, hasData) || !hasData) {
        LOG_WARN(_logger, "Error getting query result: " << PRAGMA_LOCKING_MODE_ID);
        queryFree(PRAGMA_LOCKING_MODE_ID);
        return false;
    }
    LOG_IF_FAIL(queryStringValue(PRAGMA_LOCKING_MODE_ID, 0, result));
    queryFree(PRAGMA_LOCKING_MODE_ID);
    LOG_DEBUG(_logger, "sqlite3 locking_mode=" << result);

    // PRAGMA_JOURNAL_MODE
    std::string sql(PRAGMA_JOURNAL_MODE + _journalMode + ";");
    if (!createAndPrepareRequest(PRAGMA_JOURNAL_MODE_ID, sql.c_str())) return false;
    if (!queryNext(PRAGMA_JOURNAL_MODE_ID, hasData) || !hasData) {
        LOG_WARN(_logger, "Error getting query result: " << PRAGMA_JOURNAL_MODE_ID);
        queryFree(PRAGMA_JOURNAL_MODE_ID);
        return false;
    }
    LOG_IF_FAIL(queryStringValue(PRAGMA_JOURNAL_MODE_ID, 0, result));
    queryFree(PRAGMA_JOURNAL_MODE_ID);
    LOG_DEBUG(_logger, "sqlite3 journal_mode=" << result);

    // PRAGMA_SYNCHRONOUS
    // With WAL journal the NORMAL sync mode is safe from corruption, otherwise use the standard FULL mode.
    std::string synchronousMode = "FULL";
    if (_journalMode.compare("WAL") == 0) synchronousMode = "NORMAL";
    sql = PRAGMA_SYNCHRONOUS + synchronousMode + ";";
    if (!createAndPrepareRequest(PRAGMA_SYNCHRONOUS_ID, sql.c_str())) return false;
    if (!queryNext(PRAGMA_SYNCHRONOUS_ID, hasData)) {
        LOG_WARN(_logger, "Error getting query result: " << PRAGMA_SYNCHRONOUS_ID);
        queryFree(PRAGMA_SYNCHRONOUS_ID);
        return false;
    }
    queryFree(PRAGMA_SYNCHRONOUS_ID);
    LOG_DEBUG(_logger, "sqlite3 synchronous=" << synchronousMode);

    // PRAGMA_CASE_SENSITIVE_LIKE
    if (!createAndPrepareRequest(PRAGMA_CASE_SENSITIVE_LIKE_ID, PRAGMA_CASE_SENSITIVE_LIKE)) return false;
    if (!queryNext(PRAGMA_CASE_SENSITIVE_LIKE_ID, hasData)) {
        LOG_WARN(_logger, "Error getting query result: " << PRAGMA_CASE_SENSITIVE_LIKE_ID);
        queryFree(PRAGMA_CASE_SENSITIVE_LIKE_ID);
        return false;
    }
    queryFree(PRAGMA_CASE_SENSITIVE_LIKE_ID);
    LOG_DEBUG(_logger, "sqlite3 case_sensitivity=ON");

    // PRAGMA_FOREIGN_KEYS
    if (!createAndPrepareRequest(PRAGMA_FOREIGN_KEYS_ID, PRAGMA_FOREIGN_KEYS)) return false;
    if (!queryNext(PRAGMA_FOREIGN_KEYS_ID, hasData)) {
        LOG_WARN(_logger, "Error getting query result: " << PRAGMA_FOREIGN_KEYS_ID);
        queryFree(PRAGMA_FOREIGN_KEYS_ID);
        return false;
    }
    queryFree(PRAGMA_FOREIGN_KEYS_ID);
    LOG_DEBUG(_logger, "sqlite3 foreign_keys=ON");

    return true;
}

bool Db::addIntegerColumnIfMissing(const std::string &tableName, const std::string &columnName, bool *columnAdded /*= nullptr*/) {
    const auto requestId = tableName + "add_column_" + columnName;
    const auto request = "ALTER TABLE " + tableName + " ADD COLUMN " + columnName + " INTEGER;";
    return addColumnIfMissing(tableName, columnName, requestId, request, columnAdded);
}

bool Db::addTextColumnIfMissing(const std::string &tableName, const std::string &columnName, bool *columnAdded /*= nullptr*/) {
    const auto requestId = tableName + "add_column_" + columnName;
    const auto request = "ALTER TABLE " + tableName + " ADD COLUMN " + columnName + " TEXT;";
    return addColumnIfMissing(tableName, columnName, requestId, request, columnAdded);
}

bool Db::addColumnIfMissing(const std::string &tableName, const std::string &columnName, const std::string &requestId,
                            const std::string &request, bool *columnAdded /*= nullptr*/) {
    bool exist = false;
    if (!columnExists(tableName, columnName, exist)) return false;
    if (!exist) {
        LOG_INFO(_logger, "Adding column " << columnName << " into table " << tableName);
        if (!createAndPrepareRequest(requestId.c_str(), request.c_str())) return false;
        int errId = 0;
        std::string error;
        if (!queryExec(requestId, errId, error)) {
            queryFree(requestId);
            return sqlFail(requestId, error);
        }
        queryFree(requestId);

        if (columnAdded) *columnAdded = true;
    }
    return true;
}

bool Db::createAndPrepareRequest(const char *requestId, const char *query) {
    int errId = 0;
    std::string error;

    if (!queryCreate(requestId)) {
        LOG_FATAL(_logger, "ENFORCE: \"queryCreate(" << requestId << ")\".");
    }
    if (!queryPrepare(requestId, query, false, errId, error)) {
        queryFree(requestId);
        return sqlFail(requestId, error);
    }

    return true;
}

bool Db::tableExists(const std::string &tableName, bool &exist) {
    const std::scoped_lock lock(_mutex);

    static const std::string id = CHECK_TABLE_EXISTENCE_REQUEST_ID;
    LOG_IF_FAIL(queryResetAndClearBindings(id));
    LOG_IF_FAIL(queryBindValue(id, 1, tableName));
    if (!queryNext(id, exist)) {
        LOG_WARN(_logger, "Error getting query result: " << id);
        return false;
    }

    return true;
}

bool Db::columnExists(const std::string &tableName, const std::string &columnName, bool &exist) {
    if (!tableExists(tableName, exist) || !exist) return false;

    const std::scoped_lock lock(_mutex);

    static const std::string id = CHECK_COLUMN_EXISTENCE_REQUEST_ID;
    LOG_IF_FAIL(queryResetAndClearBindings(id));
    LOG_IF_FAIL(queryBindValue(id, 1, tableName));
    LOG_IF_FAIL(queryBindValue(id, 2, columnName));

    bool found = false;
    if (!queryNext(id, found)) {
        LOG_WARN(_logger, "Error getting query result: " << id);
        return false;
    }
    if (!found) return false;

    int count = 0;
    LOG_IF_FAIL(queryIntValue(id, 0, count));
    LOG_IF_FAIL(queryResetAndClearBindings(id));

    exist = count != 0;
    return true;
}

void Db::setAutoDelete(bool value) {
    _sqliteDb->setAutoDelete(value);
}

bool Db::insertVersion(const std::string &version) {
    const std::scoped_lock lock(_mutex);

    // Insert exclusion template record
    int errId;
    std::string error;

    LOG_IF_FAIL(queryResetAndClearBindings(INSERT_VERSION_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(INSERT_VERSION_REQUEST_ID, 1, version));
    if (!queryExec(INSERT_VERSION_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << INSERT_VERSION_REQUEST_ID);
        return false;
    }

    return true;
}

bool Db::updateVersion(const std::string &version, bool &found) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    LOG_IF_FAIL(queryResetAndClearBindings(UPDATE_VERSION_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(UPDATE_VERSION_REQUEST_ID, 1, version));
    if (!queryExec(UPDATE_VERSION_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << UPDATE_VERSION_REQUEST_ID);
        return false;
    }
    if (numRowsAffected() == 1) {
        found = true;
    } else {
        LOG_WARN(_logger, "Error running query: " << UPDATE_VERSION_REQUEST_ID << " - num rows affected != 1");
        found = false;
    }

    return true;
}

bool Db::selectVersion(std::string &version, bool &found) {
    const std::scoped_lock lock(_mutex);

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_VERSION_REQUEST_ID));
    if (!queryNext(SELECT_VERSION_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_VERSION_REQUEST_ID);
        return false;
    }
    if (!found) {
        return true;
    }

    LOG_IF_FAIL(queryStringValue(SELECT_VERSION_REQUEST_ID, 0, version));

    return true;
}

} // namespace KDC
