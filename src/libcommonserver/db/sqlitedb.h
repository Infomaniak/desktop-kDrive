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

#include "sqlitequery.h"

#include <log4cplus/logger.h>

#include <string>
#include <filesystem>
#include <unordered_map>

struct sqlite3;

namespace KDC {

class SqliteDb {
    public:
        explicit SqliteDb();
        ~SqliteDb();

        bool openOrCreateReadWrite(const std::filesystem::path &dbPath);
        bool openReadOnly(const std::filesystem::path &dbPath);
        bool isOpened() const;
        bool startTransaction();
        bool commit();
        bool rollback();
        void close();

        bool queryCreate(const std::string &id);
        bool queryPrepare(const std::string &id, const std::string sql, bool allow_failure, int &errId, std::string &error);
        bool queryResetAndClearBindings(const std::string &id);
        bool queryBindValue(const std::string &id, int index, const dbtype &value);
        bool queryExec(const std::string &id, int &errId, std::string &error);
        bool queryExecAndGetRowId(const std::string &id, int64_t &rowId, int &errId, std::string &error);
        bool queryNext(const std::string &id, bool &hasData);
        bool queryIntValue(const std::string &id, int index, int &value) const;
        bool queryInt64Value(const std::string &id, int index, int64_t &value) const;
        bool queryDoubleValue(const std::string &id, int index, double &value) const;
        bool queryStringValue(const std::string &id, int index, std::string &value) const;
        bool querySyncNameValue(const std::string &id, int index, SyncName &value) const;
        bool queryBlobValue(const std::string &id, int index, std::shared_ptr<std::vector<char>> &value) const;
        bool queryIsNullValue(const std::string &id, int index, bool &ok) const;
        void queryFree(const std::string &id) noexcept;

        int numRowsAffected() const;
        inline int errorId() const { return _errId; }
        inline const std::string &error() const { return _error; }
        int extendedErrorCode() const;
        inline void setAutoDelete(bool value) { _autoDelete = value; }

        int createNormalizeSyncNameFunc();

    private:
        enum class CheckDbResult {
            Ok,
            CantPrepare,
            CantExec,
            NotOk,
        };

        struct QueryInfo {
                std::shared_ptr<SqliteQuery> _query;
                bool _isPrepared;
                bool _isExecuted;
                SqliteQuery::NextResult _result;
        };

        log4cplus::Logger _logger;
        std::shared_ptr<sqlite3> _sqlite3Db;
        int _errId;
        std::string _error;
        std::filesystem::path _dbPath;
        std::unordered_map<std::string, QueryInfo> _queries;
        bool _autoDelete;

        bool openHelper(const std::filesystem::path &dbPath, int sqliteFlags);
        CheckDbResult checkDb();
};

} // namespace KDC
