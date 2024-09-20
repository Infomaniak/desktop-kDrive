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

#include "dbdefs.h"
#include "libcommon/utility/types.h"

#include <log4cplus/logger.h>

#include <string>

#define SQLITE_DO(A)                                                            \
    _errId = (A);                                                               \
    if (_errId != SQLITE_OK && _errId != SQLITE_DONE && _errId != SQLITE_ROW) { \
        _error = std::string(sqlite3_errmsg(_sqlite3Db.get()));                 \
    }

struct sqlite3;
struct sqlite3_stmt;

namespace KDC {

class SqliteQuery {
    public:
        explicit SqliteQuery();
        explicit SqliteQuery(std::shared_ptr<sqlite3> sqlite3Db);

        ~SqliteQuery();

        int prepare(const std::string &sql, bool allow_failure = false);
        void resetAndClearBindings();
        bool bindValue(int index, const dbtype &value);
        bool exec();
        bool execAndGetRowId(int64_t &rowId);

        struct NextResult {
                bool _ok = false;
                bool _hasData = false;
        };
        NextResult next();

        bool nullValue(int index) const;
        std::string const stringValue(int index) const;
        SyncName const syncNameValue(int index) const;
        int intValue(int index) const;
        int64_t int64Value(int index) const;
        double doubleValue(int index) const;
        const void *blobValue(int index) const;
        size_t blobSize(int index) const;

        inline int errorId() const { return _errId; }
        inline const std::string &error() const { return _error; }

    private:
        log4cplus::Logger _logger;
        std::shared_ptr<sqlite3> _sqlite3Db;
        std::shared_ptr<sqlite3_stmt> _stmt;
        int _errId;
        std::string _error;
        std::string _sql;

        bool isSelect() const;
        bool isInsert() const;
        bool isPragma() const;
};

} // namespace KDC
