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

#pragma once

#include "libcommonserver/commonserverlib.h"
#include "libcommon/utility/types.h"
#include "sqlitedb.h"

#include <log4cplus/logger.h>

#include <filesystem>

#include <Poco/URI.h>

namespace KDC {

class COMMONSERVER_EXPORT Db {
    public:
        Db(const std::filesystem::path &dbPath);
        virtual ~Db();

        static std::filesystem::path makeDbName(bool &alreadyExist, bool addRandomSuffix = false);
        static std::filesystem::path makeDbName(int userId, int accountId, int driveId, int syncDbId, bool &alreadyExist,
                                                bool addRandomSuffix = false);

        void setAutoDelete(bool value);
        bool exists();
        std::filesystem::path dbPath() const;
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
        bool queryIsNullValue(const std::string &id, int index, bool &ok);
        void queryFree(const std::string &id);

        int numRowsAffected() const;
        int extendedErrorCode() const;

        bool init(const std::string &version);
        virtual std::string dbType() const { return "Unknown"; }

        virtual bool create(bool &retry) = 0;
        virtual bool prepare() = 0;
        virtual bool upgrade(const std::string &fromVersion, const std::string &toVersion) = 0;

        inline const std::string &fromVersion() const { return _fromVersion; }

        inline int createNormalizeSyncNameFunc() { return _sqliteDb->createNormalizeSyncNameFunc(); };

        bool tableExists(const std::string &tableName, bool &exist);
        bool columnExists(const std::string &tableName, const std::string &columnName, bool &exist);

    protected:
        void startTransaction();
        void commitTransaction();
        void rollbackTransaction();
        bool sqlFail(const std::string &log, const std::string &error);
        bool checkConnect(const std::string &version);

        bool addIntegerColumnIfMissing(const std::string &tableName, const std::string &columnName, bool *columnAdded = nullptr);
        bool addColumnIfMissing(const std::string &tableName, const std::string &columnName, const std::string &requestId,
                                const std::string &request, bool *columnAdded = nullptr);

        // Helpers
        bool createAndPrepareRequest(const char *requestId, const char *query);

        log4cplus::Logger _logger;
        std::shared_ptr<SqliteDb> _sqliteDb;
        std::filesystem::path _dbPath;
        std::mutex _mutex;
        int _transaction;
        std::string _journalMode;
        std::string _fromVersion;

    private:
        bool insertVersion(const std::string &version);
        bool updateVersion(const std::string &version, bool &found);
        bool selectVersion(std::string &version, bool &found);

        friend class TestDb;
};

} // namespace KDC
