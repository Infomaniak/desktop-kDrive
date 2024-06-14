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

#include "syncdb.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/utility/asserts.h"
#include "libcommonserver/log/log.h"

#include <queue>

#include <3rdparty/sqlite3/sqlite3.h>

#define PRAGMA_WRITABLE_SCHEMA_ID "db1"
#define PRAGMA_WRITABLE_SCHEMA "PRAGMA writable_schema=ON;"

//
// node
//
#define CREATE_NODE_TABLE_ID "create_node"
#define CREATE_NODE_TABLE              \
    "CREATE TABLE IF NOT EXISTS node(" \
    "nodeId INTEGER PRIMARY KEY,"      \
    "parentNodeId INTEGER,"            \
    "nameLocal TEXT,"                  \
    "nameDrive TEXT,"                  \
    "nodeIdLocal TEXT,"                \
    "nodeIdDrive TEXT,"                \
    "created INTEGER,"                 \
    "lastModifiedLocal INTEGER,"       \
    "lastModifiedDrive INTEGER,"       \
    "type INTEGER,"                    \
    "size INTEGER,"                    \
    "checksum TEXT,"                   \
    "status INTEGER,"                  \
    "syncing INTEGER,"                 \
    "FOREIGN KEY (parentNodeId) REFERENCES node(nodeId) ON DELETE CASCADE ON UPDATE NO ACTION);"

#define ALTER_NODE_TABLE_FK_ID "alter_node_fk"
#define ALTER_NODE_TABLE_FK                                                                                                    \
    "UPDATE SQLITE_MASTER SET sql = replace(sql, ')', ',FOREIGN KEY (parentNodeId) REFERENCES node(nodeId) ON DELETE CASCADE " \
    "ON UPDATE NO ACTION)')"                                                                                                   \
    "WHERE name = 'node' AND type = 'table';"

#define CREATE_NODE_TABLE_IDX1_ID "create_node_idx1"
#define CREATE_NODE_TABLE_IDX1 "CREATE INDEX IF NOT EXISTS node_idx1 ON node(parentNodeId);"

#define CREATE_NODE_TABLE_IDX2_ID "create_node_idx2"
#define CREATE_NODE_TABLE_IDX2 "CREATE UNIQUE INDEX IF NOT EXISTS node_idx2 ON node(nodeIdLocal);"

#define CREATE_NODE_TABLE_IDX3_ID "create_node_idx3"
#define CREATE_NODE_TABLE_IDX3 "CREATE UNIQUE INDEX IF NOT EXISTS node_idx3 ON node(nodeIdDrive);"

#define CREATE_NODE_TABLE_IDX4_ID "create_node_idx4"
#define CREATE_NODE_TABLE_IDX4 "CREATE INDEX IF NOT EXISTS node_idx4 ON node(parentNodeId, nameLocal);"

#define CREATE_NODE_TABLE_IDX5_ID "create_node_idx5"
#define CREATE_NODE_TABLE_IDX5 "CREATE INDEX IF NOT EXISTS node_idx5 ON node(parentNodeId, nameDrive);"

#define INSERT_NODE_REQUEST_ID "insert_node"
#define INSERT_NODE_REQUEST                                                                                        \
    "INSERT INTO node (parentNodeId, nameLocal, nameDrive, nodeIdLocal, nodeIdDrive, created, lastModifiedLocal, " \
    "lastModifiedDrive, type, size, checksum, status, syncing) "                                                   \
    "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13);"

#define UPDATE_NODE_REQUEST_ID "update_node"
#define UPDATE_NODE_REQUEST                                                                                     \
    "UPDATE node SET parentNodeId=?1, nameLocal=?2, nameDrive=?3, nodeIdLocal=?4, nodeIdDrive=?5, created=?6, " \
    "lastModifiedLocal=?7, lastModifiedDrive=?8, type=?9, size=?10, checksum=?11, status=?12, syncing=?13 "     \
    "WHERE nodeId=?14;"

#define UPDATE_NODE_STATUS_REQUEST_ID "update_node_status"
#define UPDATE_NODE_STATUS_REQUEST \
    "UPDATE node SET status=?1 "   \
    "WHERE nodeId=?2;"

#define UPDATE_NODES_SYNCING_REQUEST_ID "update_nodes_syncing"
#define UPDATE_NODES_SYNCING_REQUEST "UPDATE node SET syncing=?1;"

#define UPDATE_NODE_SYNCING_REQUEST_ID "update_node_syncing"
#define UPDATE_NODE_SYNCING_REQUEST \
    "UPDATE node SET syncing=?1 "   \
    "WHERE nodeId=?2;"

#define DELETE_NODE_REQUEST_ID "delete_node"
#define DELETE_NODE_REQUEST \
    "DELETE FROM node "     \
    "WHERE nodeId=?1;"

#define DELETE_NODES_BUT_ROOT_REQUEST_ID "delete_node2"
#define DELETE_NODES_BUT_ROOT_REQUEST "DELETE FROM node WHERE nodeId<>1;"

#define DELETE_NODES_WITH_NULL_PARENTNODEID_REQUEST_ID "delete_node3"
#define DELETE_NODES_WITH_NULL_PARENTNODEID_REQUEST "DELETE FROM node WHERE nodeId<>1 AND parentNodeId IS NULL;"

#define SELECT_NODE_BY_NODEID_LITE_ID "select_node"
#define SELECT_NODE_BY_NODEID_LITE                                                   \
    "SELECT parentNodeId, nameLocal, nameDrive, nodeIdLocal, nodeIdDrive FROM node " \
    "WHERE nodeId=?1;"

#define SELECT_NODE_BY_NODEID_FULL_ID "select_node2"
#define SELECT_NODE_BY_NODEID_FULL                                                                                               \
    "SELECT parentNodeId, nameLocal, nameDrive, nodeIdLocal, nodeIdDrive, created, lastModifiedLocal, lastModifiedDrive, type, " \
    "size, checksum, status, syncing FROM node "                                                                                 \
    "WHERE nodeId=?1;"

#define SELECT_NODE_BY_NODEID_PARENTID 0
#define SELECT_NODE_BY_NODEID_NAMELOCAL 1
#define SELECT_NODE_BY_NODEID_NAMEDRIVE 2
#define SELECT_NODE_BY_NODEID_IDLOCAL 3
#define SELECT_NODE_BY_NODEID_IDDRIVE 4
#define SELECT_NODE_BY_NODEID_CREATED 5
#define SELECT_NODE_BY_NODEID_LASTMODLOCAL 6
#define SELECT_NODE_BY_NODEID_LASTMODDRIVE 7
#define SELECT_NODE_BY_NODEID_TYPE 8
#define SELECT_NODE_BY_NODEID_SIZE 9
#define SELECT_NODE_BY_NODEID_CHECKSUM 10
#define SELECT_NODE_BY_NODEID_STATUS 11
#define SELECT_NODE_BY_NODEID_SYNCING 12

#define SELECT_NODE_BY_NODEIDLOCAL_ID "select_node3"
#define SELECT_NODE_BY_NODEIDLOCAL                                                                                               \
    "SELECT nodeId, parentNodeId, nameLocal, nameDrive, created, lastModifiedLocal, type, size, checksum, status, syncing FROM " \
    "node "                                                                                                                      \
    "WHERE nodeIdLocal=?1;"

#define SELECT_NODE_BY_NODEIDDRIVE_ID "select_node4"
#define SELECT_NODE_BY_NODEIDDRIVE                                                                                               \
    "SELECT nodeId, parentNodeId, nameLocal, nameDrive, created, lastModifiedDrive, type, size, checksum, status, syncing FROM " \
    "node "                                                                                                                      \
    "WHERE nodeIdDrive=?1;"

#define SELECT_NODE_BY_REPLICAID_DBID 0
#define SELECT_NODE_BY_REPLICAID_PARENTID 1
#define SELECT_NODE_BY_REPLICAID_NAMELOCAL 2
#define SELECT_NODE_BY_REPLICAID_NAMEDRIVE 3
#define SELECT_NODE_BY_REPLICAID_CREATED 4
#define SELECT_NODE_BY_REPLICAID_LASTMOD 5
#define SELECT_NODE_BY_REPLICAID_TYPE 6
#define SELECT_NODE_BY_REPLICAID_SIZE 7
#define SELECT_NODE_BY_REPLICAID_CHECKSUM 8
#define SELECT_NODE_BY_REPLICAID_STATUS 9
#define SELECT_NODE_BY_REPLICAID_SYNCING 10

#define SELECT_NODE_BY_PARENTNODEID_AND_NAMELOCAL_REQUEST_ID "select_node5"
#define SELECT_NODE_BY_PARENTNODEID_AND_NAMELOCAL_REQUEST    \
    "SELECT nodeId, nodeIdLocal, status, syncing FROM node " \
    "WHERE parentNodeId=?1 AND nameLocal=?2;"

#define SELECT_NODE_BY_PARENTNODEID_AND_NAMEDRIVE_REQUEST_ID "select_node6"
#define SELECT_NODE_BY_PARENTNODEID_AND_NAMEDRIVE_REQUEST    \
    "SELECT nodeId, nodeIdDrive, status, syncing FROM node " \
    "WHERE parentNodeId=?1 AND nameDrive=?2;"

#define SELECT_NODE_BY_PARENTNODEID_REQUEST_ID "select_node7"
#define SELECT_NODE_BY_PARENTNODEID_REQUEST                                                  \
    "SELECT nodeId, nameLocal, nameDrive, nodeIdLocal, nodeIdDrive, type, status FROM node " \
    "WHERE parentNodeId=?1;"

#define SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID "select_node8"
#define SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST                                                      \
    "SELECT nodeId, nameLocal, nameDrive, nodeIdLocal, nodeIdDrive, type, status, syncing FROM node " \
    "WHERE parentNodeId IS NULL;"

#define SELECT_NODE_STATUS_BY_NODEID_REQUEST_ID "select_node9"
#define SELECT_NODE_STATUS_BY_NODEID_REQUEST \
    "SELECT status FROM node "               \
    "WHERE nodeId=?1;"

#define SELECT_NODE_SYNCING_BY_NODEID_REQUEST_ID "select_node10"
#define SELECT_NODE_SYNCING_BY_NODEID_REQUEST \
    "SELECT syncing FROM node "               \
    "WHERE nodeId=?1;"

#define SELECT_ALL_RENAMED_COLON_NODES_REQUEST_ID "select_node11"
#define SELECT_ALL_RENAMED_COLON_NODES_REQUEST                                                                  \
    "SELECT nodeId, parentNodeId, nameLocal, nameDrive, nodeIdLocal, nodeIdDrive, created, lastModifiedLocal, " \
    "lastModifiedDrive, type, size, checksum, status, syncing FROM node "                                       \
    "WHERE nameLocal != nameDrive AND instr(nameDrive, ':') > 0;"

#define SELECT_ALL_RENAMED_NODES_REQUEST_ID "select_node12"
#define SELECT_ALL_RENAMED_NODES_REQUEST                                                                        \
    "SELECT nodeId, parentNodeId, nameLocal, nameDrive, nodeIdLocal, nodeIdDrive, created, lastModifiedLocal, " \
    "lastModifiedDrive, type, size, checksum, status, syncing FROM node "                                       \
    "WHERE nameLocal != nameDrive;"

//
// sync_node
//
#define CREATE_SYNC_NODE_TABLE_ID "create_sync_node"
#define CREATE_SYNC_NODE_TABLE              \
    "CREATE TABLE IF NOT EXISTS sync_node(" \
    "nodeId TEXT,"                          \
    "type INTEGER);"

#define INSERT_SYNC_NODE_REQUEST_ID "insert_sync_node"
#define INSERT_SYNC_NODE_REQUEST            \
    "INSERT INTO sync_node (nodeId, type) " \
    "VALUES (?1, ?2);"

#define DELETE_ALL_SYNC_NODE_BY_TYPE_REQUEST_ID "delete_all_sync_node_by_type"
#define DELETE_ALL_SYNC_NODE_BY_TYPE_REQUEST \
    "DELETE FROM sync_node "                 \
    "WHERE type=?1;"

#define SELECT_ALL_SYNC_NODE_REQUEST_ID "select_sync_node"
#define SELECT_ALL_SYNC_NODE_REQUEST \
    "SELECT nodeId FROM sync_node "  \
    "WHERE type=?1;"

//
// upload_session_token
//
#define CREATE_UPLOAD_SESSION_TOKEN_TABLE_ID "create_upload_session_token"
#define CREATE_UPLOAD_SESSION_TOKEN_TABLE              \
    "CREATE TABLE IF NOT EXISTS upload_session_token(" \
    "dbId INTEGER PRIMARY KEY,"                        \
    "token TEXT);"

#define INSERT_UPLOAD_SESSION_TOKEN_REQUEST_ID "insert_upload_session_token"
#define INSERT_UPLOAD_SESSION_TOKEN_REQUEST     \
    "INSERT INTO upload_session_token (token) " \
    "VALUES (?1);"

#define SELECT_ALL_UPLOAD_SESSION_TOKEN_REQUEST_ID "select_upload_session_token"
#define SELECT_ALL_UPLOAD_SESSION_TOKEN_REQUEST "SELECT dbId, token FROM upload_session_token;"

#define DELETE_UPLOAD_SESSION_TOKEN_BY_DBID_REQUEST_ID "delete_upload_session_token_by_dbid"
#define DELETE_UPLOAD_SESSION_TOKEN_BY_DBID_REQUEST \
    "DELETE FROM upload_session_token "             \
    "WHERE dbId=?1;"

#define DELETE_ALL_UPLOAD_SESSION_TOKEN_REQUEST_ID "delete_all_upload_session_token"
#define DELETE_ALL_UPLOAD_SESSION_TOKEN_REQUEST "DELETE FROM upload_session_token;"

namespace KDC {

DbNode SyncDb::_driveRootNode(0, std::nullopt, SyncName(), SyncName(), "1", "1", std::nullopt, std::nullopt, std::nullopt,
                              NodeType::Directory, 0, std::nullopt);

SyncDb::SyncDb(const std::string &dbPath, const std::string &version, const std::string &targetNodeId)
    : Db(dbPath), _rootNode(_driveRootNode) {
    if (!targetNodeId.empty()) {
        _rootNode.setNodeIdRemote(targetNodeId);
    }

    if (!checkConnect(version)) {
        throw std::runtime_error("Cannot open DB!");
    }
}

bool SyncDb::create(bool &retry) {
    int errId;
    std::string error;

    // Node
    ASSERT(queryCreate(CREATE_NODE_TABLE_ID));
    if (!queryPrepare(CREATE_NODE_TABLE_ID, CREATE_NODE_TABLE, false, errId, error)) {
        queryFree(CREATE_NODE_TABLE_ID);
        return sqlFail(CREATE_NODE_TABLE_ID, error);
    }
    if (!queryExec(CREATE_NODE_TABLE_ID, errId, error)) {
        // In certain situations the io error can be avoided by switching
        // to the DELETE journal mode
        if (_journalMode != "DELETE" && errId == SQLITE_IOERR && extendedErrorCode() == SQLITE_IOERR_SHMMAP) {
            LOG_WARN(_logger, "IO error SHMMAP on table creation, attempting with DELETE journal mode");
            _journalMode = "DELETE";
            queryFree(CREATE_NODE_TABLE_ID);
            retry = true;
            return false;
        }

        return sqlFail(CREATE_NODE_TABLE_ID, error);
    }
    queryFree(CREATE_NODE_TABLE_ID);

    ASSERT(queryCreate(CREATE_NODE_TABLE_IDX1_ID));
    if (!queryPrepare(CREATE_NODE_TABLE_IDX1_ID, CREATE_NODE_TABLE_IDX1, false, errId, error)) {
        queryFree(CREATE_NODE_TABLE_IDX1_ID);
        return sqlFail(CREATE_NODE_TABLE_IDX1_ID, error);
    }
    if (!queryExec(CREATE_NODE_TABLE_IDX1_ID, errId, error)) {
        queryFree(CREATE_NODE_TABLE_IDX1_ID);
        return sqlFail(CREATE_NODE_TABLE_IDX1_ID, error);
    }
    queryFree(CREATE_NODE_TABLE_IDX1_ID);

    ASSERT(queryCreate(CREATE_NODE_TABLE_IDX2_ID));
    if (!queryPrepare(CREATE_NODE_TABLE_IDX2_ID, CREATE_NODE_TABLE_IDX2, false, errId, error)) {
        queryFree(CREATE_NODE_TABLE_IDX2_ID);
        return sqlFail(CREATE_NODE_TABLE_IDX2_ID, error);
    }
    if (!queryExec(CREATE_NODE_TABLE_IDX2_ID, errId, error)) {
        queryFree(CREATE_NODE_TABLE_IDX2_ID);
        return sqlFail(CREATE_NODE_TABLE_IDX2_ID, error);
    }
    queryFree(CREATE_NODE_TABLE_IDX2_ID);

    ASSERT(queryCreate(CREATE_NODE_TABLE_IDX3_ID));
    if (!queryPrepare(CREATE_NODE_TABLE_IDX3_ID, CREATE_NODE_TABLE_IDX3, false, errId, error)) {
        queryFree(CREATE_NODE_TABLE_IDX3_ID);
        return sqlFail(CREATE_NODE_TABLE_IDX3_ID, error);
    }
    if (!queryExec(CREATE_NODE_TABLE_IDX3_ID, errId, error)) {
        queryFree(CREATE_NODE_TABLE_IDX3_ID);
        return sqlFail(CREATE_NODE_TABLE_IDX3_ID, error);
    }
    queryFree(CREATE_NODE_TABLE_IDX3_ID);

    ASSERT(queryCreate(CREATE_NODE_TABLE_IDX4_ID));
    if (!queryPrepare(CREATE_NODE_TABLE_IDX4_ID, CREATE_NODE_TABLE_IDX4, false, errId, error)) {
        queryFree(CREATE_NODE_TABLE_IDX4_ID);
        return sqlFail(CREATE_NODE_TABLE_IDX4_ID, error);
    }
    if (!queryExec(CREATE_NODE_TABLE_IDX4_ID, errId, error)) {
        queryFree(CREATE_NODE_TABLE_IDX4_ID);
        return sqlFail(CREATE_NODE_TABLE_IDX4_ID, error);
    }
    queryFree(CREATE_NODE_TABLE_IDX4_ID);

    ASSERT(queryCreate(CREATE_NODE_TABLE_IDX5_ID));
    if (!queryPrepare(CREATE_NODE_TABLE_IDX5_ID, CREATE_NODE_TABLE_IDX5, false, errId, error)) {
        queryFree(CREATE_NODE_TABLE_IDX5_ID);
        return sqlFail(CREATE_NODE_TABLE_IDX5_ID, error);
    }
    if (!queryExec(CREATE_NODE_TABLE_IDX5_ID, errId, error)) {
        queryFree(CREATE_NODE_TABLE_IDX5_ID);
        return sqlFail(CREATE_NODE_TABLE_IDX5_ID, error);
    }
    queryFree(CREATE_NODE_TABLE_IDX5_ID);

    // Sync Node
    ASSERT(queryCreate(CREATE_SYNC_NODE_TABLE_ID));
    if (!queryPrepare(CREATE_SYNC_NODE_TABLE_ID, CREATE_SYNC_NODE_TABLE, false, errId, error)) {
        queryFree(CREATE_SYNC_NODE_TABLE_ID);
        return sqlFail(CREATE_SYNC_NODE_TABLE_ID, error);
    }
    if (!queryExec(CREATE_SYNC_NODE_TABLE_ID, errId, error)) {
        queryFree(CREATE_SYNC_NODE_TABLE_ID);
        return sqlFail(CREATE_SYNC_NODE_TABLE_ID, error);
    }
    queryFree(CREATE_SYNC_NODE_TABLE_ID);

    // Upload session token table
    ASSERT(queryCreate(CREATE_UPLOAD_SESSION_TOKEN_TABLE_ID));
    if (!queryPrepare(CREATE_UPLOAD_SESSION_TOKEN_TABLE_ID, CREATE_UPLOAD_SESSION_TOKEN_TABLE, false, errId, error)) {
        queryFree(CREATE_UPLOAD_SESSION_TOKEN_TABLE_ID);
        return sqlFail(CREATE_UPLOAD_SESSION_TOKEN_TABLE_ID, error);
    }
    if (!queryExec(CREATE_UPLOAD_SESSION_TOKEN_TABLE_ID, errId, error)) {
        queryFree(CREATE_UPLOAD_SESSION_TOKEN_TABLE_ID);
        return sqlFail(CREATE_UPLOAD_SESSION_TOKEN_TABLE_ID, error);
    }
    queryFree(CREATE_UPLOAD_SESSION_TOKEN_TABLE_ID);

    return true;
}

bool SyncDb::prepare() {
    int errId;
    std::string error;

    // Node
    ASSERT(queryCreate(INSERT_NODE_REQUEST_ID));
    if (!queryPrepare(INSERT_NODE_REQUEST_ID, INSERT_NODE_REQUEST, false, errId, error)) {
        queryFree(INSERT_NODE_REQUEST_ID);
        return sqlFail(INSERT_NODE_REQUEST_ID, error);
    }

    ASSERT(queryCreate(UPDATE_NODE_REQUEST_ID));
    if (!queryPrepare(UPDATE_NODE_REQUEST_ID, UPDATE_NODE_REQUEST, false, errId, error)) {
        queryFree(UPDATE_NODE_REQUEST_ID);
        return sqlFail(UPDATE_NODE_REQUEST_ID, error);
    }

    ASSERT(queryCreate(UPDATE_NODE_STATUS_REQUEST_ID));
    if (!queryPrepare(UPDATE_NODE_STATUS_REQUEST_ID, UPDATE_NODE_STATUS_REQUEST, false, errId, error)) {
        queryFree(UPDATE_NODE_STATUS_REQUEST_ID);
        return sqlFail(UPDATE_NODE_STATUS_REQUEST_ID, error);
    }

    ASSERT(queryCreate(UPDATE_NODES_SYNCING_REQUEST_ID));
    if (!queryPrepare(UPDATE_NODES_SYNCING_REQUEST_ID, UPDATE_NODE_SYNCING_REQUEST, false, errId, error)) {
        queryFree(UPDATE_NODES_SYNCING_REQUEST_ID);
        return sqlFail(UPDATE_NODES_SYNCING_REQUEST_ID, error);
    }

    ASSERT(queryCreate(UPDATE_NODE_SYNCING_REQUEST_ID));
    if (!queryPrepare(UPDATE_NODE_SYNCING_REQUEST_ID, UPDATE_NODE_SYNCING_REQUEST, false, errId, error)) {
        queryFree(UPDATE_NODE_SYNCING_REQUEST_ID);
        return sqlFail(UPDATE_NODE_SYNCING_REQUEST_ID, error);
    }

    ASSERT(queryCreate(DELETE_NODE_REQUEST_ID));
    if (!queryPrepare(DELETE_NODE_REQUEST_ID, DELETE_NODE_REQUEST, false, errId, error)) {
        queryFree(DELETE_NODE_REQUEST_ID);
        return sqlFail(DELETE_NODE_REQUEST_ID, error);
    }

    ASSERT(queryCreate(DELETE_NODES_BUT_ROOT_REQUEST_ID));
    if (!queryPrepare(DELETE_NODES_BUT_ROOT_REQUEST_ID, DELETE_NODES_BUT_ROOT_REQUEST, false, errId, error)) {
        queryFree(DELETE_NODES_BUT_ROOT_REQUEST_ID);
        return sqlFail(DELETE_NODES_BUT_ROOT_REQUEST_ID, error);
    }

    ASSERT(queryCreate(DELETE_NODES_WITH_NULL_PARENTNODEID_REQUEST_ID));
    if (!queryPrepare(DELETE_NODES_WITH_NULL_PARENTNODEID_REQUEST_ID, DELETE_NODES_WITH_NULL_PARENTNODEID_REQUEST, false, errId,
                      error)) {
        queryFree(DELETE_NODES_WITH_NULL_PARENTNODEID_REQUEST_ID);
        return sqlFail(DELETE_NODES_WITH_NULL_PARENTNODEID_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_NODE_BY_NODEID_LITE_ID));
    if (!queryPrepare(SELECT_NODE_BY_NODEID_LITE_ID, SELECT_NODE_BY_NODEID_LITE, false, errId, error)) {
        queryFree(SELECT_NODE_BY_NODEID_LITE_ID);
        return sqlFail(SELECT_NODE_BY_NODEID_LITE_ID, error);
    }

    ASSERT(queryCreate(SELECT_NODE_BY_NODEID_FULL_ID));
    if (!queryPrepare(SELECT_NODE_BY_NODEID_FULL_ID, SELECT_NODE_BY_NODEID_FULL, false, errId, error)) {
        queryFree(SELECT_NODE_BY_NODEID_FULL_ID);
        return sqlFail(SELECT_NODE_BY_NODEID_FULL_ID, error);
    }

    ASSERT(queryCreate(SELECT_NODE_BY_NODEIDLOCAL_ID));
    if (!queryPrepare(SELECT_NODE_BY_NODEIDLOCAL_ID, SELECT_NODE_BY_NODEIDLOCAL, false, errId, error)) {
        queryFree(SELECT_NODE_BY_NODEIDLOCAL_ID);
        return sqlFail(SELECT_NODE_BY_NODEIDLOCAL_ID, error);
    }

    ASSERT(queryCreate(SELECT_NODE_BY_NODEIDDRIVE_ID));
    if (!queryPrepare(SELECT_NODE_BY_NODEIDDRIVE_ID, SELECT_NODE_BY_NODEIDDRIVE, false, errId, error)) {
        queryFree(SELECT_NODE_BY_NODEIDDRIVE_ID);
        return sqlFail(SELECT_NODE_BY_NODEIDDRIVE_ID, error);
    }

    ASSERT(queryCreate(SELECT_NODE_BY_PARENTNODEID_AND_NAMELOCAL_REQUEST_ID));
    if (!queryPrepare(SELECT_NODE_BY_PARENTNODEID_AND_NAMELOCAL_REQUEST_ID, SELECT_NODE_BY_PARENTNODEID_AND_NAMELOCAL_REQUEST,
                      false, errId, error)) {
        queryFree(SELECT_NODE_BY_PARENTNODEID_AND_NAMELOCAL_REQUEST_ID);
        return sqlFail(SELECT_NODE_BY_PARENTNODEID_AND_NAMELOCAL_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_NODE_BY_PARENTNODEID_AND_NAMEDRIVE_REQUEST_ID));
    if (!queryPrepare(SELECT_NODE_BY_PARENTNODEID_AND_NAMEDRIVE_REQUEST_ID, SELECT_NODE_BY_PARENTNODEID_AND_NAMEDRIVE_REQUEST,
                      false, errId, error)) {
        queryFree(SELECT_NODE_BY_PARENTNODEID_AND_NAMEDRIVE_REQUEST_ID);
        return sqlFail(SELECT_NODE_BY_PARENTNODEID_AND_NAMEDRIVE_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID));
    if (!queryPrepare(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, SELECT_NODE_BY_PARENTNODEID_REQUEST, false, errId, error)) {
        queryFree(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID);
        return sqlFail(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID));
    if (!queryPrepare(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST, false, errId,
                      error)) {
        queryFree(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID);
        return sqlFail(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_NODE_STATUS_BY_NODEID_REQUEST_ID));
    if (!queryPrepare(SELECT_NODE_STATUS_BY_NODEID_REQUEST_ID, SELECT_NODE_STATUS_BY_NODEID_REQUEST, false, errId, error)) {
        queryFree(SELECT_NODE_STATUS_BY_NODEID_REQUEST_ID);
        return sqlFail(SELECT_NODE_STATUS_BY_NODEID_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_NODE_SYNCING_BY_NODEID_REQUEST_ID));
    if (!queryPrepare(SELECT_NODE_SYNCING_BY_NODEID_REQUEST_ID, SELECT_NODE_SYNCING_BY_NODEID_REQUEST, false, errId, error)) {
        queryFree(SELECT_NODE_SYNCING_BY_NODEID_REQUEST_ID);
        return sqlFail(SELECT_NODE_SYNCING_BY_NODEID_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_ALL_RENAMED_COLON_NODES_REQUEST_ID));
    if (!queryPrepare(SELECT_ALL_RENAMED_COLON_NODES_REQUEST_ID, SELECT_ALL_RENAMED_COLON_NODES_REQUEST, false, errId, error)) {
        queryFree(SELECT_ALL_RENAMED_COLON_NODES_REQUEST_ID);
        return sqlFail(SELECT_ALL_RENAMED_COLON_NODES_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_ALL_RENAMED_NODES_REQUEST_ID));
    if (!queryPrepare(SELECT_ALL_RENAMED_NODES_REQUEST_ID, SELECT_ALL_RENAMED_NODES_REQUEST, false, errId, error)) {
        queryFree(SELECT_ALL_RENAMED_NODES_REQUEST_ID);
        return sqlFail(SELECT_ALL_RENAMED_NODES_REQUEST_ID, error);
    }

    // Sync Node
    ASSERT(queryCreate(INSERT_SYNC_NODE_REQUEST_ID));
    if (!queryPrepare(INSERT_SYNC_NODE_REQUEST_ID, INSERT_SYNC_NODE_REQUEST, false, errId, error)) {
        queryFree(INSERT_SYNC_NODE_REQUEST_ID);
        return sqlFail(INSERT_SYNC_NODE_REQUEST_ID, error);
    }

    ASSERT(queryCreate(DELETE_ALL_SYNC_NODE_BY_TYPE_REQUEST_ID));
    if (!queryPrepare(DELETE_ALL_SYNC_NODE_BY_TYPE_REQUEST_ID, DELETE_ALL_SYNC_NODE_BY_TYPE_REQUEST, false, errId, error)) {
        queryFree(DELETE_ALL_SYNC_NODE_BY_TYPE_REQUEST_ID);
        return sqlFail(DELETE_ALL_SYNC_NODE_BY_TYPE_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_ALL_SYNC_NODE_REQUEST_ID));
    if (!queryPrepare(SELECT_ALL_SYNC_NODE_REQUEST_ID, SELECT_ALL_SYNC_NODE_REQUEST, false, errId, error)) {
        queryFree(SELECT_ALL_SYNC_NODE_REQUEST_ID);
        return sqlFail(SELECT_ALL_SYNC_NODE_REQUEST_ID, error);
    }

    // Upload session token table
    ASSERT(queryCreate(INSERT_UPLOAD_SESSION_TOKEN_REQUEST_ID));
    if (!queryPrepare(INSERT_UPLOAD_SESSION_TOKEN_REQUEST_ID, INSERT_UPLOAD_SESSION_TOKEN_REQUEST, false, errId, error)) {
        queryFree(INSERT_UPLOAD_SESSION_TOKEN_REQUEST_ID);
        return sqlFail(INSERT_UPLOAD_SESSION_TOKEN_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_ALL_UPLOAD_SESSION_TOKEN_REQUEST_ID));
    if (!queryPrepare(SELECT_ALL_UPLOAD_SESSION_TOKEN_REQUEST_ID, SELECT_ALL_UPLOAD_SESSION_TOKEN_REQUEST, false, errId, error)) {
        queryFree(SELECT_ALL_UPLOAD_SESSION_TOKEN_REQUEST_ID);
        return sqlFail(SELECT_ALL_UPLOAD_SESSION_TOKEN_REQUEST_ID, error);
    }

    ASSERT(queryCreate(DELETE_UPLOAD_SESSION_TOKEN_BY_DBID_REQUEST_ID));
    if (!queryPrepare(DELETE_UPLOAD_SESSION_TOKEN_BY_DBID_REQUEST_ID, DELETE_UPLOAD_SESSION_TOKEN_BY_DBID_REQUEST, false, errId,
                      error)) {
        queryFree(DELETE_UPLOAD_SESSION_TOKEN_BY_DBID_REQUEST_ID);
        return sqlFail(DELETE_UPLOAD_SESSION_TOKEN_BY_DBID_REQUEST_ID, error);
    }

    ASSERT(queryCreate(DELETE_ALL_UPLOAD_SESSION_TOKEN_REQUEST_ID));
    if (!queryPrepare(DELETE_ALL_UPLOAD_SESSION_TOKEN_REQUEST_ID, DELETE_ALL_UPLOAD_SESSION_TOKEN_REQUEST, false, errId, error)) {
        queryFree(DELETE_ALL_UPLOAD_SESSION_TOKEN_REQUEST_ID);
        return sqlFail(DELETE_ALL_UPLOAD_SESSION_TOKEN_REQUEST_ID, error);
    }

    if (!initData()) {
        LOG_WARN(_logger, "Error in initParameters");
        return false;
    }

    return true;
}

bool SyncDb::upgrade(const std::string &fromVersion, const std::string & /*toVersion*/) {
    int errId;
    std::string error;

    std::string dbFromVersionNumber = CommonUtility::dbVersionNumber(fromVersion);
    if (dbFromVersionNumber == "3.4.0") {
        LOG_DEBUG(_logger, "Upgrade 3.4.0 DB");

        // Upload session token table
        ASSERT(queryCreate(CREATE_UPLOAD_SESSION_TOKEN_TABLE_ID));
        if (!queryPrepare(CREATE_UPLOAD_SESSION_TOKEN_TABLE_ID, CREATE_UPLOAD_SESSION_TOKEN_TABLE, false, errId, error)) {
            queryFree(CREATE_UPLOAD_SESSION_TOKEN_TABLE_ID);
            return sqlFail(CREATE_UPLOAD_SESSION_TOKEN_TABLE_ID, error);
        }
        if (!queryExec(CREATE_UPLOAD_SESSION_TOKEN_TABLE_ID, errId, error)) {
            queryFree(CREATE_UPLOAD_SESSION_TOKEN_TABLE_ID);
            return sqlFail(CREATE_UPLOAD_SESSION_TOKEN_TABLE_ID, error);
        }
        queryFree(CREATE_UPLOAD_SESSION_TOKEN_TABLE_ID);
    }

    if (CommonUtility::isVersionLower(dbFromVersionNumber, "3.4.4")) {
        LOG_DEBUG(_logger, "Upgrade < 3.4.4 DB");

        ASSERT(queryCreate(PRAGMA_WRITABLE_SCHEMA_ID));
        if (!queryPrepare(PRAGMA_WRITABLE_SCHEMA_ID, PRAGMA_WRITABLE_SCHEMA, false, errId, error)) {
            queryFree(PRAGMA_WRITABLE_SCHEMA_ID);
            return sqlFail(PRAGMA_WRITABLE_SCHEMA_ID, error);
        }
        bool hasData;
        if (!queryNext(PRAGMA_WRITABLE_SCHEMA_ID, hasData)) {
            queryFree(PRAGMA_WRITABLE_SCHEMA_ID);
            return sqlFail(PRAGMA_WRITABLE_SCHEMA_ID, error);
        }
        queryFree(PRAGMA_WRITABLE_SCHEMA_ID);

        ASSERT(queryCreate(ALTER_NODE_TABLE_FK_ID));
        if (!queryPrepare(ALTER_NODE_TABLE_FK_ID, ALTER_NODE_TABLE_FK, false, errId, error)) {
            queryFree(ALTER_NODE_TABLE_FK_ID);
            return sqlFail(ALTER_NODE_TABLE_FK_ID, error);
        }
        if (!queryExec(ALTER_NODE_TABLE_FK_ID, errId, error)) {
            queryFree(ALTER_NODE_TABLE_FK_ID);
            return sqlFail(ALTER_NODE_TABLE_FK_ID, error);
        }
        queryFree(ALTER_NODE_TABLE_FK_ID);
    }

    return true;
}

bool SyncDb::initData() {
    // Try to get root node dbId
    DbNodeId dbNodeId;
    bool found;
    if (!dbId(ReplicaSide::Local, _rootNode.nodeIdLocal().value(), dbNodeId, found)) {
        LOG_WARN(_logger, "Error in dbId");
        return false;
    }
    if (!found) {
        // Insert root node
        bool constraintError = false;
        if (!insertNode(_rootNode, dbNodeId, constraintError)) {
            LOG_WARN(_logger, "Error in insertNode");
            return false;
        }
    }

    _rootNode.setNodeId(dbNodeId);

    return true;
}

bool SyncDb::insertNode(const DbNode &node, DbNodeId &dbNodeId, bool &constraintError) {
    const std::lock_guard<std::mutex> lock(_mutex);

    if ((node.nodeIdLocal() ? node.nodeIdLocal().value() : "").empty() ||
        (node.nodeIdRemote() ? node.nodeIdRemote().value() : "").empty()) {
        LOG_WARN(_logger, "nodeId cannot be empty");
        return false;
    }

    if (node.nodeId() != _rootNode.nodeId() && !node.parentNodeId().has_value()) {
        LOG_WARN(_logger, "parentNodeId cannot be empty");
        return false;
    }

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(INSERT_NODE_REQUEST_ID));
    ASSERT(queryBindValue(INSERT_NODE_REQUEST_ID, 1,
                          (node.parentNodeId() ? dbtype(node.parentNodeId().value()) : std::monostate())));
    ASSERT(queryBindValue(INSERT_NODE_REQUEST_ID, 2, node.nameLocal()));
    ASSERT(queryBindValue(INSERT_NODE_REQUEST_ID, 3, node.nameRemote()));
    ASSERT(
        queryBindValue(INSERT_NODE_REQUEST_ID, 4, (node.nodeIdLocal() ? dbtype(node.nodeIdLocal().value()) : std::monostate())));
    ASSERT(queryBindValue(INSERT_NODE_REQUEST_ID, 5,
                          (node.nodeIdRemote() ? dbtype(node.nodeIdRemote().value()) : std::monostate())));
    ASSERT(queryBindValue(INSERT_NODE_REQUEST_ID, 6, (node.created() ? dbtype(node.created().value()) : std::monostate())));
    ASSERT(queryBindValue(INSERT_NODE_REQUEST_ID, 7,
                          (node.lastModifiedLocal() ? dbtype(node.lastModifiedLocal().value()) : std::monostate())));
    ASSERT(queryBindValue(INSERT_NODE_REQUEST_ID, 8,
                          (node.lastModifiedRemote() ? dbtype(node.lastModifiedRemote().value()) : std::monostate())));
    ASSERT(queryBindValue(INSERT_NODE_REQUEST_ID, 9, static_cast<int>(node.type())));
    ASSERT(queryBindValue(INSERT_NODE_REQUEST_ID, 10, node.size()));
    ASSERT(queryBindValue(INSERT_NODE_REQUEST_ID, 11, (node.checksum() ? dbtype(node.checksum().value()) : std::monostate())));
    ASSERT(queryBindValue(INSERT_NODE_REQUEST_ID, 12, static_cast<int>(node.status())));
    ASSERT(queryBindValue(INSERT_NODE_REQUEST_ID, 13, static_cast<int>(node.syncing())));
    if (!queryExecAndGetRowId(INSERT_NODE_REQUEST_ID, dbNodeId, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << INSERT_NODE_REQUEST_ID);
        constraintError = (errId == SQLITE_CONSTRAINT);
        return false;
    }

    return true;
}

bool SyncDb::updateNode(const DbNode &node, bool &found) {
    const std::lock_guard<std::mutex> lock(_mutex);

    if ((node.nodeIdLocal() ? node.nodeIdLocal().value() : "").empty() ||
        (node.nodeIdRemote() ? node.nodeIdRemote().value() : "").empty()) {
        LOG_WARN(_logger, "nodeId cannot be empty");
        return false;
    }

    if (node.nodeId() != _rootNode.nodeId() && !node.parentNodeId().has_value()) {
        LOG_WARN(_logger, "parentNodeId cannot be empty");
        return false;
    }

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(UPDATE_NODE_REQUEST_ID));
    ASSERT(queryBindValue(UPDATE_NODE_REQUEST_ID, 1,
                          (node.parentNodeId() ? dbtype(node.parentNodeId().value()) : std::monostate())));
    ASSERT(queryBindValue(UPDATE_NODE_REQUEST_ID, 2, node.nameLocal()));
    ASSERT(queryBindValue(UPDATE_NODE_REQUEST_ID, 3, node.nameRemote()));
    ASSERT(
        queryBindValue(UPDATE_NODE_REQUEST_ID, 4, (node.nodeIdLocal() ? dbtype(node.nodeIdLocal().value()) : std::monostate())));
    ASSERT(queryBindValue(UPDATE_NODE_REQUEST_ID, 5,
                          (node.nodeIdRemote() ? dbtype(node.nodeIdRemote().value()) : std::monostate())));
    ASSERT(queryBindValue(UPDATE_NODE_REQUEST_ID, 6, (node.created() ? dbtype(node.created().value()) : std::monostate())));
    ASSERT(queryBindValue(UPDATE_NODE_REQUEST_ID, 7,
                          (node.lastModifiedLocal() ? dbtype(node.lastModifiedLocal().value()) : std::monostate())));
    ASSERT(queryBindValue(UPDATE_NODE_REQUEST_ID, 8,
                          (node.lastModifiedRemote() ? dbtype(node.lastModifiedRemote().value()) : std::monostate())));
    ASSERT(queryBindValue(UPDATE_NODE_REQUEST_ID, 9, static_cast<int>(node.type())));
    ASSERT(queryBindValue(UPDATE_NODE_REQUEST_ID, 10, node.size()));
    ASSERT(queryBindValue(UPDATE_NODE_REQUEST_ID, 11, (node.checksum() ? dbtype(node.checksum().value()) : std::monostate())));
    ASSERT(queryBindValue(UPDATE_NODE_REQUEST_ID, 12, static_cast<int>(node.status())));
    ASSERT(queryBindValue(UPDATE_NODE_REQUEST_ID, 13, static_cast<int>(node.syncing())));
    ASSERT(queryBindValue(UPDATE_NODE_REQUEST_ID, 14, node.nodeId()));
    if (!queryExec(UPDATE_NODE_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << UPDATE_NODE_REQUEST_ID);
        return false;
    }
    if (numRowsAffected() == 1) {
        found = true;
    } else {
        LOG_WARN(_logger, "Error running query: " << UPDATE_NODE_REQUEST_ID << " - num rows affected != 1");
        found = false;
    }

    return true;
}

bool SyncDb::updateNodeStatus(DbNodeId nodeId, SyncFileStatus status, bool &found) {
    const std::lock_guard<std::mutex> lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(UPDATE_NODE_STATUS_REQUEST_ID));
    ASSERT(queryBindValue(UPDATE_NODE_STATUS_REQUEST_ID, 1, static_cast<int>(status)));
    ASSERT(queryBindValue(UPDATE_NODE_STATUS_REQUEST_ID, 2, nodeId));
    if (!queryExec(UPDATE_NODE_STATUS_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << UPDATE_NODE_STATUS_REQUEST_ID);
        return false;
    }
    if (numRowsAffected() == 1) {
        found = true;
    } else {
        LOG_WARN(_logger, "Error running query: " << UPDATE_NODE_STATUS_REQUEST_ID << " - num rows affected != 1");
        found = false;
    }

    return true;
}

bool SyncDb::updateNodesSyncing(bool syncing) {
    const std::lock_guard<std::mutex> lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(UPDATE_NODES_SYNCING_REQUEST_ID));
    ASSERT(queryBindValue(UPDATE_NODES_SYNCING_REQUEST_ID, 1, static_cast<int>(syncing)));
    if (!queryExec(UPDATE_NODES_SYNCING_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << UPDATE_NODES_SYNCING_REQUEST_ID);
        return false;
    }

    return true;
}

bool SyncDb::updateNodeSyncing(DbNodeId nodeId, bool syncing, bool &found) {
    const std::lock_guard<std::mutex> lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(UPDATE_NODE_SYNCING_REQUEST_ID));
    ASSERT(queryBindValue(UPDATE_NODE_SYNCING_REQUEST_ID, 1, static_cast<int>(syncing)));
    ASSERT(queryBindValue(UPDATE_NODE_SYNCING_REQUEST_ID, 2, nodeId));
    if (!queryExec(UPDATE_NODE_SYNCING_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << UPDATE_NODE_SYNCING_REQUEST_ID);
        return false;
    }
    if (numRowsAffected() == 1) {
        found = true;
    } else {
        LOG_WARN(_logger, "Error running query: " << UPDATE_NODE_SYNCING_REQUEST_ID << " - num rows affected != 1");
        found = false;
    }

    return true;
}

bool SyncDb::deleteNode(DbNodeId nodeId, bool &found) {
    const std::lock_guard<std::mutex> lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(DELETE_NODE_REQUEST_ID));
    ASSERT(queryBindValue(DELETE_NODE_REQUEST_ID, 1, nodeId));
    if (!queryExec(DELETE_NODE_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << DELETE_NODE_REQUEST_ID);
        return false;
    }
    if (numRowsAffected() == 1) {
        found = true;
    } else {
        LOG_WARN(_logger, "Error running query: " << DELETE_NODE_REQUEST_ID << " - num rows affected != 1");
        found = false;
    }

    return true;
}

bool SyncDb::status(ReplicaSide side, const SyncPath &path, SyncFileStatus &status, bool &found) {
    DbNodeId dbNodeId;
    if (!dbId(side, path, dbNodeId, found)) {
        return false;
    }
    if (!found) {
        return true;
    }

    if (!selectStatus(dbNodeId, status, found)) {
        return false;
    }

    return true;
}

bool SyncDb::status(ReplicaSide side, const NodeId &nodeId, SyncFileStatus &status, bool &found) {
    DbNodeId dbNodeId;
    if (!dbId(side, nodeId, dbNodeId, found)) {
        return false;
    }
    if (!found) {
        return true;
    }

    if (!selectStatus(dbNodeId, status, found)) {
        return false;
    }

    return true;
}

bool SyncDb::selectStatus(DbNodeId nodeId, SyncFileStatus &status, bool &found) {
    const std::lock_guard<std::mutex> lock(_mutex);

    ASSERT(queryResetAndClearBindings(SELECT_NODE_STATUS_BY_NODEID_REQUEST_ID));
    ASSERT(queryBindValue(SELECT_NODE_STATUS_BY_NODEID_REQUEST_ID, 1, nodeId));
    if (!queryNext(SELECT_NODE_STATUS_BY_NODEID_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_NODE_STATUS_BY_NODEID_REQUEST_ID);
        return false;
    }
    if (!found) {
        return true;
    }

    int intResult;
    ASSERT(queryIntValue(SELECT_NODE_STATUS_BY_NODEID_REQUEST_ID, 0, intResult));
    status = static_cast<SyncFileStatus>(intResult);

    ASSERT(queryResetAndClearBindings(SELECT_NODE_STATUS_BY_NODEID_REQUEST_ID));

    return true;
}

bool SyncDb::setStatus(ReplicaSide side, const SyncPath &path, SyncFileStatus status, bool &found) {
    DbNodeId dbNodeId;
    if (!dbId(side, path, dbNodeId, found)) {
        return false;
    }
    if (!found) {
        return true;
    }

    if (!updateNodeStatus(dbNodeId, status, found)) {
        return false;
    }

    return true;
}

bool SyncDb::setStatus(ReplicaSide side, const NodeId &nodeId, SyncFileStatus status, bool &found) {
    DbNodeId dbNodeId;
    if (!dbId(side, nodeId, dbNodeId, found)) {
        return false;
    }
    if (!found) {
        return true;
    }

    if (!updateNodeStatus(dbNodeId, status, found)) {
        return false;
    }

    return true;
}

bool SyncDb::syncing(ReplicaSide side, const SyncPath &path, bool &syncing, bool &found) {
    DbNodeId dbNodeId;
    if (!dbId(side, path, dbNodeId, found)) {
        return false;
    }
    if (!found) {
        return true;
    }

    if (!selectSyncing(dbNodeId, syncing, found)) {
        return false;
    }

    return true;
}

bool SyncDb::selectSyncing(DbNodeId nodeId, bool &syncing, bool &found) {
    const std::lock_guard<std::mutex> lock(_mutex);

    ASSERT(queryResetAndClearBindings(SELECT_NODE_SYNCING_BY_NODEID_REQUEST_ID));
    ASSERT(queryBindValue(SELECT_NODE_SYNCING_BY_NODEID_REQUEST_ID, 1, nodeId));
    if (!queryNext(SELECT_NODE_SYNCING_BY_NODEID_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_NODE_SYNCING_BY_NODEID_REQUEST_ID);
        return false;
    }
    if (!found) {
        return true;
    }

    int intResult;
    ASSERT(queryIntValue(SELECT_NODE_SYNCING_BY_NODEID_REQUEST_ID, 0, intResult));
    syncing = static_cast<bool>(intResult);

    ASSERT(queryResetAndClearBindings(SELECT_NODE_SYNCING_BY_NODEID_REQUEST_ID));

    return true;
}

bool SyncDb::setSyncing(ReplicaSide side, const SyncPath &path, bool syncing, bool &found) {
    DbNodeId dbNodeId;
    if (!dbId(side, path, dbNodeId, found)) {
        return false;
    }
    if (!found) {
        return true;
    }

    if (!updateNodeSyncing(dbNodeId, syncing, found)) {
        return false;
    }

    return true;
}

bool SyncDb::node(ReplicaSide snapshot, const NodeId &nodeId, DbNode &dbNode, bool &found) {
    const std::lock_guard<std::mutex> lock(_mutex);

    std::string id = (snapshot == ReplicaSide::Local ? SELECT_NODE_BY_NODEIDLOCAL_ID : SELECT_NODE_BY_NODEIDDRIVE_ID);
    ASSERT(queryResetAndClearBindings(id));
    ASSERT(queryBindValue(id, 1, nodeId));
    if (!queryNext(id, found)) {
        LOG_WARN(_logger, "Error getting query result: "
                              << id.c_str() << (snapshot == ReplicaSide::Local ? " - nodeIdLocal=" : " - nodeIdDrive=")
                              << nodeId.c_str());
        return false;
    }
    if (!found) {
        return true;
    }

    DbNodeId dbNodeId;
    ASSERT(queryInt64Value(id, SELECT_NODE_BY_REPLICAID_DBID, dbNodeId));

    bool ok;
    std::optional<DbNodeId> parentNodeId;
    ASSERT(queryIsNullValue(id, SELECT_NODE_BY_REPLICAID_PARENTID, ok));
    if (ok) {
        parentNodeId = std::nullopt;
    } else {
        DbNodeId dbParentNodeId;
        ASSERT(queryInt64Value(id, SELECT_NODE_BY_REPLICAID_PARENTID, dbParentNodeId));
        parentNodeId = std::make_optional(dbParentNodeId);
    }

    SyncName nameLocal;
    ASSERT(querySyncNameValue(id, SELECT_NODE_BY_REPLICAID_NAMELOCAL, nameLocal));
    SyncName nameDrive;
    ASSERT(querySyncNameValue(id, SELECT_NODE_BY_REPLICAID_NAMEDRIVE, nameDrive));

    std::optional<SyncTime> created;
    ASSERT(queryIsNullValue(id, SELECT_NODE_BY_REPLICAID_CREATED, ok));
    if (ok) {
        created = std::nullopt;
    } else {
        SyncTime timeTmp;
        ASSERT(queryInt64Value(id, SELECT_NODE_BY_REPLICAID_CREATED, timeTmp));
        created = std::make_optional(timeTmp);
    }

    std::optional<SyncTime> lastModified;
    ASSERT(queryIsNullValue(id, SELECT_NODE_BY_REPLICAID_LASTMOD, ok));
    if (ok) {
        lastModified = std::nullopt;
    } else {
        SyncTime timeTmp;
        ASSERT(queryInt64Value(id, SELECT_NODE_BY_REPLICAID_LASTMOD, timeTmp));
        lastModified = std::make_optional(timeTmp);
    }

    int intResult;
    ASSERT(queryIntValue(id, SELECT_NODE_BY_REPLICAID_TYPE, intResult));
    NodeType type = static_cast<NodeType>(intResult);

    int64_t size;
    ASSERT(queryInt64Value(id, SELECT_NODE_BY_REPLICAID_SIZE, size));

    std::optional<std::string> cs;
    ASSERT(queryIsNullValue(id, SELECT_NODE_BY_REPLICAID_CHECKSUM, ok));
    if (ok) {
        cs = std::nullopt;
    } else {
        std::string csTmp;
        ASSERT(queryStringValue(id, SELECT_NODE_BY_REPLICAID_CHECKSUM, csTmp));
        cs = std::make_optional(csTmp);
    }

    ASSERT(queryIntValue(id, SELECT_NODE_BY_REPLICAID_STATUS, intResult));
    SyncFileStatus status = static_cast<SyncFileStatus>(intResult);

    ASSERT(queryIntValue(id, SELECT_NODE_BY_REPLICAID_SYNCING, intResult));
    bool syncing = static_cast<bool>(intResult);

    ASSERT(queryResetAndClearBindings(id));

    dbNode.setNodeId(dbNodeId);
    dbNode.setParentNodeId(parentNodeId);
    dbNode.setNameLocal(nameLocal);
    dbNode.setNameRemote(nameDrive);
    dbNode.setCreated(created);
    dbNode.setType(type);
    dbNode.setSize(size);
    if (snapshot == ReplicaSide::Local) {
        dbNode.setLastModifiedLocal(lastModified);
    } else {
        dbNode.setLastModifiedRemote(lastModified);
    }
    dbNode.setChecksum(cs);
    dbNode.setStatus(status);
    dbNode.setSyncing(syncing);

    return true;
}

bool SyncDb::dbIds(std::unordered_set<DbNodeId> &ids, bool &found) {
    const std::lock_guard<std::mutex> lock(_mutex);

    // Find root node
    ASSERT(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID));
    if (!queryNext(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID);
        return false;
    }
    if (!found) {
        return true;
    }
    DbNodeId nodeDbId;
    ASSERT(queryInt64Value(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, 0, nodeDbId));
    ids.insert(nodeDbId);

    ASSERT(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID));

    // Push child nodes' ids
    try {
        if (!pushChildDbIds(nodeDbId, ids)) {
            return false;
        }
    } catch (...) {
        LOG_WARN(_logger, "Error in SyncDb::pushChildDbIds");
        return false;
    }

    return true;
}

bool SyncDb::path(DbNodeId dbNodeId, SyncPath &localPath, SyncPath &remotePath, bool &found) {
    const std::lock_guard<std::mutex> lock(_mutex);

    ASSERT(queryResetAndClearBindings(SELECT_NODE_BY_NODEID_LITE_ID));
    ASSERT(queryBindValue(SELECT_NODE_BY_NODEID_LITE_ID, 1, dbNodeId));
    if (!queryNext(SELECT_NODE_BY_NODEID_LITE_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_NODE_BY_NODEID_LITE_ID << " - nodeId=" << dbNodeId);
        return false;
    }
    if (!found) {
        return true;
    }

    // Fill names' vector
    std::vector<std::pair<SyncName, SyncName>> names;  // first: local names, second: drive names

    bool parentNodeDbIdIsNull;
    DbNodeId parentNodeDbId;
    ASSERT(queryIsNullValue(SELECT_NODE_BY_NODEID_LITE_ID, SELECT_NODE_BY_NODEID_PARENTID, parentNodeDbIdIsNull));
    if (!parentNodeDbIdIsNull) {
        ASSERT(queryInt64Value(SELECT_NODE_BY_NODEID_LITE_ID, SELECT_NODE_BY_NODEID_PARENTID, parentNodeDbId));

        SyncName localName;
        ASSERT(querySyncNameValue(SELECT_NODE_BY_NODEID_LITE_ID, SELECT_NODE_BY_NODEID_NAMELOCAL, localName));
        SyncName driveName;
        ASSERT(querySyncNameValue(SELECT_NODE_BY_NODEID_LITE_ID, SELECT_NODE_BY_NODEID_NAMEDRIVE, driveName));
        names.push_back({localName, driveName});

        while (!parentNodeDbIdIsNull) {
            ASSERT(queryResetAndClearBindings(SELECT_NODE_BY_NODEID_LITE_ID));
            ASSERT(queryBindValue(SELECT_NODE_BY_NODEID_LITE_ID, 1, parentNodeDbId));
            if (!queryNext(SELECT_NODE_BY_NODEID_LITE_ID, found)) {
                LOG_WARN(_logger, "Error getting query result: " << SELECT_NODE_BY_NODEID_LITE_ID << " - nodeId=" << dbNodeId);
                return false;
            }
            if (!found) {
                return true;
            }

            ASSERT(queryIsNullValue(SELECT_NODE_BY_NODEID_LITE_ID, SELECT_NODE_BY_NODEID_PARENTID, parentNodeDbIdIsNull));
            if (!parentNodeDbIdIsNull) {
                ASSERT(queryInt64Value(SELECT_NODE_BY_NODEID_LITE_ID, SELECT_NODE_BY_NODEID_PARENTID, parentNodeDbId));

                ASSERT(querySyncNameValue(SELECT_NODE_BY_NODEID_LITE_ID, SELECT_NODE_BY_NODEID_NAMELOCAL, localName));
                ASSERT(querySyncNameValue(SELECT_NODE_BY_NODEID_LITE_ID, SELECT_NODE_BY_NODEID_NAMEDRIVE, driveName));
                names.push_back({localName, driveName});
            }
        }
    }

    ASSERT(queryResetAndClearBindings(SELECT_NODE_BY_NODEID_LITE_ID));

    // Construct path from names' vector
    localPath.clear();
    remotePath.clear();
    for (auto nameIt = names.rbegin(); nameIt != names.rend(); ++nameIt) {
        localPath.append(nameIt->first);
        remotePath.append(nameIt->second);
    }

    return true;
}

bool SyncDb::node(DbNodeId dbNodeId, DbNode &dbNode, bool &found) {
    const std::lock_guard<std::mutex> lock(_mutex);

    std::string id = SELECT_NODE_BY_NODEID_FULL_ID;
    ASSERT(queryResetAndClearBindings(id));
    ASSERT(queryBindValue(id, 1, dbNodeId));
    if (!queryNext(id, found)) {
        LOG_WARN(_logger, "Error getting query result: " << id.c_str() << " - dbNodeId=" << dbNodeId);
        return false;
    }
    if (!found) {
        return true;
    }

    bool ok;
    std::optional<DbNodeId> parentNodeId;
    ASSERT(queryIsNullValue(id, SELECT_NODE_BY_NODEID_PARENTID, ok));
    if (ok) {
        parentNodeId = std::nullopt;
    } else {
        DbNodeId dbParentNodeId;
        ASSERT(queryInt64Value(id, SELECT_NODE_BY_NODEID_PARENTID, dbParentNodeId));
        parentNodeId = std::make_optional(dbParentNodeId);
    }

    SyncName nameLocal;
    ASSERT(querySyncNameValue(id, SELECT_NODE_BY_NODEID_NAMELOCAL, nameLocal));

    SyncName nameDrive;
    ASSERT(querySyncNameValue(id, SELECT_NODE_BY_NODEID_NAMEDRIVE, nameDrive));

    NodeId nodeIdLocal;
    ASSERT(queryStringValue(id, SELECT_NODE_BY_NODEID_IDLOCAL, nodeIdLocal));

    NodeId nodeIdDrive;
    ASSERT(queryStringValue(id, SELECT_NODE_BY_NODEID_IDDRIVE, nodeIdDrive));

    std::optional<SyncTime> created;
    ASSERT(queryIsNullValue(id, SELECT_NODE_BY_NODEID_CREATED, ok));
    if (ok) {
        created = std::nullopt;
    } else {
        SyncTime timeTmp;
        ASSERT(queryInt64Value(id, SELECT_NODE_BY_NODEID_CREATED, timeTmp));
        created = std::make_optional(timeTmp);
    }

    std::optional<SyncTime> lastModifiedLocal;
    ASSERT(queryIsNullValue(id, SELECT_NODE_BY_NODEID_LASTMODLOCAL, ok));
    if (ok) {
        lastModifiedLocal = std::nullopt;
    } else {
        SyncTime timeTmp;
        ASSERT(queryInt64Value(id, SELECT_NODE_BY_NODEID_LASTMODLOCAL, timeTmp));
        lastModifiedLocal = std::make_optional(timeTmp);
    }

    std::optional<SyncTime> lastModifiedDrive;
    ASSERT(queryIsNullValue(id, SELECT_NODE_BY_NODEID_LASTMODDRIVE, ok));
    if (ok) {
        lastModifiedDrive = std::nullopt;
    } else {
        SyncTime timeTmp;
        ASSERT(queryInt64Value(id, SELECT_NODE_BY_NODEID_LASTMODDRIVE, timeTmp));
        lastModifiedDrive = std::make_optional(timeTmp);
    }

    int intResult;
    ASSERT(queryIntValue(id, SELECT_NODE_BY_NODEID_TYPE, intResult));
    NodeType type = static_cast<NodeType>(intResult);

    int64_t size;
    ASSERT(queryInt64Value(id, SELECT_NODE_BY_NODEID_SIZE, size));

    std::optional<std::string> cs;
    ASSERT(queryIsNullValue(id, SELECT_NODE_BY_NODEID_CHECKSUM, ok));
    if (ok) {
        cs = std::nullopt;
    } else {
        std::string csTmp;
        ASSERT(queryStringValue(id, SELECT_NODE_BY_NODEID_CHECKSUM, csTmp));
        cs = std::make_optional(csTmp);
    }

    ASSERT(queryIntValue(id, SELECT_NODE_BY_NODEID_STATUS, intResult));
    SyncFileStatus status = static_cast<SyncFileStatus>(intResult);

    ASSERT(queryIntValue(id, SELECT_NODE_BY_NODEID_SYNCING, intResult));
    bool syncing = static_cast<bool>(intResult);

    ASSERT(queryResetAndClearBindings(id));

    dbNode.setNodeId(dbNodeId);
    dbNode.setParentNodeId(parentNodeId);
    dbNode.setNodeIdLocal(nodeIdLocal);
    dbNode.setNodeIdRemote(nodeIdDrive);
    dbNode.setNameLocal(nameLocal);
    dbNode.setNameRemote(nameDrive);
    dbNode.setCreated(created);
    dbNode.setLastModifiedLocal(lastModifiedLocal);
    dbNode.setLastModifiedRemote(lastModifiedDrive);
    dbNode.setType(type);
    dbNode.setSize(size);
    dbNode.setChecksum(cs);
    dbNode.setStatus(status);
    dbNode.setSyncing(syncing);

    return true;
}

bool SyncDb::dbId(ReplicaSide side, const SyncPath &path, DbNodeId &dbNodeId, bool &found) {
    const std::lock_guard<std::mutex> lock(_mutex);

    // Split path
    std::vector<SyncName> names;
    SyncPath pathTmp(path);
    while (pathTmp != pathTmp.root_path()) {
        names.push_back(pathTmp.filename().native());
        pathTmp = pathTmp.parent_path();
    }

    // Find root node
    ASSERT(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID));
    if (!queryNext(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID);
        return false;
    }
    if (!found) {
        return true;
    }

    ASSERT(queryInt64Value(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, 0, dbNodeId));

    ASSERT(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID));

    if (names.size() > 0) {
        // Find file node
        std::string id = (side == ReplicaSide::Local ? SELECT_NODE_BY_PARENTNODEID_AND_NAMELOCAL_REQUEST_ID
                                                                : SELECT_NODE_BY_PARENTNODEID_AND_NAMEDRIVE_REQUEST_ID);
        for (std::vector<SyncName>::reverse_iterator nameIt = names.rbegin(); nameIt != names.rend(); ++nameIt) {
            ASSERT(queryResetAndClearBindings(id));
            ASSERT(queryBindValue(id, 1, dbNodeId));
            ASSERT(queryBindValue(id, 2, Utility::normalizedSyncName(*nameIt)));
            if (!queryNext(id, found)) {
                LOGW_WARN(_logger, L"Error getting query result: " << Utility::s2ws(id).c_str() << L" - parentNodeId="
                                                                   << std::to_wstring(dbNodeId).c_str() << L" and name="
                                                                   << (SyncName2WStr(*nameIt)).c_str());
                return false;
            }
            if (!found) {
                return true;
            }
            ASSERT(queryInt64Value(id, 0, dbNodeId));

            ASSERT(queryResetAndClearBindings(id));
        }
    }

    found = true;

    return true;
}

bool SyncDb::clearNodes() {
    const std::lock_guard<std::mutex> lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(DELETE_NODES_BUT_ROOT_REQUEST_ID));
    if (!queryExec(DELETE_NODES_BUT_ROOT_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << DELETE_NODES_BUT_ROOT_REQUEST_ID);
        return false;
    }

    return true;
}

// Returns the id of the object from its path
// path is relative to the root directory
bool SyncDb::id(ReplicaSide snapshot, const SyncPath &path, std::optional<NodeId> &nodeId, bool &found) {
    const std::lock_guard<std::mutex> lock(_mutex);

    // Split path
    std::vector<SyncName> names;
    SyncPath pathTmp(path);
    while (pathTmp != pathTmp.root_path()) {
        names.push_back(pathTmp.filename().native());
        pathTmp = pathTmp.parent_path();
    }

    // Find root node
    ASSERT(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID));
    if (!queryNext(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID);
        return false;
    }
    if (!found) {
        return true;
    }
    DbNodeId nodeDbId;
    ASSERT(queryInt64Value(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, 0, nodeDbId));

    if (names.size() > 0) {
        ASSERT(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID));

        std::string queryId = (snapshot == ReplicaSide::Local ? SELECT_NODE_BY_PARENTNODEID_AND_NAMELOCAL_REQUEST_ID
                                                                         : SELECT_NODE_BY_PARENTNODEID_AND_NAMEDRIVE_REQUEST_ID);
        // Find file node
        for (std::vector<SyncName>::reverse_iterator nameIt = names.rbegin(); nameIt != names.rend(); ++nameIt) {
            ASSERT(queryResetAndClearBindings(queryId));
            ASSERT(queryBindValue(queryId, 1, nodeDbId));
            ASSERT(queryBindValue(queryId, 2, Utility::normalizedSyncName(*nameIt)));
            if (!queryNext(queryId, found)) {
                LOGW_WARN(_logger, L"Error getting query result: " << queryId.c_str() << L" - parentNodeId=" << nodeDbId
                                                                   << L" and name=" << (SyncName2WStr(*nameIt)).c_str());
                return false;
            }
            if (!found) {
                return true;
            }
            ASSERT(queryInt64Value(queryId, 0, nodeDbId));
        }
        bool ok;
        ASSERT(queryIsNullValue(queryId, 1, ok));
        if (ok) {
            nodeId = std::nullopt;
        } else {
            NodeId idTmp;
            ASSERT(queryStringValue(queryId, 1, idTmp));
            nodeId = std::make_optional(idTmp);
        }

        ASSERT(queryResetAndClearBindings(queryId));
    } else {
        bool ok;
        ASSERT(queryIsNullValue(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, (snapshot == ReplicaSide::Local ? 3 : 4),
                                ok));
        if (ok) {
            nodeId = std::nullopt;
        } else {
            NodeId idTmp;
            ASSERT(queryStringValue(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID,
                                    (snapshot == ReplicaSide::Local ? 3 : 4), idTmp));
            nodeId = std::make_optional(idTmp);
        }

        ASSERT(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID));
    }

    found = true;
    return true;
}

// Returns the type of the object with ID nodeId
bool SyncDb::type(ReplicaSide snapshot, const NodeId &nodeId, NodeType &type, bool &found) {
    const std::lock_guard<std::mutex> lock(_mutex);

    std::string id = (snapshot == ReplicaSide::Local ? SELECT_NODE_BY_NODEIDLOCAL_ID : SELECT_NODE_BY_NODEIDDRIVE_ID);
    ASSERT(queryResetAndClearBindings(id));
    ASSERT(queryBindValue(id, 1, nodeId));
    if (!queryNext(id, found)) {
        LOG_WARN(_logger, "Error getting query result: "
                              << id.c_str() << (snapshot == ReplicaSide::Local ? " - nodeIdLocal=" : " - nodeIdDrive=")
                              << nodeId.c_str());
        return false;
    }
    if (!found) {
        return true;
    }
    int result;
    ASSERT(queryIntValue(id, SELECT_NODE_BY_REPLICAID_TYPE, result));
    type = static_cast<NodeType>(result);

    ASSERT(queryResetAndClearBindings(id));

    return true;
}

bool SyncDb::size(ReplicaSide snapshot, const NodeId &nodeId, int64_t &size, bool &found) {
    const std::lock_guard<std::mutex> lock(_mutex);

    std::string id = (snapshot == ReplicaSide::Local ? SELECT_NODE_BY_NODEIDLOCAL_ID : SELECT_NODE_BY_NODEIDDRIVE_ID);
    ASSERT(queryResetAndClearBindings(id));
    ASSERT(queryBindValue(id, 1, nodeId));
    if (!queryNext(id, found)) {
        LOG_WARN(_logger, "Error getting query result: "
                              << id.c_str() << (snapshot == ReplicaSide::Local ? " - nodeIdLocal=" : " - nodeIdDrive=")
                              << nodeId.c_str());
        return false;
    }
    if (!found) {
        return true;
    }
    ASSERT(queryInt64Value(id, SELECT_NODE_BY_REPLICAID_SIZE, size));

    ASSERT(queryResetAndClearBindings(id));

    return true;
}

bool SyncDb::created(ReplicaSide snapshot, const NodeId &nodeId, std::optional<SyncTime> &time, bool &found) {
    const std::lock_guard<std::mutex> lock(_mutex);

    std::string id = (snapshot == ReplicaSide::Local ? SELECT_NODE_BY_NODEIDLOCAL_ID : SELECT_NODE_BY_NODEIDDRIVE_ID);
    ASSERT(queryResetAndClearBindings(id));
    ASSERT(queryBindValue(id, 1, nodeId));
    if (!queryNext(id, found)) {
        LOG_WARN(_logger, "Error getting query result: "
                              << id.c_str() << (snapshot == ReplicaSide::Local ? " - nodeIdLocal=" : " - nodeIdDrive=")
                              << nodeId.c_str());
        return false;
    }
    if (!found) {
        return true;
    }
    bool ok;
    ASSERT(queryIsNullValue(id, SELECT_NODE_BY_REPLICAID_CREATED, ok));
    if (ok) {
        time = std::nullopt;
    } else {
        SyncTime timeTmp;
        ASSERT(queryInt64Value(id, SELECT_NODE_BY_REPLICAID_CREATED, timeTmp));
        time = std::make_optional(timeTmp);
    }

    ASSERT(queryResetAndClearBindings(id));

    return true;
}

// Returns the lastmodified date of the object with ID nodeId
bool SyncDb::lastModified(ReplicaSide snapshot, const NodeId &nodeId, std::optional<SyncTime> &time, bool &found) {
    const std::lock_guard<std::mutex> lock(_mutex);

    std::string id = (snapshot == ReplicaSide::Local ? SELECT_NODE_BY_NODEIDLOCAL_ID : SELECT_NODE_BY_NODEIDDRIVE_ID);
    ASSERT(queryResetAndClearBindings(id));
    ASSERT(queryBindValue(id, 1, nodeId));
    if (!queryNext(id, found)) {
        LOG_WARN(_logger, "Error getting query result: "
                              << id.c_str() << (snapshot == ReplicaSide::Local ? " - nodeIdLocal=" : " - nodeIdDrive=")
                              << nodeId.c_str());
        return false;
    }
    if (!found) {
        return true;
    }
    bool ok;
    ASSERT(queryIsNullValue(id, SELECT_NODE_BY_REPLICAID_LASTMOD, ok));
    if (ok) {
        time = std::nullopt;
    } else {
        SyncTime timeTmp;
        ASSERT(queryInt64Value(id, SELECT_NODE_BY_REPLICAID_LASTMOD, timeTmp));
        time = std::make_optional(timeTmp);
    }

    ASSERT(queryResetAndClearBindings(id));

    return true;
}

// Returns the parent directory ID of the object with ID nodeId
bool SyncDb::parent(ReplicaSide snapshot, const NodeId &nodeId, NodeId &parentNodeid, bool &found) {
    const std::lock_guard<std::mutex> lock(_mutex);

    std::string id = (snapshot == ReplicaSide::Local ? SELECT_NODE_BY_NODEIDLOCAL_ID : SELECT_NODE_BY_NODEIDDRIVE_ID);
    ASSERT(queryResetAndClearBindings(id));
    ASSERT(queryBindValue(id, 1, nodeId));
    if (!queryNext(id, found)) {
        LOG_WARN(_logger, "Error getting query result: "
                              << id.c_str() << (snapshot == ReplicaSide::Local ? " - nodeIdLocal=" : " - nodeIdDrive=")
                              << nodeId.c_str());
        return false;
    }
    if (!found) {
        return true;
    }
    DbNodeId parentNodeDbId;
    ASSERT(queryInt64Value(id, SELECT_NODE_BY_REPLICAID_PARENTID, parentNodeDbId));
    ASSERT(queryResetAndClearBindings(id));

    ASSERT(queryResetAndClearBindings(SELECT_NODE_BY_NODEID_LITE_ID));
    ASSERT(queryBindValue(SELECT_NODE_BY_NODEID_LITE_ID, 1, parentNodeDbId));
    if (!queryNext(SELECT_NODE_BY_NODEID_LITE_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: "
                              << SELECT_NODE_BY_NODEID_LITE_ID
                              << (snapshot == ReplicaSide::Local ? " - nodeIdLocal=" : " - nodeIdDrive=")
                              << nodeId.c_str());
        return false;
    }
    if (!found) {
        return true;
    }
    ASSERT(queryStringValue(SELECT_NODE_BY_NODEID_LITE_ID, (snapshot == ReplicaSide::Local ? 3 : 4), parentNodeid));
    ASSERT(queryResetAndClearBindings(SELECT_NODE_BY_NODEID_LITE_ID));

    return true;
}

// Returns the path from the root node to the node with ID nodeId, concatenating the respective names of the nodes along the
// traversal-path
bool SyncDb::path(ReplicaSide snapshot, const NodeId &nodeId, SyncPath &path, bool &found) {
    const std::lock_guard<std::mutex> lock(_mutex);

    std::string id = (snapshot == ReplicaSide::Local ? SELECT_NODE_BY_NODEIDLOCAL_ID : SELECT_NODE_BY_NODEIDDRIVE_ID);
    ASSERT(queryResetAndClearBindings(id));
    ASSERT(queryBindValue(id, 1, nodeId));
    if (!queryNext(id, found)) {
        LOG_WARN(_logger, "Error getting query result: "
                              << id.c_str() << (snapshot == ReplicaSide::Local ? " - nodeIdLocal=" : " - nodeIdDrive=")
                              << nodeId.c_str());
        return false;
    }
    if (!found) {
        return true;
    }

    // Fill names' vector
    std::vector<std::string> names;

    bool parentNodeDbIdIsNull;
    DbNodeId parentNodeDbId;
    ASSERT(queryIsNullValue(id, SELECT_NODE_BY_REPLICAID_PARENTID, parentNodeDbIdIsNull));
    if (!parentNodeDbIdIsNull) {
        ASSERT(queryInt64Value(id, SELECT_NODE_BY_REPLICAID_PARENTID, parentNodeDbId));

        std::string name;
        ASSERT(queryStringValue(
            id, snapshot == ReplicaSide::Local ? SELECT_NODE_BY_REPLICAID_NAMELOCAL : SELECT_NODE_BY_REPLICAID_NAMEDRIVE, name));
        names.push_back(name);

        ASSERT(queryResetAndClearBindings(id));

        while (!parentNodeDbIdIsNull) {
            ASSERT(queryResetAndClearBindings(SELECT_NODE_BY_NODEID_LITE_ID));
            ASSERT(queryBindValue(SELECT_NODE_BY_NODEID_LITE_ID, 1, parentNodeDbId));
            if (!queryNext(SELECT_NODE_BY_NODEID_LITE_ID, found)) {
                LOG_WARN(_logger, "Error getting query result: "
                                      << SELECT_NODE_BY_NODEID_LITE_ID
                                      << (snapshot == ReplicaSide::Local ? " - nodeIdLocal=" : " - nodeIdDrive=")
                                      << nodeId.c_str());
                return false;
            }
            if (!found) {
                return true;
            }

            ASSERT(queryIsNullValue(SELECT_NODE_BY_NODEID_LITE_ID, 0, parentNodeDbIdIsNull));
            if (!parentNodeDbIdIsNull) {
                ASSERT(queryInt64Value(SELECT_NODE_BY_NODEID_LITE_ID, 0, parentNodeDbId));

                ASSERT(
                    queryStringValue(SELECT_NODE_BY_NODEID_LITE_ID, (snapshot == ReplicaSide::Local ? 1 : 2), name));
                names.push_back(name);
            }

            ASSERT(queryResetAndClearBindings(SELECT_NODE_BY_NODEID_LITE_ID));
        }
    } else {
        ASSERT(queryResetAndClearBindings(id));
    }

    // Construct path from names' vector
    path.clear();
    for (std::vector<std::string>::reverse_iterator nameIt = names.rbegin(); nameIt != names.rend(); ++nameIt) {
        path.append(*nameIt);
    }

    return true;
}

// Returns the name of the object with ID nodeId
bool SyncDb::name(ReplicaSide snapshot, const NodeId &nodeId, SyncName &name, bool &found) {
    const std::lock_guard<std::mutex> lock(_mutex);

    std::string id = (snapshot == ReplicaSide::Local ? SELECT_NODE_BY_NODEIDLOCAL_ID : SELECT_NODE_BY_NODEIDDRIVE_ID);
    ASSERT(queryResetAndClearBindings(id));
    ASSERT(queryBindValue(id, 1, nodeId));
    if (!queryNext(id, found)) {
        LOG_WARN(_logger, "Error getting query result: "
                              << id.c_str() << (snapshot == ReplicaSide::Local ? " - nodeIdLocal=" : " - nodeIdDrive=")
                              << nodeId.c_str());
        return false;
    }
    if (!found) {
        return true;
    }
    ASSERT(querySyncNameValue(
        id, snapshot == ReplicaSide::Local ? SELECT_NODE_BY_REPLICAID_NAMELOCAL : SELECT_NODE_BY_REPLICAID_NAMEDRIVE, name));
    ASSERT(queryResetAndClearBindings(id));

    return true;
}

// Returns the checksum of the object with ID nodeId
bool SyncDb::checksum(ReplicaSide snapshot, const NodeId &nodeId, std::optional<std::string> &cs, bool &found) {
    const std::lock_guard<std::mutex> lock(_mutex);

    std::string id = (snapshot == ReplicaSide::Local ? SELECT_NODE_BY_NODEIDLOCAL_ID : SELECT_NODE_BY_NODEIDDRIVE_ID);
    ASSERT(queryResetAndClearBindings(id));
    ASSERT(queryBindValue(id, 1, nodeId));
    if (!queryNext(id, found)) {
        LOG_WARN(_logger, "Error getting query result: "
                              << id.c_str() << (snapshot == ReplicaSide::Local ? " - nodeIdLocal=" : " - nodeIdDrive=")
                              << nodeId.c_str());
        return false;
    }
    if (!found) {
        return true;
    }
    bool ok;
    ASSERT(queryIsNullValue(id, SELECT_NODE_BY_REPLICAID_CHECKSUM, ok));
    if (ok) {
        cs = std::nullopt;
    } else {
        std::string csTmp;
        ASSERT(queryStringValue(id, SELECT_NODE_BY_REPLICAID_CHECKSUM, csTmp));
        cs = std::make_optional(csTmp);
    }
    ASSERT(queryResetAndClearBindings(id));

    return true;
}

// Returns the list of IDs contained in snapshot
bool SyncDb::ids(ReplicaSide snapshot, std::vector<NodeId> &ids, bool &found) {
    const std::lock_guard<std::mutex> lock(_mutex);

    // Find root node
    ASSERT(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID));
    if (!queryNext(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID);
        return false;
    }
    if (!found) {
        return true;
    }
    DbNodeId nodeDbId;
    ASSERT(queryInt64Value(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, 0, nodeDbId));

    std::string id;
    ASSERT(
        queryStringValue(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, (snapshot == ReplicaSide::Local ? 3 : 4), id));
    ids.push_back(id);

    ASSERT(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID));

    // Push child nodes' ids
    if (!pushChildIds(snapshot, nodeDbId, ids)) {
        return false;
    }

    return true;
}

bool SyncDb::ids(ReplicaSide snapshot, std::unordered_set<NodeId> &ids, bool &found) {
    const std::lock_guard<std::mutex> lock(_mutex);

    // Find root node
    ASSERT(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID));
    if (!queryNext(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID);
        return false;
    }
    if (!found) {
        return true;
    }
    DbNodeId nodeDbId;
    ASSERT(queryInt64Value(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, 0, nodeDbId));

    std::string id;
    ASSERT(
        queryStringValue(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, (snapshot == ReplicaSide::Local ? 3 : 4), id));
    ids.insert(id);

    ASSERT(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID));

    // Push child nodes' ids
    if (!pushChildIds(snapshot, nodeDbId, ids)) {
        return false;
    }

    return true;
}

// Returns whether node with ID nodeId1 is an ancestor of the node with ID nodeId2 in snapshot
bool SyncDb::ancestor(ReplicaSide snapshot, const NodeId &nodeId1, const NodeId &nodeId2, bool &ret, bool &found) {
    const std::lock_guard<std::mutex> lock(_mutex);

    if (nodeId1 == nodeId2) {
        ret = true;
        return true;
    }

    // Find node 2
    std::string id = (snapshot == ReplicaSide::Local ? SELECT_NODE_BY_NODEIDLOCAL_ID : SELECT_NODE_BY_NODEIDDRIVE_ID);
    ASSERT(queryResetAndClearBindings(id));
    ASSERT(queryBindValue(id, 1, nodeId2));
    if (!queryNext(id, found)) {
        LOG_WARN(_logger, "Error getting query result: "
                              << id.c_str() << (snapshot == ReplicaSide::Local ? " - nodeIdLocal=" : " - nodeIdDrive=")
                              << nodeId2.c_str());
        return false;
    }
    if (!found) {
        return true;
    }
    bool parentNodeDbIdIsNull;
    ASSERT(queryIsNullValue(id, SELECT_NODE_BY_REPLICAID_PARENTID, parentNodeDbIdIsNull));
    if (parentNodeDbIdIsNull) {
        ret = false;
        return true;
    }

    // Loop on the ancestors
    DbNodeId parentNodeDbId;
    ASSERT(queryInt64Value(id, SELECT_NODE_BY_REPLICAID_PARENTID, parentNodeDbId));
    ASSERT(queryResetAndClearBindings(id));
    do {
        ASSERT(queryResetAndClearBindings(SELECT_NODE_BY_NODEID_LITE_ID));
        ASSERT(queryBindValue(SELECT_NODE_BY_NODEID_LITE_ID, 1, parentNodeDbId));
        if (!queryNext(SELECT_NODE_BY_NODEID_LITE_ID, found)) {
            LOG_WARN(_logger, "Error getting query result: " << SELECT_NODE_BY_NODEID_LITE_ID);
            return false;
        }
        if (!found) {
            return true;
        }

        bool nodeIdIsNull;
        ASSERT(
            queryIsNullValue(SELECT_NODE_BY_NODEID_LITE_ID, (snapshot == ReplicaSide::Local ? 3 : 4), nodeIdIsNull));
        if (nodeIdIsNull) {
            // Database inconsistency
            LOG_WARN(_logger, "Database inconsistency: node with dbId="
                                  << parentNodeDbId << " is not in snapshot="
                                  << (snapshot == ReplicaSide::Local ? "Local" : "Drive") << " while child is");
            found = false;
            return true;
        }
        NodeId nodeId;
        ASSERT(queryStringValue(SELECT_NODE_BY_NODEID_LITE_ID, (snapshot == ReplicaSide::Local ? 3 : 4), nodeId));
        if (nodeId == nodeId1) {
            ret = true;
            return true;
        }

        ASSERT(queryIsNullValue(SELECT_NODE_BY_NODEID_LITE_ID, 0, parentNodeDbIdIsNull));
        if (!parentNodeDbIdIsNull) {
            ASSERT(queryInt64Value(SELECT_NODE_BY_NODEID_LITE_ID, 0, parentNodeDbId));
        }

        ASSERT(queryResetAndClearBindings(SELECT_NODE_BY_NODEID_LITE_ID));
    } while (!parentNodeDbIdIsNull);

    ret = false;
    return true;
}

// Returns database ID for the ID nodeId of snapshot
bool SyncDb::dbId(ReplicaSide snapshot, const NodeId &nodeId, DbNodeId &dbNodeId, bool &found) {
    const std::lock_guard<std::mutex> lock(_mutex);

    std::string id = (snapshot == ReplicaSide::Local ? SELECT_NODE_BY_NODEIDLOCAL_ID : SELECT_NODE_BY_NODEIDDRIVE_ID);
    ASSERT(queryResetAndClearBindings(id));
    ASSERT(queryBindValue(id, 1, nodeId));
    if (!queryNext(id, found)) {
        LOG_WARN(_logger, "Error getting query result: "
                              << id.c_str() << (snapshot == ReplicaSide::Local ? " - nodeIdLocal=" : " - nodeIdDrive=")
                              << nodeId.c_str());
        return false;
    }
    if (!found) {
        return true;
    }
    ASSERT(queryInt64Value(id, SELECT_NODE_BY_REPLICAID_DBID, dbNodeId));
    ASSERT(queryResetAndClearBindings(id));

    return true;
}

// Returns the ID of snapshot for the database ID dbNodeId
bool SyncDb::id(ReplicaSide snapshot, DbNodeId dbNodeId, NodeId &nodeId, bool &found) {
    const std::lock_guard<std::mutex> lock(_mutex);

    ASSERT(queryResetAndClearBindings(SELECT_NODE_BY_NODEID_LITE_ID));
    ASSERT(queryBindValue(SELECT_NODE_BY_NODEID_LITE_ID, 1, dbNodeId));
    if (!queryNext(SELECT_NODE_BY_NODEID_LITE_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_NODE_BY_NODEID_LITE_ID
                                                         << " - nodeId=" << std::to_string(dbNodeId).c_str());
        return false;
    }
    if (!found) {
        return true;
    }
    ASSERT(queryStringValue(SELECT_NODE_BY_NODEID_LITE_ID, (snapshot == ReplicaSide::Local ? 3 : 4), nodeId));
    ASSERT(queryResetAndClearBindings(SELECT_NODE_BY_NODEID_LITE_ID));

    return true;
}

bool SyncDb::correspondingNodeId(ReplicaSide snapshotIn, const NodeId &nodeIdIn, NodeId &nodeIdOut, bool &found) {
    DbNodeId dbNodeId;
    if (!dbId(snapshotIn, nodeIdIn, dbNodeId, found)) {
        return false;
    }
    if (!found) {
        return true;
    }

    return id(otherSide(snapshotIn), dbNodeId, nodeIdOut, found);
}

bool SyncDb::updateAllSyncNodes(SyncNodeType type, const std::unordered_set<NodeId> &nodeIdSet) {
    const std::lock_guard<std::mutex> lock(_mutex);

    int errId;
    std::string error;

    startTransaction();

    // Delete existing SyncNodes
    ASSERT(queryResetAndClearBindings(DELETE_ALL_SYNC_NODE_BY_TYPE_REQUEST_ID));
    ASSERT(queryBindValue(DELETE_ALL_SYNC_NODE_BY_TYPE_REQUEST_ID, 1, type));
    if (!queryExec(DELETE_ALL_SYNC_NODE_BY_TYPE_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << DELETE_ALL_SYNC_NODE_BY_TYPE_REQUEST_ID);
        rollbackTransaction();
        return false;
    }

    // Insert new SyncNodes
    for (const NodeId &nodeId : nodeIdSet) {
        ASSERT(queryResetAndClearBindings(INSERT_SYNC_NODE_REQUEST_ID));
        ASSERT(queryBindValue(INSERT_SYNC_NODE_REQUEST_ID, 1, nodeId));
        ASSERT(queryBindValue(INSERT_SYNC_NODE_REQUEST_ID, 2, type));
        if (!queryExec(INSERT_SYNC_NODE_REQUEST_ID, errId, error)) {
            LOG_WARN(_logger, "Error running query: " << INSERT_SYNC_NODE_REQUEST_ID);
            rollbackTransaction();
            return false;
        }
    }

    commitTransaction();

    return true;
}

bool SyncDb::selectAllSyncNodes(SyncNodeType type, std::unordered_set<NodeId> &nodeIdSet) {
    const std::lock_guard<std::mutex> lock(_mutex);

    ASSERT(queryResetAndClearBindings(SELECT_ALL_SYNC_NODE_REQUEST_ID));
    ASSERT(queryBindValue(SELECT_ALL_SYNC_NODE_REQUEST_ID, 1, type));
    bool found;
    for (;;) {
        if (!queryNext(SELECT_ALL_SYNC_NODE_REQUEST_ID, found)) {
            LOGW_WARN(_logger, L"Error getting query result: " << SELECT_ALL_SYNC_NODE_REQUEST_ID);
            return false;
        }
        if (!found) {
            break;
        }

        NodeId nodeId;
        ASSERT(queryStringValue(SELECT_ALL_SYNC_NODE_REQUEST_ID, 0, nodeId));

        nodeIdSet.insert(nodeId);
    }
    ASSERT(queryResetAndClearBindings(SELECT_ALL_SYNC_NODE_REQUEST_ID));

    return true;
}

bool SyncDb::insertUploadSessionToken(const UploadSessionToken &uploadSessionToken, int64_t &uploadSessionTokenDbId) {
    const std::lock_guard<std::mutex> lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(INSERT_UPLOAD_SESSION_TOKEN_REQUEST_ID));
    ASSERT(queryBindValue(INSERT_UPLOAD_SESSION_TOKEN_REQUEST_ID, 1, uploadSessionToken.token()));
    if (!queryExecAndGetRowId(INSERT_UPLOAD_SESSION_TOKEN_REQUEST_ID, uploadSessionTokenDbId, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << INSERT_UPLOAD_SESSION_TOKEN_REQUEST_ID);
        return false;
    }

    return true;
}

bool SyncDb::deleteUploadSessionTokenByDbId(int64_t dbId, bool &found) {
    const std::lock_guard<std::mutex> lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(DELETE_UPLOAD_SESSION_TOKEN_BY_DBID_REQUEST_ID));
    ASSERT(queryBindValue(DELETE_UPLOAD_SESSION_TOKEN_BY_DBID_REQUEST_ID, 1, dbId));
    if (!queryExec(DELETE_UPLOAD_SESSION_TOKEN_BY_DBID_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << DELETE_UPLOAD_SESSION_TOKEN_BY_DBID_REQUEST_ID);
        return false;
    }
    if (numRowsAffected() == 1) {
        found = true;
    } else {
        LOG_WARN(_logger,
                 "Error running query: " << DELETE_UPLOAD_SESSION_TOKEN_BY_DBID_REQUEST_ID << " - num rows affected != 1");
        found = false;
    }

    return true;
}

bool SyncDb::selectAllUploadSessionTokens(std::vector<UploadSessionToken> &uploadSessionTokenList) {
    const std::lock_guard<std::mutex> lock(_mutex);

    uploadSessionTokenList.clear();

    ASSERT(queryResetAndClearBindings(SELECT_ALL_UPLOAD_SESSION_TOKEN_REQUEST_ID));
    bool found;
    for (;;) {
        if (!queryNext(SELECT_ALL_UPLOAD_SESSION_TOKEN_REQUEST_ID, found)) {
            LOG_WARN(_logger, "Error getting query result: " << SELECT_ALL_UPLOAD_SESSION_TOKEN_REQUEST_ID);
            return false;
        }
        if (!found) {
            break;
        }

        int id;
        ASSERT(queryIntValue(SELECT_ALL_UPLOAD_SESSION_TOKEN_REQUEST_ID, 0, id));
        std::string token;
        ASSERT(queryStringValue(SELECT_ALL_UPLOAD_SESSION_TOKEN_REQUEST_ID, 1, token));

        uploadSessionTokenList.emplace_back(id, token);
    }
    ASSERT(queryResetAndClearBindings(SELECT_ALL_UPLOAD_SESSION_TOKEN_REQUEST_ID));

    return true;
}

bool SyncDb::deleteAllUploadSessionToken() {
    const std::lock_guard<std::mutex> lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(DELETE_ALL_UPLOAD_SESSION_TOKEN_REQUEST_ID));
    if (!queryExec(DELETE_ALL_UPLOAD_SESSION_TOKEN_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << DELETE_ALL_UPLOAD_SESSION_TOKEN_REQUEST_ID);
        return false;
    }

    return true;
}

bool SyncDb::setTargetNodeId(const std::string &targetNodeId, bool &found) {
    // Update root node
    _rootNode.setNodeIdRemote(targetNodeId);
    return updateNode(_rootNode, found);
}

bool SyncDb::pushChildIds(ReplicaSide snapshot, DbNodeId parentNodeDbId, std::vector<NodeId> &ids) {
    std::queue<DbNodeId> dbNodeIdQueue;
    dbNodeIdQueue.push(parentNodeDbId);

    while (!dbNodeIdQueue.empty()) {
        DbNodeId dbNodeId = dbNodeIdQueue.front();
        dbNodeIdQueue.pop();

        ASSERT(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID));
        ASSERT(queryBindValue(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, 1, dbNodeId));
        for (;;) {
            bool found;
            if (!queryNext(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, found)) {
                LOG_WARN(_logger, "Error getting query result: " << SELECT_NODE_BY_PARENTNODEID_REQUEST_ID
                                                                 << " - parentNodeId=" << std::to_string(dbNodeId).c_str());
                return false;
            }
            if (!found) {
                break;
            }
            bool nodeIdIsNull;
            ASSERT(queryIsNullValue(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, (snapshot == ReplicaSide::Local ? 3 : 4),
                                    nodeIdIsNull));
            if (!nodeIdIsNull) {
                // The node exists in the snapshot
                DbNodeId dbChildNodeId;
                ASSERT(queryInt64Value(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, 0, dbChildNodeId));

                NodeId childNodeId;
                ASSERT(queryStringValue(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID,
                                        (snapshot == ReplicaSide::Local ? 3 : 4), childNodeId));
                ids.push_back(childNodeId);

                int type;
                ASSERT(queryIntValue(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, 5, type));
                if (static_cast<NodeType>(type) == NodeType::Directory) {
                    dbNodeIdQueue.push(dbChildNodeId);
                }
            }
        }
        ASSERT(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID));
    }

    return true;
}

bool SyncDb::pushChildIds(ReplicaSide snapshot, DbNodeId parentNodeDbId, std::unordered_set<NodeId> &ids) {
    std::queue<DbNodeId> dbNodeIdQueue;
    dbNodeIdQueue.push(parentNodeDbId);

    while (!dbNodeIdQueue.empty()) {
        DbNodeId dbNodeId = dbNodeIdQueue.front();
        dbNodeIdQueue.pop();

        ASSERT(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID));
        ASSERT(queryBindValue(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, 1, dbNodeId));
        for (;;) {
            bool found;
            if (!queryNext(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, found)) {
                LOG_WARN(_logger, "Error getting query result: " << SELECT_NODE_BY_PARENTNODEID_REQUEST_ID
                                                                 << " - parentNodeId=" << std::to_string(dbNodeId).c_str());
                return false;
            }
            if (!found) {
                break;
            }
            bool nodeIdIsNull;
            ASSERT(queryIsNullValue(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, (snapshot == ReplicaSide::Local ? 3 : 4),
                                    nodeIdIsNull));
            if (!nodeIdIsNull) {
                // The node exists in the snapshot
                DbNodeId dbChildNodeId;
                ASSERT(queryInt64Value(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, 0, dbChildNodeId));

                NodeId childNodeId;
                ASSERT(queryStringValue(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID,
                                        (snapshot == ReplicaSide::Local ? 3 : 4), childNodeId));
                ids.insert(childNodeId);

                int type;
                ASSERT(queryIntValue(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, 5, type));
                if (static_cast<NodeType>(type) == NodeType::Directory) {
                    dbNodeIdQueue.push(dbChildNodeId);
                }
            }
        }
        ASSERT(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID));
    }

    return true;
}

bool SyncDb::selectAllRenamedNodes(std::vector<DbNode> &dbNodeList, bool onlyColon) {
    const std::lock_guard<std::mutex> lock(_mutex);

    dbNodeList.clear();

    std::string requestId = onlyColon ? SELECT_ALL_RENAMED_COLON_NODES_REQUEST_ID : SELECT_ALL_RENAMED_NODES_REQUEST_ID;

    ASSERT(queryResetAndClearBindings(requestId));

    bool found;
    for (;;) {
        if (!queryNext(requestId, found)) {
            LOGW_WARN(_logger, L"Error getting query result: " << requestId.c_str());
            return false;
        }
        if (!found) {
            break;
        }

        DbNodeId dbNodeId;
        ASSERT(queryInt64Value(requestId, 0, dbNodeId));

        bool ok;
        std::optional<DbNodeId> parentNodeId;
        ASSERT(queryIsNullValue(requestId, 1, ok));
        if (ok) {
            parentNodeId = std::nullopt;
        } else {
            DbNodeId dbParentNodeId;
            ASSERT(queryInt64Value(requestId, 1, dbParentNodeId));
            parentNodeId = std::make_optional(dbParentNodeId);
        }

        SyncName nameLocal;
        ASSERT(querySyncNameValue(requestId, 2, nameLocal));
        SyncName nameDrive;
        ASSERT(querySyncNameValue(requestId, 3, nameDrive));

        std::optional<NodeId> nodeIdLocal;
        ASSERT(queryIsNullValue(requestId, 4, ok));
        if (ok) {
            nodeIdLocal = std::nullopt;
        } else {
            NodeId nodeIdLocalTmp;
            ASSERT(queryStringValue(requestId, 4, nodeIdLocalTmp));
            nodeIdLocal = std::make_optional(nodeIdLocalTmp);
        }

        std::optional<NodeId> nodeIdDrive;
        ASSERT(queryIsNullValue(requestId, 5, ok));
        if (ok) {
            nodeIdDrive = std::nullopt;
        } else {
            NodeId nodeIdDriveTmp;
            ASSERT(queryStringValue(requestId, 5, nodeIdDriveTmp));
            nodeIdDrive = std::make_optional(nodeIdDriveTmp);
        }

        std::optional<SyncTime> created;
        ASSERT(queryIsNullValue(requestId, 6, ok));
        if (ok) {
            created = std::nullopt;
        } else {
            SyncTime timeTmp;
            ASSERT(queryInt64Value(requestId, 6, timeTmp));
            created = std::make_optional(timeTmp);
        }

        std::optional<SyncTime> lastModifiedLocal;
        ASSERT(queryIsNullValue(requestId, 7, ok));
        if (ok) {
            lastModifiedLocal = std::nullopt;
        } else {
            SyncTime timeTmp;
            ASSERT(queryInt64Value(requestId, 7, timeTmp));
            lastModifiedLocal = std::make_optional(timeTmp);
        }

        std::optional<SyncTime> lastModifiedDrive;
        ASSERT(queryIsNullValue(requestId, 8, ok));
        if (ok) {
            lastModifiedDrive = std::nullopt;
        } else {
            SyncTime timeTmp;
            ASSERT(queryInt64Value(requestId, 8, timeTmp));
            lastModifiedDrive = std::make_optional(timeTmp);
        }

        int intResult;
        ASSERT(queryIntValue(requestId, 9, intResult));
        NodeType type = static_cast<NodeType>(intResult);

        int64_t size;
        ASSERT(queryInt64Value(requestId, 10, size));

        std::optional<std::string> cs;
        ASSERT(queryIsNullValue(requestId, 11, ok));
        if (ok) {
            cs = std::nullopt;
        } else {
            std::string csTmp;
            ASSERT(queryStringValue(requestId, 9, csTmp));
            cs = std::make_optional(csTmp);
        }

        ASSERT(queryIntValue(requestId, 10, intResult));
        SyncFileStatus status = static_cast<SyncFileStatus>(intResult);

        ASSERT(queryIntValue(requestId, 11, intResult));
        bool syncing = static_cast<bool>(intResult);

        DbNode dbNode;
        dbNode.setNodeId(dbNodeId);
        dbNode.setParentNodeId(parentNodeId);
        dbNode.setNameLocal(nameLocal);
        dbNode.setNameRemote(nameDrive);
        dbNode.setNodeIdLocal(nodeIdLocal);
        dbNode.setNodeIdRemote(nodeIdDrive);
        dbNode.setCreated(created);
        dbNode.setType(type);
        dbNode.setSize(size);
        dbNode.setLastModifiedLocal(lastModifiedLocal);
        dbNode.setLastModifiedRemote(lastModifiedDrive);
        dbNode.setChecksum(cs);
        dbNode.setStatus(status);
        dbNode.setSyncing(syncing);

        dbNodeList.push_back(dbNode);
    }

    ASSERT(queryResetAndClearBindings(requestId));

    return true;
}

bool SyncDb::deleteNodesWithNullParentNodeId() {
    const std::lock_guard<std::mutex> lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(DELETE_NODES_WITH_NULL_PARENTNODEID_REQUEST_ID));
    if (!queryExec(DELETE_NODES_WITH_NULL_PARENTNODEID_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << DELETE_NODES_WITH_NULL_PARENTNODEID_REQUEST_ID);
        return false;
    }

    return true;
}

bool SyncDb::pushChildDbIds(DbNodeId parentNodeDbId, std::unordered_set<DbNodeId> &ids) {
    std::queue<DbNodeId> dbNodeIdQueue;
    dbNodeIdQueue.push(parentNodeDbId);

    while (!dbNodeIdQueue.empty()) {
        DbNodeId dbNodeId = dbNodeIdQueue.front();
        dbNodeIdQueue.pop();

        ASSERT(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID));
        ASSERT(queryBindValue(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, 1, dbNodeId));
        for (;;) {
            bool found;
            if (!queryNext(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, found)) {
                LOG_WARN(_logger, "Error getting query result: " << SELECT_NODE_BY_PARENTNODEID_REQUEST_ID
                                                                 << " - parentNodeId=" << std::to_string(dbNodeId).c_str());
                return false;
            }
            if (!found) {
                break;
            }
            bool nodeIdIsNull;
            ASSERT(queryIsNullValue(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, 0, nodeIdIsNull));
            if (!nodeIdIsNull) {
                // The node exists in the snapshot
                DbNodeId dbChildNodeId;
                ASSERT(queryInt64Value(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, 0, dbChildNodeId));
                ids.insert(dbChildNodeId);

                int type;
                ASSERT(queryIntValue(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, 5, type));
                if (static_cast<NodeType>(type) == NodeType::Directory) {
                    dbNodeIdQueue.push(dbChildNodeId);
                }
            }
        }
        ASSERT(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID));
    }

    return true;
}

}  // namespace KDC
