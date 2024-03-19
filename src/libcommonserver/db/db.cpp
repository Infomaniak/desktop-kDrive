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

#include "db.h"
#include "utility/utility.h"
#include "utility/asserts.h"
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
#define PRAGMA_LOCKING_MODE "PRAGMA locking_mode=NORMAL;"  // For debugging

#define PRAGMA_JOURNAL_MODE_ID "db3"
#define PRAGMA_JOURNAL_MODE "PRAGMA journal_mode="

#define PRAGMA_SYNCHRONOUS_ID "db4"
#define PRAGMA_SYNCHRONOUS "PRAGMA synchronous="

#define PRAGMA_CASE_SENSITIVE_LIKE_ID "db5"
#define PRAGMA_CASE_SENSITIVE_LIKE "PRAGMA case_sensitive_like=ON;"

#define PRAGMA_FOREIGN_KEYS_ID "db6"
#define PRAGMA_FOREIGN_KEYS "PRAGMA foreign_keys=ON;"

// Check if a table exists
#define CHECK_TABLE_EXISTS_ID "check_table_exists"
#define CHECK_TABLE_EXISTS "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?1;"

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

namespace KDC {

static std::string defaultJournalMode(const std::string &dbPath) {
#if defined(__APPLE__)
    if (Utility::startsWith(dbPath, "/Volumes/")) {
        return "DELETE";
    }
#elif defined(_WIN32)
    std::string fsName = Utility::fileSystemName(dbPath);
    if (fsName.find("FAT") != std::string::npos) {
        return "DELETE";
    }
#else
    (void)dbPath;
#endif

    return "WAL";
}

Db::Db(const std::filesystem::path &dbPath)
    : _logger(Log::instance()->getLogger()),
      _sqliteDb(new SqliteDb()),
      _dbPath(dbPath),
      _transaction(false),
      _journalMode(defaultJournalMode(dbPath.string())),
      _fromVersion(std::string()) {}

Db::~Db() {
    close();
}

std::filesystem::path Db::makeDbName(bool &alreadyExist, bool addRandomSuffix /*= false*/) {
    return makeDbName(0, 0, 0, 0, alreadyExist, addRandomSuffix);
}

std::filesystem::path Db::makeDbName(int userId, int accountId, int driveId, int syncDbId, bool &alreadyExist,
                                     bool addRandomSuffix /*= false*/) {
    // App support dir
    std::filesystem::path dbPath(CommonUtility::getAppSupportDir());

    bool exists = false;
    IoError ioError = IoErrorSuccess;

    if (!IoHelper::checkIfPathExists(dbPath, exists, ioError)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(dbPath, ioError).c_str());
        return std::filesystem::path();
    }

    if (!exists) {
        if (!IoHelper::createDirectory(dbPath, ioError)) {
            LOGW_WARN(Log::instance()->getLogger(),
                      L"Failed to create directory: " << Utility::formatIoError(dbPath, ioError).c_str());
            return std::filesystem::path();
        }
    }

    // Db file name
    std::string dbFile;
    if (!userId && !accountId && !driveId && !syncDbId) {
        std::string suffix;
        if (addRandomSuffix) {
            suffix = CommonUtility::generateRandomStringAlphaNum();
        }
        dbFile.append(".parms" + suffix + ".db");
    } else {
        std::string key(std::to_string(userId));
        key.append(":");
        key.append(std::to_string(accountId));
        key.append(":");
        key.append(std::to_string(driveId));
        key.append(":");
        key.append(std::to_string(syncDbId));
        Poco::MD5Engine md5;
        md5.update(key.c_str());
        std::string keyHash = Poco::DigestEngine::digestToHex(md5.digest()).substr(0, 6);
        std::string keyHashHex;
        Utility::str2hexstr(keyHash, keyHashHex);

        dbFile.append(".sync_");
        dbFile.append(keyHashHex);
        if (addRandomSuffix) {
            dbFile.append(CommonUtility::generateRandomStringAlphaNum());
        }
        dbFile.append(".db");
    }

    // Db file path
    dbPath.append(dbFile);

    // If it exists already, the path is clearly usable
    if (!IoHelper::checkIfPathExists(dbPath, exists, ioError)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(dbPath, ioError).c_str());
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
    const std::lock_guard<std::mutex> lock(_mutex);

    if (_dbPath.empty()) {
        return false;
    } else {
        bool exists = false;
        IoError ioError = IoErrorSuccess;
        if (!IoHelper::checkIfPathExists(_dbPath, exists, ioError)) {
            LOGW_WARN(Log::instance()->getLogger(),
                      L"Error in IoHelper::checkIfPathExists for path=" << Utility::formatIoError(_dbPath, ioError).c_str());
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

    const std::lock_guard<std::mutex> lock(_mutex);

    LOGW_DEBUG(_logger, L"Closing DB " << Path2WStr(_dbPath).c_str());

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
    ASSERT(_sqliteDb->queryResetAndClearBindings(id));
    return ret;
}

bool Db::queryExecAndGetRowId(const std::string &id, int64_t &rowId, int &errId, std::string &error) {
    bool ret = _sqliteDb->queryExecAndGetRowId(id, rowId, errId, error);
    ASSERT(_sqliteDb->queryResetAndClearBindings(id));
    return ret;
}

bool Db::queryNext(const std::string &id, bool &hasData) {
    bool ret = _sqliteDb->queryNext(id, hasData);
    if (!ret || !hasData) {
        ASSERT(_sqliteDb->queryResetAndClearBindings(id));
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

    int errId;
    std::string error;

    if (!version.empty()) {
        // Check if DB is already initialized
        bool dbExists;
        if (!checkIfTableExists("version", dbExists)) {
            return false;
        }

        if (dbExists) {
            // Check version
            LOG_DEBUG(_logger, "Check DB version");
            ASSERT(queryCreate(SELECT_VERSION_REQUEST_ID));
            if (!queryPrepare(SELECT_VERSION_REQUEST_ID, SELECT_VERSION_REQUEST, false, errId, error)) {
                queryFree(SELECT_VERSION_REQUEST_ID);
                return sqlFail(SELECT_VERSION_REQUEST_ID, error);
            }

            bool found;
            if (!selectVersion(_fromVersion, found)) {
                LOG_WARN(_logger, "Error in Db::selectVersion");
                return false;
            }
            if (!found) {
                LOG_WARN(_logger, "Version not found");
                return false;
            }

            queryFree(SELECT_VERSION_REQUEST_ID);

            if (_fromVersion != version) {
                // Upgrade DB
                LOG_INFO(_logger, "Upgrade DB from " << _fromVersion.c_str() << " to " << version.c_str());
                if (!upgrade(_fromVersion, version)) {
                    LOG_WARN(_logger, "Error in Db::upgrade");
                    return false;
                }

                // Update version
                ASSERT(queryCreate(UPDATE_VERSION_REQUEST_ID));
                if (!queryPrepare(UPDATE_VERSION_REQUEST_ID, UPDATE_VERSION_REQUEST, false, errId, error)) {
                    queryFree(UPDATE_VERSION_REQUEST_ID);
                    return sqlFail(UPDATE_VERSION_REQUEST_ID, error);
                }

                if (!updateVersion(version, found)) {
                    LOG_WARN(_logger, "Error in Db::updateVersion");
                    return false;
                }
                if (!found) {
                    LOG_WARN(_logger, "Version not found");
                    return false;
                }

                queryFree(UPDATE_VERSION_REQUEST_ID);
            }
        } else {
            // Create version table
            LOG_DEBUG(_logger, "Create version table");
            ASSERT(queryCreate(CREATE_VERSION_TABLE_ID));
            if (!queryPrepare(CREATE_VERSION_TABLE_ID, CREATE_VERSION_TABLE, false, errId, error)) {
                queryFree(CREATE_VERSION_TABLE_ID);
                return sqlFail(CREATE_VERSION_TABLE_ID, error);
            }
            if (!queryExec(CREATE_VERSION_TABLE_ID, errId, error)) {
                queryFree(CREATE_VERSION_TABLE_ID);
                return sqlFail(CREATE_VERSION_TABLE_ID, error);
            }
            queryFree(CREATE_VERSION_TABLE_ID);

            // Insert version
            LOG_DEBUG(_logger, "Insert version " << version.c_str());
            ASSERT(queryCreate(INSERT_VERSION_REQUEST_ID));
            if (!queryPrepare(INSERT_VERSION_REQUEST_ID, INSERT_VERSION_REQUEST, false, errId, error)) {
                queryFree(INSERT_VERSION_REQUEST_ID);
                return sqlFail(INSERT_VERSION_REQUEST_ID, error);
            }

            if (!insertVersion(version)) {
                LOG_WARN(_logger, "Error in Db::insertVersion");
                return false;
            }

            queryFree(INSERT_VERSION_REQUEST_ID);

            // Create DB
            LOG_INFO(_logger, "Create DB");
            bool retry;
            if (!create(retry)) {
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
    LOG_INFO(_logger, "Prepare DB");
    if (!prepare()) {
        LOG_WARN(_logger, "Error in Db::prepare");
        return false;
    }

    return true;
}

void Db::startTransaction() {
    if (!_transaction) {
        if (!_sqliteDb->startTransaction()) {
            LOG_WARN(_logger, "ERROR starting transaction: " << _sqliteDb->error().c_str());
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
            LOG_WARN(_logger, "ERROR committing to the database: " << _sqliteDb->error().c_str());
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
            LOG_WARN(_logger, "ERROR rollbacking to the database: " << _sqliteDb->error().c_str());
            return;
        }
        _transaction = false;
    } else {
        LOG_DEBUG(_logger, "No database Transaction to rollback");
    }
}

bool Db::sqlFail(const std::string &log, const std::string &error) {
    commitTransaction();
    LOG_WARN(_logger, "SQL Error - " << log.c_str() << " - " << error.c_str());
    _sqliteDb->close();
    ASSERT(false);
    return false;
}

bool Db::checkConnect(const std::string &version) {
    if (_sqliteDb && _sqliteDb->isOpened()) {
        // Unfortunately the sqlite isOpen check can return true even when the underlying storage
        // has become unavailable - and then some operations may cause crashes.
        bool exists = false;
        IoError ioError = IoErrorSuccess;
        if (!IoHelper::checkIfPathExists(_dbPath, exists, ioError)) {
            LOGW_WARN(Log::instance()->getLogger(),
                      L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_dbPath, ioError).c_str());
            close();
            return false;
        }

        if (!exists) {
            LOGW_WARN(_logger, L"Database is opened, but file " << Path2WStr(_dbPath).c_str() << L" does not exist");
            close();
            return false;
        }

        return true;
    }

    if (_dbPath.empty()) {
        LOGW_WARN(_logger, L"Database filename" << Path2WStr(_dbPath).c_str() << L" is empty");
        return false;
    }

    // The database file is created by this call (SQLITE_OPEN_CREATE)
    if (!_sqliteDb->openOrCreateReadWrite(_dbPath)) {
        std::string error = _sqliteDb->error();
        LOG_WARN(_logger, "Error opening the db: " << error.c_str());
        return false;
    }

    bool exists = false;
    IoError ioError = IoErrorSuccess;
    if (!IoHelper::checkIfPathExists(_dbPath, exists, ioError)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in IoHelper::checkIfPathExists for path=" << Utility::formatIoError(_dbPath, ioError).c_str());
        return false;
    }

    if (!exists) {
        LOGW_WARN(_logger, L"Database file" << Path2WStr(_dbPath).c_str() << L" does not exist");
        return false;
    }

    // SELECT_SQLITE_VERSION
    int errId;
    std::string error;

    ASSERT(queryCreate(SELECT_SQLITE_VERSION_ID));
    if (!queryPrepare(SELECT_SQLITE_VERSION_ID, SELECT_SQLITE_VERSION, false, errId, error)) {
        LOG_WARN(_logger, "Error preparing query:" << SELECT_SQLITE_VERSION_ID);
        queryFree(SELECT_SQLITE_VERSION_ID);
        return false;
    }
    bool hasData;
    if (!queryNext(SELECT_SQLITE_VERSION_ID, hasData) || !hasData) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_SQLITE_VERSION_ID);
        queryFree(SELECT_SQLITE_VERSION_ID);
        return false;
    }
    std::string result;
    ASSERT(queryStringValue(SELECT_SQLITE_VERSION_ID, 0, result));
    queryFree(SELECT_SQLITE_VERSION_ID);
    LOG_DEBUG(_logger, "sqlite3 version=" << result.c_str());

    // PRAGMA_LOCKING_MODE
    ASSERT(queryCreate(PRAGMA_LOCKING_MODE_ID));
    if (!queryPrepare(PRAGMA_LOCKING_MODE_ID, PRAGMA_LOCKING_MODE, false, errId, error)) {
        LOG_WARN(_logger, "Error preparing query:" << PRAGMA_LOCKING_MODE_ID);
        queryFree(PRAGMA_LOCKING_MODE_ID);
        return false;
    }
    if (!queryNext(PRAGMA_LOCKING_MODE_ID, hasData) || !hasData) {
        LOG_WARN(_logger, "Error getting query result: " << PRAGMA_LOCKING_MODE_ID);
        queryFree(PRAGMA_LOCKING_MODE_ID);
        return false;
    }
    ASSERT(queryStringValue(PRAGMA_LOCKING_MODE_ID, 0, result));
    queryFree(PRAGMA_LOCKING_MODE_ID);
    LOG_DEBUG(_logger, "sqlite3 locking_mode=" << result.c_str());

    // PRAGMA_JOURNAL_MODE
    std::string sql(PRAGMA_JOURNAL_MODE + _journalMode + ";");
    ASSERT(queryCreate(PRAGMA_JOURNAL_MODE_ID));
    if (!queryPrepare(PRAGMA_JOURNAL_MODE_ID, sql, false, errId, error)) {
        LOG_WARN(_logger, "Error preparing query:" << PRAGMA_JOURNAL_MODE_ID);
        queryFree(PRAGMA_JOURNAL_MODE_ID);
        return false;
    }
    if (!queryNext(PRAGMA_JOURNAL_MODE_ID, hasData) || !hasData) {
        LOG_WARN(_logger, "Error getting query result: " << PRAGMA_JOURNAL_MODE_ID);
        queryFree(PRAGMA_JOURNAL_MODE_ID);
        return false;
    }
    ASSERT(queryStringValue(PRAGMA_JOURNAL_MODE_ID, 0, result));
    queryFree(PRAGMA_JOURNAL_MODE_ID);
    LOG_DEBUG(_logger, "sqlite3 journal_mode=" << result.c_str());

    // PRAGMA_SYNCHRONOUS
    // With WAL journal the NORMAL sync mode is safe from corruption, otherwise use the standard FULL mode.
    std::string synchronousMode = "FULL";
    if (_journalMode.compare("WAL") == 0) synchronousMode = "NORMAL";
    sql = PRAGMA_SYNCHRONOUS + synchronousMode + ";";
    ASSERT(queryCreate(PRAGMA_SYNCHRONOUS_ID));
    if (!queryPrepare(PRAGMA_SYNCHRONOUS_ID, sql, false, errId, error)) {
        LOG_WARN(_logger, "Error preparing query:" << PRAGMA_SYNCHRONOUS_ID);
        queryFree(PRAGMA_SYNCHRONOUS_ID);
        return false;
    }
    if (!queryNext(PRAGMA_SYNCHRONOUS_ID, hasData)) {
        LOG_WARN(_logger, "Error getting query result: " << PRAGMA_SYNCHRONOUS_ID);
        queryFree(PRAGMA_SYNCHRONOUS_ID);
        return false;
    }
    queryFree(PRAGMA_SYNCHRONOUS_ID);
    LOG_DEBUG(_logger, "sqlite3 synchronous=" << synchronousMode.c_str());

    // PRAGMA_CASE_SENSITIVE_LIKE
    ASSERT(queryCreate(PRAGMA_CASE_SENSITIVE_LIKE_ID));
    if (!queryPrepare(PRAGMA_CASE_SENSITIVE_LIKE_ID, PRAGMA_CASE_SENSITIVE_LIKE, false, errId, error)) {
        LOG_WARN(_logger, "Error preparing query:" << PRAGMA_CASE_SENSITIVE_LIKE_ID);
        queryFree(PRAGMA_CASE_SENSITIVE_LIKE_ID);
        return false;
    }
    if (!queryNext(PRAGMA_CASE_SENSITIVE_LIKE_ID, hasData)) {
        LOG_WARN(_logger, "Error getting query result: " << PRAGMA_CASE_SENSITIVE_LIKE_ID);
        queryFree(PRAGMA_CASE_SENSITIVE_LIKE_ID);
        return false;
    }
    queryFree(PRAGMA_CASE_SENSITIVE_LIKE_ID);
    LOG_DEBUG(_logger, "sqlite3 case_sensitivity=ON");

    // PRAGMA_FOREIGN_KEYS
    ASSERT(queryCreate(PRAGMA_FOREIGN_KEYS_ID));
    if (!queryPrepare(PRAGMA_FOREIGN_KEYS_ID, PRAGMA_FOREIGN_KEYS, false, errId, error)) {
        LOG_WARN(_logger, "Error preparing query:" << PRAGMA_FOREIGN_KEYS_ID);
        queryFree(PRAGMA_FOREIGN_KEYS_ID);
        return false;
    }
    if (!queryNext(PRAGMA_FOREIGN_KEYS_ID, hasData)) {
        LOG_WARN(_logger, "Error getting query result: " << PRAGMA_FOREIGN_KEYS_ID);
        queryFree(PRAGMA_FOREIGN_KEYS_ID);
        return false;
    }
    queryFree(PRAGMA_FOREIGN_KEYS_ID);
    LOG_DEBUG(_logger, "sqlite3 foreign_keys=ON");

    // DB initialization
    if (!init(version)) {
        LOG_ERROR(_logger, "Database initialisation error");
        return false;
    }

    return true;
}

void Db::setAutoDelete(bool value) {
    _sqliteDb->setAutoDelete(value);
}

bool Db::checkIfTableExists(const std::string &tableName, bool &found) {
    const std::lock_guard<std::mutex> lock(_mutex);

    int errId;
    std::string error;
    ASSERT(queryCreate(CHECK_TABLE_EXISTS_ID));
    if (!queryPrepare(CHECK_TABLE_EXISTS_ID, CHECK_TABLE_EXISTS, false, errId, error)) {
        queryFree(CHECK_TABLE_EXISTS_ID);
        return sqlFail(CHECK_TABLE_EXISTS_ID, error);
    }

    ASSERT(queryResetAndClearBindings(CHECK_TABLE_EXISTS_ID));
    ASSERT(queryBindValue(CHECK_TABLE_EXISTS_ID, 1, tableName));
    if (!queryNext(CHECK_TABLE_EXISTS_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << CHECK_TABLE_EXISTS_ID);
        queryFree(CHECK_TABLE_EXISTS_ID);
        return false;
    }

    queryFree(CHECK_TABLE_EXISTS_ID);

    return true;
}

bool Db::insertVersion(const std::string &version) {
    const std::lock_guard<std::mutex> lock(_mutex);

    // Insert exclusion template record
    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(INSERT_VERSION_REQUEST_ID));
    ASSERT(queryBindValue(INSERT_VERSION_REQUEST_ID, 1, version));
    if (!queryExec(INSERT_VERSION_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << INSERT_VERSION_REQUEST_ID);
        return false;
    }

    return true;
}

bool Db::updateVersion(const std::string &version, bool &found) {
    const std::lock_guard<std::mutex> lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(UPDATE_VERSION_REQUEST_ID));
    ASSERT(queryBindValue(UPDATE_VERSION_REQUEST_ID, 1, version));
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
    const std::lock_guard<std::mutex> lock(_mutex);

    ASSERT(queryResetAndClearBindings(SELECT_VERSION_REQUEST_ID));
    if (!queryNext(SELECT_VERSION_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_VERSION_REQUEST_ID);
        return false;
    }
    if (!found) {
        return true;
    }

    ASSERT(queryStringValue(SELECT_VERSION_REQUEST_ID, 0, version));

    return true;
}

}  // namespace KDC
