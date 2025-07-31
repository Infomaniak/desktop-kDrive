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

#include "syncdb.h"
#include "libcommon/utility/utility.h"
#include "libcommon/utility/logiffail.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/log/log.h"

#include "libparms/db/sync.h"
#include "libparms/db/parmsdb.h"

#include <queue>

#include <3rdparty/sqlite3/sqlite3.h>

#define PRAGMA_WRITABLE_SCHEMA_ID "db1"
#define PRAGMA_WRITABLE_SCHEMA "PRAGMA writable_schema=ON;"

//
// node
//

// /!\ nameLocal & nameDrive must be in NFC form

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

#define UPDATE_NODE_NAME_LOCAL_REQUEST_ID "update_node_name_local"
#define UPDATE_NODE_NAME_LOCAL_REQUEST \
    "UPDATE node SET nameLocal=?1 "    \
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
#define SELECT_NODE_BY_NODEIDLOCAL                                                                                          \
    "SELECT nodeId, parentNodeId, nameLocal, nameDrive, nodeIdLocal, nodeIdDrive, created, lastModifiedLocal, type, size, " \
    "checksum, status, syncing FROM "                                                                                       \
    "node "                                                                                                                 \
    "WHERE nodeIdLocal=?1;"

#define SELECT_NODE_BY_NODEIDDRIVE_ID "select_node4"
#define SELECT_NODE_BY_NODEIDDRIVE                                                                                          \
    "SELECT nodeId, parentNodeId, nameLocal, nameDrive, nodeIdLocal, nodeIdDrive, created, lastModifiedDrive, type, size, " \
    "checksum, status, syncing FROM "                                                                                       \
    "node "                                                                                                                 \
    "WHERE nodeIdDrive=?1;"

#define SELECT_NODE_BY_REPLICAID_DBID 0
#define SELECT_NODE_BY_REPLICAID_PARENTID 1
#define SELECT_NODE_BY_REPLICAID_NAMELOCAL 2
#define SELECT_NODE_BY_REPLICAID_NAMEDRIVE 3
#define SELECT_NODE_BY_REPLICAID_IDLOCAL 4
#define SELECT_NODE_BY_REPLICAID_IDDRIVE 5
#define SELECT_NODE_BY_REPLICAID_CREATED 6
#define SELECT_NODE_BY_REPLICAID_LASTMOD 7
#define SELECT_NODE_BY_REPLICAID_TYPE 8
#define SELECT_NODE_BY_REPLICAID_SIZE 9
#define SELECT_NODE_BY_REPLICAID_CHECKSUM 10
#define SELECT_NODE_BY_REPLICAID_STATUS 11
#define SELECT_NODE_BY_REPLICAID_SYNCING 12

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

#define SELECT_ANCESTORS_NODES_REQUEST_ID "select_node13"
#define SELECT_ANCESTORS_NODES_REQUEST                                                                            \
    "WITH ancestor as (SELECT parentNodeId AS pid, nameLocal AS nl, nameDrive AS nd FROM node WHERE nodeId = ?1 " \
    "UNION ALL "                                                                                                  \
    "SELECT parentNodeId, nameLocal, nameDrive FROM ancestor, node WHERE ancestor.pid = node.nodeId) "            \
    "SELECT nl, nd FROM ancestor;"

#define SELECT_ANCESTORS_NODES_LOCAL_REQUEST_ID "select_node14"
#define SELECT_ANCESTORS_NODES_LOCAL_REQUEST                                                                           \
    "WITH ancestor as (SELECT parentNodeId AS pid, nameLocal AS nl, nameDrive AS nd FROM node WHERE nodeIdLocal = ?1 " \
    "UNION ALL "                                                                                                       \
    "SELECT parentNodeId, nameLocal, nameDrive FROM ancestor, node WHERE ancestor.pid = node.nodeId) "                 \
    "SELECT nl, nd FROM ancestor;"

#define SELECT_ANCESTORS_NODES_DRIVE_REQUEST_ID "select_node15"
#define SELECT_ANCESTORS_NODES_DRIVE_REQUEST                                                                           \
    "WITH ancestor as (SELECT parentNodeId AS pid, nameLocal AS nl, nameDrive AS nd FROM node WHERE nodeIdDrive = ?1 " \
    "UNION ALL "                                                                                                       \
    "SELECT parentNodeId, nameLocal, nameDrive FROM ancestor, node WHERE ancestor.pid = node.nodeId) "                 \
    "SELECT nl, nd FROM ancestor;"

#define SELECT_ALL_NODES_REQUEST_ID "select_node16"
#define SELECT_ALL_NODES_REQUEST                                                                                \
    "SELECT nodeId, parentNodeId, nameLocal, nameDrive, nodeIdLocal, nodeIdDrive, created, lastModifiedLocal, " \
    "lastModifiedDrive, type, size, checksum, status, syncing FROM node;"

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

SyncDb::SyncDb(const std::string &dbPath, const std::string &version, const std::string &targetNodeId) :
    Db(dbPath),
    _cache(*this) {
    if (!targetNodeId.empty()) {
        _rootNode.setNodeIdRemote(targetNodeId);
    }

    if (!checkConnect(version)) {
        throw std::runtime_error("Cannot open DB!");
    }

    LOG_INFO(_logger, "SyncDb initialization done: dbPath=" << dbPath << " targetNodeId=" << targetNodeId);
}

bool SyncDb::create(bool &retry) {
    int errId;
    std::string error;

    // Node
    if (!createAndPrepareRequest(CREATE_NODE_TABLE_ID, CREATE_NODE_TABLE)) return false;
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

    if (!createAndPrepareRequest(CREATE_NODE_TABLE_IDX1_ID, CREATE_NODE_TABLE_IDX1)) return false;
    if (!queryExec(CREATE_NODE_TABLE_IDX1_ID, errId, error)) {
        queryFree(CREATE_NODE_TABLE_IDX1_ID);
        return sqlFail(CREATE_NODE_TABLE_IDX1_ID, error);
    }
    queryFree(CREATE_NODE_TABLE_IDX1_ID);

    if (!createAndPrepareRequest(CREATE_NODE_TABLE_IDX2_ID, CREATE_NODE_TABLE_IDX2)) return false;
    if (!queryExec(CREATE_NODE_TABLE_IDX2_ID, errId, error)) {
        queryFree(CREATE_NODE_TABLE_IDX2_ID);
        return sqlFail(CREATE_NODE_TABLE_IDX2_ID, error);
    }
    queryFree(CREATE_NODE_TABLE_IDX2_ID);

    if (!createAndPrepareRequest(CREATE_NODE_TABLE_IDX3_ID, CREATE_NODE_TABLE_IDX3)) return false;
    if (!queryExec(CREATE_NODE_TABLE_IDX3_ID, errId, error)) {
        queryFree(CREATE_NODE_TABLE_IDX3_ID);
        return sqlFail(CREATE_NODE_TABLE_IDX3_ID, error);
    }
    queryFree(CREATE_NODE_TABLE_IDX3_ID);

    if (!createAndPrepareRequest(CREATE_NODE_TABLE_IDX4_ID, CREATE_NODE_TABLE_IDX4)) return false;
    if (!queryExec(CREATE_NODE_TABLE_IDX4_ID, errId, error)) {
        queryFree(CREATE_NODE_TABLE_IDX4_ID);
        return sqlFail(CREATE_NODE_TABLE_IDX4_ID, error);
    }
    queryFree(CREATE_NODE_TABLE_IDX4_ID);

    if (!createAndPrepareRequest(CREATE_NODE_TABLE_IDX5_ID, CREATE_NODE_TABLE_IDX5)) return false;
    if (!queryExec(CREATE_NODE_TABLE_IDX5_ID, errId, error)) {
        queryFree(CREATE_NODE_TABLE_IDX5_ID);
        return sqlFail(CREATE_NODE_TABLE_IDX5_ID, error);
    }
    queryFree(CREATE_NODE_TABLE_IDX5_ID);

    // Sync Node
    if (!createAndPrepareRequest(CREATE_SYNC_NODE_TABLE_ID, CREATE_SYNC_NODE_TABLE)) return false;
    if (!queryExec(CREATE_SYNC_NODE_TABLE_ID, errId, error)) {
        queryFree(CREATE_SYNC_NODE_TABLE_ID);
        return sqlFail(CREATE_SYNC_NODE_TABLE_ID, error);
    }
    queryFree(CREATE_SYNC_NODE_TABLE_ID);

    // Upload session token table
    if (!createAndPrepareRequest(CREATE_UPLOAD_SESSION_TOKEN_TABLE_ID, CREATE_UPLOAD_SESSION_TOKEN_TABLE)) return false;
    if (!queryExec(CREATE_UPLOAD_SESSION_TOKEN_TABLE_ID, errId, error)) {
        queryFree(CREATE_UPLOAD_SESSION_TOKEN_TABLE_ID);
        return sqlFail(CREATE_UPLOAD_SESSION_TOKEN_TABLE_ID, error);
    }
    queryFree(CREATE_UPLOAD_SESSION_TOKEN_TABLE_ID);

    return true;
}

bool SyncDb::prepare() {
    // Node
    if (!createAndPrepareRequest(INSERT_NODE_REQUEST_ID, INSERT_NODE_REQUEST)) return false;
    if (!createAndPrepareRequest(UPDATE_NODE_REQUEST_ID, UPDATE_NODE_REQUEST)) return false;
    if (!createAndPrepareRequest(UPDATE_NODE_STATUS_REQUEST_ID, UPDATE_NODE_STATUS_REQUEST)) return false;
    if (!createAndPrepareRequest(UPDATE_NODE_NAME_LOCAL_REQUEST_ID, UPDATE_NODE_NAME_LOCAL_REQUEST)) return false;
    if (!createAndPrepareRequest(UPDATE_NODES_SYNCING_REQUEST_ID, UPDATE_NODES_SYNCING_REQUEST)) return false;
    if (!createAndPrepareRequest(UPDATE_NODE_SYNCING_REQUEST_ID, UPDATE_NODE_SYNCING_REQUEST)) return false;
    if (!createAndPrepareRequest(DELETE_NODE_REQUEST_ID, DELETE_NODE_REQUEST)) return false;
    if (!createAndPrepareRequest(DELETE_NODES_BUT_ROOT_REQUEST_ID, DELETE_NODES_BUT_ROOT_REQUEST)) return false;
    if (!createAndPrepareRequest(DELETE_NODES_WITH_NULL_PARENTNODEID_REQUEST_ID, DELETE_NODES_WITH_NULL_PARENTNODEID_REQUEST))
        return false;
    if (!createAndPrepareRequest(SELECT_NODE_BY_NODEID_LITE_ID, SELECT_NODE_BY_NODEID_LITE)) return false;
    if (!createAndPrepareRequest(SELECT_NODE_BY_NODEID_FULL_ID, SELECT_NODE_BY_NODEID_FULL)) return false;
    if (!createAndPrepareRequest(SELECT_NODE_BY_NODEIDLOCAL_ID, SELECT_NODE_BY_NODEIDLOCAL)) return false;
    if (!createAndPrepareRequest(SELECT_NODE_BY_NODEIDDRIVE_ID, SELECT_NODE_BY_NODEIDDRIVE)) return false;
    if (!createAndPrepareRequest(SELECT_NODE_BY_PARENTNODEID_AND_NAMELOCAL_REQUEST_ID,
                                 SELECT_NODE_BY_PARENTNODEID_AND_NAMELOCAL_REQUEST))
        return false;
    if (!createAndPrepareRequest(SELECT_NODE_BY_PARENTNODEID_AND_NAMEDRIVE_REQUEST_ID,
                                 SELECT_NODE_BY_PARENTNODEID_AND_NAMEDRIVE_REQUEST))
        return false;
    if (!createAndPrepareRequest(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, SELECT_NODE_BY_PARENTNODEID_REQUEST)) return false;
    if (!createAndPrepareRequest(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST))
        return false;
    if (!createAndPrepareRequest(SELECT_NODE_STATUS_BY_NODEID_REQUEST_ID, SELECT_NODE_STATUS_BY_NODEID_REQUEST)) return false;
    if (!createAndPrepareRequest(SELECT_NODE_SYNCING_BY_NODEID_REQUEST_ID, SELECT_NODE_SYNCING_BY_NODEID_REQUEST)) return false;
    if (!createAndPrepareRequest(SELECT_ALL_RENAMED_COLON_NODES_REQUEST_ID, SELECT_ALL_RENAMED_COLON_NODES_REQUEST)) return false;
    if (!createAndPrepareRequest(SELECT_ALL_RENAMED_NODES_REQUEST_ID, SELECT_ALL_RENAMED_NODES_REQUEST)) return false;
    if (!createAndPrepareRequest(SELECT_ANCESTORS_NODES_REQUEST_ID, SELECT_ANCESTORS_NODES_REQUEST)) return false;
    if (!createAndPrepareRequest(SELECT_ANCESTORS_NODES_LOCAL_REQUEST_ID, SELECT_ANCESTORS_NODES_LOCAL_REQUEST)) return false;
    if (!createAndPrepareRequest(SELECT_ANCESTORS_NODES_DRIVE_REQUEST_ID, SELECT_ANCESTORS_NODES_DRIVE_REQUEST)) return false;
    if (!createAndPrepareRequest(SELECT_ALL_NODES_REQUEST_ID, SELECT_ALL_NODES_REQUEST)) return false;

    // Sync Node
    if (!createAndPrepareRequest(INSERT_SYNC_NODE_REQUEST_ID, INSERT_SYNC_NODE_REQUEST)) return false;
    if (!createAndPrepareRequest(DELETE_ALL_SYNC_NODE_BY_TYPE_REQUEST_ID, DELETE_ALL_SYNC_NODE_BY_TYPE_REQUEST)) return false;
    if (!createAndPrepareRequest(SELECT_ALL_SYNC_NODE_REQUEST_ID, SELECT_ALL_SYNC_NODE_REQUEST)) return false;

    // Upload session token table
    if (!createAndPrepareRequest(INSERT_UPLOAD_SESSION_TOKEN_REQUEST_ID, INSERT_UPLOAD_SESSION_TOKEN_REQUEST)) return false;
    if (!createAndPrepareRequest(DELETE_UPLOAD_SESSION_TOKEN_BY_DBID_REQUEST_ID, DELETE_UPLOAD_SESSION_TOKEN_BY_DBID_REQUEST))
        return false;
    if (!createAndPrepareRequest(DELETE_ALL_UPLOAD_SESSION_TOKEN_REQUEST_ID, DELETE_ALL_UPLOAD_SESSION_TOKEN_REQUEST))
        return false;
    if (!createAndPrepareRequest(SELECT_ALL_UPLOAD_SESSION_TOKEN_REQUEST_ID, SELECT_ALL_UPLOAD_SESSION_TOKEN_REQUEST))
        return false;

    if (!initData()) {
        LOG_WARN(_logger, "Error in initParameters");
        return false;
    }

    return true;
}

bool SyncDb::upgrade(const std::string &fromVersion, const std::string &toVersion) {
    if (!CommonUtility::isVersionLower(fromVersion, toVersion)) return true;

    LOG_INFO(_logger, "Upgrade " << dbType() << " DB from " << fromVersion << " to " << toVersion);

    const std::string dbFromVersionNumber = CommonUtility::dbVersionNumber(fromVersion);

    int errId = -1;
    std::string error;

    if (dbFromVersionNumber == "3.4.0") {
        LOG_DEBUG(_logger, "Upgrade 3.4.0 Sync DB");

        // Upload session token table
        if (!createAndPrepareRequest(CREATE_UPLOAD_SESSION_TOKEN_TABLE_ID, CREATE_UPLOAD_SESSION_TOKEN_TABLE)) return false;
        if (!queryExec(CREATE_UPLOAD_SESSION_TOKEN_TABLE_ID, errId, error)) {
            queryFree(CREATE_UPLOAD_SESSION_TOKEN_TABLE_ID);
            return sqlFail(CREATE_UPLOAD_SESSION_TOKEN_TABLE_ID, error);
        }
        queryFree(CREATE_UPLOAD_SESSION_TOKEN_TABLE_ID);
    }

    if (CommonUtility::isVersionLower(dbFromVersionNumber, "3.4.4")) {
        LOG_DEBUG(_logger, "Upgrade < 3.4.4 Sync DB");

        if (!createAndPrepareRequest(PRAGMA_WRITABLE_SCHEMA_ID, PRAGMA_WRITABLE_SCHEMA)) return false;
        bool hasData = false;
        if (!queryNext(PRAGMA_WRITABLE_SCHEMA_ID, hasData)) {
            queryFree(PRAGMA_WRITABLE_SCHEMA_ID);
            return sqlFail(PRAGMA_WRITABLE_SCHEMA_ID, error);
        }
        queryFree(PRAGMA_WRITABLE_SCHEMA_ID);

        if (!createAndPrepareRequest(ALTER_NODE_TABLE_FK_ID, ALTER_NODE_TABLE_FK)) return false;
        if (!queryExec(ALTER_NODE_TABLE_FK_ID, errId, error)) {
            queryFree(ALTER_NODE_TABLE_FK_ID);
            return sqlFail(ALTER_NODE_TABLE_FK_ID, error);
        }
        queryFree(ALTER_NODE_TABLE_FK_ID);
    }

    if (!reinstateEncodingOfLocalNames(dbFromVersionNumber)) return false;

    LOG_DEBUG(_logger, "Upgrade of Sync DB successfully completed.");

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

bool SyncDb::updateNames(const char *queryId, const SyncName &localName, const SyncName &remoteName) {
    LOG_IF_FAIL(queryBindValue(queryId, 2, localName))

    SyncName remoteNormalizedName;
    if (!Utility::normalizedSyncName(remoteName, remoteNormalizedName)) {
        LOGW_DEBUG(_logger, L"Error in Utility::normalizedSyncName: " << Utility::formatSyncName(remoteName));
        return false;
    }
    LOG_IF_FAIL(queryBindValue(queryId, 3, remoteNormalizedName))
    return true;
}

bool SyncDb::checkNodeIds(const DbNode &node) {
    if (!node.hasLocalNodeId() || !node.hasRemoteNodeId()) {
        LOG_WARN(_logger, "nodeIdLocal and nodeIdRemote cannot be empty");
        return false;
    }

    if (node.nodeId() != _rootNode.nodeId() && !node.parentNodeId()) {
        LOG_WARN(_logger, "parentNodeId cannot be empty");
        return false;
    }

    return true;
}

bool SyncDb::insertNode(const DbNode &node, DbNodeId &dbNodeId, bool &constraintError) {
    const std::scoped_lock lock(_mutex);
    if (!checkNodeIds(node)) return false;
    invalidateCache();

    const char *queryId = INSERT_NODE_REQUEST_ID;
    int errId;
    std::string error;

    LOG_IF_FAIL(queryResetAndClearBindings(queryId))
    LOG_IF_FAIL(queryBindValue(queryId, 1, (node.parentNodeId() ? dbtype(node.parentNodeId().value()) : std::monostate())))
    if (!updateNames(queryId, node.nameLocal(), node.nameRemote())) return false;
    LOG_IF_FAIL(queryBindValue(queryId, 4, (node.nodeIdLocal() ? dbtype(node.nodeIdLocal().value()) : std::monostate())))
    LOG_IF_FAIL(queryBindValue(queryId, 5, (node.nodeIdRemote() ? dbtype(node.nodeIdRemote().value()) : std::monostate())))
    LOG_IF_FAIL(queryBindValue(queryId, 6, (node.created() ? dbtype(node.created().value()) : std::monostate())))
    LOG_IF_FAIL(
            queryBindValue(queryId, 7, (node.lastModifiedLocal() ? dbtype(node.lastModifiedLocal().value()) : std::monostate())))
    LOG_IF_FAIL(queryBindValue(queryId, 8,
                               (node.lastModifiedRemote() ? dbtype(node.lastModifiedRemote().value()) : std::monostate())))
    LOG_IF_FAIL(queryBindValue(queryId, 9, static_cast<int>(node.type())))
    LOG_IF_FAIL(queryBindValue(queryId, 10, node.size()))
    LOG_IF_FAIL(queryBindValue(queryId, 11, (node.checksum() ? dbtype(node.checksum().value()) : std::monostate())))
    LOG_IF_FAIL(queryBindValue(queryId, 12, static_cast<int>(node.status())))
    LOG_IF_FAIL(queryBindValue(queryId, 13, static_cast<int>(node.syncing())))

    if (!queryExecAndGetRowId(queryId, dbNodeId, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << queryId);
        constraintError = (errId == SQLITE_CONSTRAINT);
        return false;
    }

    return true;
}

bool SyncDb::insertNode(const DbNode &node) {
    DbNodeId dummyNodeId = 0;
    bool dummyConstraintError = false;
    return insertNode(node, dummyNodeId, dummyConstraintError);
}

bool SyncDb::updateNode(const DbNode &node, bool &found) {
    const std::scoped_lock lock(_mutex);
    if (!checkNodeIds(node)) return false;
    invalidateCache();

    SyncName remoteNormalizedName;
    if (!Utility::normalizedSyncName(node.nameRemote(), remoteNormalizedName)) {
        LOGW_DEBUG(_logger, L"Error in Utility::normalizedSyncName: " << Utility::formatSyncName(node.nameRemote()));
        return false;
    }

    LOG_IF_FAIL(queryResetAndClearBindings(UPDATE_NODE_REQUEST_ID))
    LOG_IF_FAIL(queryBindValue(UPDATE_NODE_REQUEST_ID, 1,
                               (node.parentNodeId() ? dbtype(node.parentNodeId().value()) : std::monostate())))
    LOG_IF_FAIL(queryBindValue(UPDATE_NODE_REQUEST_ID, 2, node.nameLocal()))
    LOG_IF_FAIL(queryBindValue(UPDATE_NODE_REQUEST_ID, 3, remoteNormalizedName))
    LOG_IF_FAIL(queryBindValue(UPDATE_NODE_REQUEST_ID, 4,
                               (node.nodeIdLocal() ? dbtype(node.nodeIdLocal().value()) : std::monostate())))
    LOG_IF_FAIL(queryBindValue(UPDATE_NODE_REQUEST_ID, 5,
                               (node.nodeIdRemote() ? dbtype(node.nodeIdRemote().value()) : std::monostate())))
    LOG_IF_FAIL(queryBindValue(UPDATE_NODE_REQUEST_ID, 6, (node.created() ? dbtype(node.created().value()) : std::monostate())))
    LOG_IF_FAIL(queryBindValue(UPDATE_NODE_REQUEST_ID, 7,
                               (node.lastModifiedLocal() ? dbtype(node.lastModifiedLocal().value()) : std::monostate())))
    LOG_IF_FAIL(queryBindValue(UPDATE_NODE_REQUEST_ID, 8,
                               (node.lastModifiedRemote() ? dbtype(node.lastModifiedRemote().value()) : std::monostate())))
    LOG_IF_FAIL(queryBindValue(UPDATE_NODE_REQUEST_ID, 9, static_cast<int>(node.type())))
    LOG_IF_FAIL(queryBindValue(UPDATE_NODE_REQUEST_ID, 10, node.size()))
    LOG_IF_FAIL(
            queryBindValue(UPDATE_NODE_REQUEST_ID, 11, (node.checksum() ? dbtype(node.checksum().value()) : std::monostate())))
    LOG_IF_FAIL(queryBindValue(UPDATE_NODE_REQUEST_ID, 12, static_cast<int>(node.status())))
    LOG_IF_FAIL(queryBindValue(UPDATE_NODE_REQUEST_ID, 13, static_cast<int>(node.syncing())))
    LOG_IF_FAIL(queryBindValue(UPDATE_NODE_REQUEST_ID, 14, node.nodeId()))

    int errId = -1;
    std::string error;
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
    const std::scoped_lock lock(_mutex);
    invalidateCache();

    int errId;
    std::string error;

    LOG_IF_FAIL(queryResetAndClearBindings(UPDATE_NODE_STATUS_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(UPDATE_NODE_STATUS_REQUEST_ID, 1, static_cast<int>(status)));
    LOG_IF_FAIL(queryBindValue(UPDATE_NODE_STATUS_REQUEST_ID, 2, nodeId));
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

bool SyncDb::updateNodeLocalName(DbNodeId nodeId, const SyncName &nameLocal, bool &found) {
    const std::scoped_lock lock(_mutex);
    invalidateCache();

    const char *queryId = UPDATE_NODE_NAME_LOCAL_REQUEST_ID;
    LOG_IF_FAIL(queryResetAndClearBindings(queryId));
    LOG_IF_FAIL(queryBindValue(queryId, 1, nameLocal));
    LOG_IF_FAIL(queryBindValue(queryId, 2, nodeId));

    int errId = -1;
    std::string error;
    if (!queryExec(queryId, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << queryId);
        return false;
    }
    if (numRowsAffected() == 1) {
        found = true;
    } else {
        LOG_WARN(_logger, "Error running query: " << queryId << " - num rows affected != 1");
        found = false;
    }

    return true;
}

bool SyncDb::updateNodesSyncing(bool syncing) {
    const std::scoped_lock lock(_mutex);
    invalidateCache();

    int errId;
    std::string error;

    LOG_IF_FAIL(queryResetAndClearBindings(UPDATE_NODES_SYNCING_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(UPDATE_NODES_SYNCING_REQUEST_ID, 1, static_cast<int>(syncing)));
    if (!queryExec(UPDATE_NODES_SYNCING_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << UPDATE_NODES_SYNCING_REQUEST_ID);
        return false;
    }

    return true;
}

bool SyncDb::updateNodeSyncing(DbNodeId nodeId, bool syncing, bool &found) {
    const std::scoped_lock lock(_mutex);
    invalidateCache();

    int errId;
    std::string error;

    LOG_IF_FAIL(queryResetAndClearBindings(UPDATE_NODE_SYNCING_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(UPDATE_NODE_SYNCING_REQUEST_ID, 1, static_cast<int>(syncing)));
    LOG_IF_FAIL(queryBindValue(UPDATE_NODE_SYNCING_REQUEST_ID, 2, nodeId));
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
    const std::scoped_lock lock(_mutex);
    invalidateCache();

    int errId;
    std::string error;

    LOG_IF_FAIL(queryResetAndClearBindings(DELETE_NODE_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(DELETE_NODE_REQUEST_ID, 1, nodeId));
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
    const std::scoped_lock lock(_mutex);

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_NODE_STATUS_BY_NODEID_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(SELECT_NODE_STATUS_BY_NODEID_REQUEST_ID, 1, nodeId));
    if (!queryNext(SELECT_NODE_STATUS_BY_NODEID_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_NODE_STATUS_BY_NODEID_REQUEST_ID);
        return false;
    }
    if (!found) {
        return true;
    }

    int intResult;
    LOG_IF_FAIL(queryIntValue(SELECT_NODE_STATUS_BY_NODEID_REQUEST_ID, 0, intResult));
    status = static_cast<SyncFileStatus>(intResult);

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_NODE_STATUS_BY_NODEID_REQUEST_ID));

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
    const std::scoped_lock lock(_mutex);

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_NODE_SYNCING_BY_NODEID_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(SELECT_NODE_SYNCING_BY_NODEID_REQUEST_ID, 1, nodeId));
    if (!queryNext(SELECT_NODE_SYNCING_BY_NODEID_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_NODE_SYNCING_BY_NODEID_REQUEST_ID);
        return false;
    }
    if (!found) {
        return true;
    }

    int intResult;
    LOG_IF_FAIL(queryIntValue(SELECT_NODE_SYNCING_BY_NODEID_REQUEST_ID, 0, intResult));
    syncing = static_cast<bool>(intResult);

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_NODE_SYNCING_BY_NODEID_REQUEST_ID));

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

bool SyncDb::node(ReplicaSide side, const NodeId &nodeId, DbNode &dbNode, bool &found) {
    const std::scoped_lock lock(_mutex);

    std::string id = (side == ReplicaSide::Local ? SELECT_NODE_BY_NODEIDLOCAL_ID : SELECT_NODE_BY_NODEIDDRIVE_ID);
    LOG_IF_FAIL(queryResetAndClearBindings(id));
    LOG_IF_FAIL(queryBindValue(id, 1, nodeId));
    if (!queryNext(id, found)) {
        LOG_WARN(_logger, "Error getting query result: "
                                  << id << (side == ReplicaSide::Local ? " - nodeIdLocal=" : " - nodeIdDrive=") << nodeId);
        return false;
    }
    if (!found) {
        return true;
    }

    DbNodeId dbNodeId;
    LOG_IF_FAIL(queryInt64Value(id, SELECT_NODE_BY_REPLICAID_DBID, dbNodeId));

    bool isNull = false;
    std::optional<DbNodeId> parentNodeId;
    LOG_IF_FAIL(queryIsNullValue(id, SELECT_NODE_BY_REPLICAID_PARENTID, isNull));
    if (isNull) {
        parentNodeId = std::nullopt;
    } else {
        DbNodeId dbParentNodeId;
        LOG_IF_FAIL(queryInt64Value(id, SELECT_NODE_BY_REPLICAID_PARENTID, dbParentNodeId));
        parentNodeId = std::make_optional(dbParentNodeId);
    }

    SyncName nameLocal;
    LOG_IF_FAIL(querySyncNameValue(id, SELECT_NODE_BY_REPLICAID_NAMELOCAL, nameLocal));
    SyncName nameDrive;
    LOG_IF_FAIL(querySyncNameValue(id, SELECT_NODE_BY_REPLICAID_NAMEDRIVE, nameDrive));

    NodeId idLocal;
    LOG_IF_FAIL(queryStringValue(id, SELECT_NODE_BY_REPLICAID_IDLOCAL, idLocal));
    NodeId idDrive;
    LOG_IF_FAIL(queryStringValue(id, SELECT_NODE_BY_REPLICAID_IDDRIVE, idDrive));

    std::optional<SyncTime> created;
    LOG_IF_FAIL(queryIsNullValue(id, SELECT_NODE_BY_REPLICAID_CREATED, isNull));
    if (isNull) {
        created = std::nullopt;
    } else {
        SyncTime timeTmp = 0;
        LOG_IF_FAIL(queryInt64Value(id, SELECT_NODE_BY_REPLICAID_CREATED, timeTmp));
        created = std::make_optional(timeTmp);
    }

    std::optional<SyncTime> lastModified;
    LOG_IF_FAIL(queryIsNullValue(id, SELECT_NODE_BY_REPLICAID_LASTMOD, isNull));
    if (isNull) {
        lastModified = std::nullopt;
    } else {
        SyncTime timeTmp = 0;
        LOG_IF_FAIL(queryInt64Value(id, SELECT_NODE_BY_REPLICAID_LASTMOD, timeTmp));
        lastModified = std::make_optional(timeTmp);
    }

    int intResult = 0;
    LOG_IF_FAIL(queryIntValue(id, SELECT_NODE_BY_REPLICAID_TYPE, intResult));
    auto type = static_cast<NodeType>(intResult);

    int64_t size = 0;
    LOG_IF_FAIL(queryInt64Value(id, SELECT_NODE_BY_REPLICAID_SIZE, size));

    std::optional<std::string> checksum;
    LOG_IF_FAIL(queryIsNullValue(id, SELECT_NODE_BY_REPLICAID_CHECKSUM, isNull));
    if (isNull) {
        checksum = std::nullopt;
    } else {
        std::string checksumTmp;
        LOG_IF_FAIL(queryStringValue(id, SELECT_NODE_BY_REPLICAID_CHECKSUM, checksumTmp));
        checksum = std::make_optional(checksumTmp);
    }

    LOG_IF_FAIL(queryIntValue(id, SELECT_NODE_BY_REPLICAID_STATUS, intResult));
    auto status = static_cast<SyncFileStatus>(intResult);

    LOG_IF_FAIL(queryIntValue(id, SELECT_NODE_BY_REPLICAID_SYNCING, intResult));
    auto syncing = static_cast<bool>(intResult);

    LOG_IF_FAIL(queryResetAndClearBindings(id));

    dbNode.setNodeId(dbNodeId);
    dbNode.setParentNodeId(parentNodeId);
    dbNode.setNameLocal(nameLocal);
    dbNode.setNameRemote(nameDrive);
    dbNode.setNodeIdLocal(idLocal);
    dbNode.setNodeIdRemote(idDrive);
    dbNode.setCreated(created);
    dbNode.setLastModifiedLocal(lastModified);
    dbNode.setLastModifiedRemote(lastModified);
    dbNode.setType(type);
    dbNode.setSize(size);
    if (side == ReplicaSide::Local) {
        dbNode.setLastModifiedLocal(lastModified);
    } else {
        dbNode.setLastModifiedRemote(lastModified);
    }
    dbNode.setChecksum(checksum);
    dbNode.setStatus(status);
    dbNode.setSyncing(syncing);

    return true;
}

bool SyncDb::dbIds(std::unordered_set<DbNodeId> &ids, bool &found) {
    const std::scoped_lock lock(_mutex);

    // Find root node
    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID));
    if (!queryNext(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID);
        return false;
    }
    if (!found) {
        return true;
    }
    DbNodeId nodeDbId;
    LOG_IF_FAIL(queryInt64Value(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, 0, nodeDbId));
    ids.insert(nodeDbId);

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID));

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

bool SyncDb::ids(std::unordered_set<NodeIds, NodeIds::HashFunction> &ids, bool &found) {
    const std::scoped_lock lock(_mutex);

    // Find root node
    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID));
    if (!queryNext(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID);
        return false;
    }
    if (!found) {
        return true;
    }
    NodeIds nodeIds;
    LOG_IF_FAIL(queryInt64Value(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, 0, nodeIds.dbNodeId));
    LOG_IF_FAIL(queryStringValue(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, 3, nodeIds.localNodeId));
    LOG_IF_FAIL(queryStringValue(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, 0, nodeIds.remoteNodeId));

    (void) ids.insert(nodeIds);

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID));

    // Push child nodes' ids
    try {
        if (!pushChildDbIds(nodeIds.dbNodeId, ids)) {
            return false;
        }
    } catch (...) {
        LOG_WARN(_logger, "Error in SyncDb::pushChildDbIds");
        return false;
    }

    return true;
}

bool SyncDb::path(DbNodeId dbNodeId, SyncPath &localPath, SyncPath &remotePath, bool &found) {
    const std::scoped_lock lock(_mutex);

    localPath.clear();
    remotePath.clear();
    found = false;

    // Local and remote name vector.
    std::vector<std::pair<SyncName, SyncName>> names; // first: local names, second: drive names

    std::string requestId = SELECT_ANCESTORS_NODES_REQUEST_ID;

    LOG_IF_FAIL(queryResetAndClearBindings(requestId));
    LOG_IF_FAIL(queryBindValue(requestId, 1, dbNodeId));

    for (;;) {
        bool hasNext = false;
        if (!queryNext(requestId, hasNext)) {
            LOG_WARN(_logger, "Error getting query result: " << requestId);
            return false;
        }
        if (!hasNext) {
            break;
        }

        SyncName nameLocal;
        LOG_IF_FAIL(querySyncNameValue(requestId, 0, nameLocal));
        SyncName nameDrive;
        LOG_IF_FAIL(querySyncNameValue(requestId, 1, nameDrive)); // Name on the remote drive.

        names.emplace_back(nameLocal, nameDrive);
    }

    LOG_IF_FAIL(queryResetAndClearBindings(requestId));

    found = !names.empty();

    // Construct path from names' vector
    for (auto nameIt = names.rbegin(); nameIt != names.rend(); ++nameIt) {
        localPath.append(nameIt->first);
        remotePath.append(nameIt->second);
    }

    return true;
}

bool SyncDb::node(DbNodeId dbNodeId, DbNode &dbNode, bool &found) {
    const std::scoped_lock lock(_mutex);

    std::string id = SELECT_NODE_BY_NODEID_FULL_ID;
    LOG_IF_FAIL(queryResetAndClearBindings(id));
    LOG_IF_FAIL(queryBindValue(id, 1, dbNodeId));
    if (!queryNext(id, found)) {
        LOG_WARN(_logger, "Error getting query result: " << id << " - dbNodeId=" << dbNodeId);
        return false;
    }
    if (!found) {
        return true;
    }

    bool isNull = false;
    std::optional<DbNodeId> parentNodeId;
    LOG_IF_FAIL(queryIsNullValue(id, SELECT_NODE_BY_NODEID_PARENTID, isNull));
    if (isNull) {
        parentNodeId = std::nullopt;
    } else {
        DbNodeId dbParentNodeId;
        LOG_IF_FAIL(queryInt64Value(id, SELECT_NODE_BY_NODEID_PARENTID, dbParentNodeId));
        parentNodeId = std::make_optional(dbParentNodeId);
    }

    SyncName nameLocal;
    LOG_IF_FAIL(querySyncNameValue(id, SELECT_NODE_BY_NODEID_NAMELOCAL, nameLocal));

    SyncName nameDrive;
    LOG_IF_FAIL(querySyncNameValue(id, SELECT_NODE_BY_NODEID_NAMEDRIVE, nameDrive));

    NodeId nodeIdLocal;
    LOG_IF_FAIL(queryStringValue(id, SELECT_NODE_BY_NODEID_IDLOCAL, nodeIdLocal));

    NodeId nodeIdDrive;
    LOG_IF_FAIL(queryStringValue(id, SELECT_NODE_BY_NODEID_IDDRIVE, nodeIdDrive));

    std::optional<SyncTime> created;
    LOG_IF_FAIL(queryIsNullValue(id, SELECT_NODE_BY_NODEID_CREATED, isNull));
    if (isNull) {
        created = std::nullopt;
    } else {
        SyncTime timeTmp;
        LOG_IF_FAIL(queryInt64Value(id, SELECT_NODE_BY_NODEID_CREATED, timeTmp));
        created = std::make_optional(timeTmp);
    }

    std::optional<SyncTime> lastModifiedLocal;
    LOG_IF_FAIL(queryIsNullValue(id, SELECT_NODE_BY_NODEID_LASTMODLOCAL, isNull));
    if (isNull) {
        lastModifiedLocal = std::nullopt;
    } else {
        SyncTime timeTmp;
        LOG_IF_FAIL(queryInt64Value(id, SELECT_NODE_BY_NODEID_LASTMODLOCAL, timeTmp));
        lastModifiedLocal = std::make_optional(timeTmp);
    }

    std::optional<SyncTime> lastModifiedDrive;
    LOG_IF_FAIL(queryIsNullValue(id, SELECT_NODE_BY_NODEID_LASTMODDRIVE, isNull));
    if (isNull) {
        lastModifiedDrive = std::nullopt;
    } else {
        SyncTime timeTmp;
        LOG_IF_FAIL(queryInt64Value(id, SELECT_NODE_BY_NODEID_LASTMODDRIVE, timeTmp));
        lastModifiedDrive = std::make_optional(timeTmp);
    }

    int intResult;
    LOG_IF_FAIL(queryIntValue(id, SELECT_NODE_BY_NODEID_TYPE, intResult));
    NodeType type = static_cast<NodeType>(intResult);

    int64_t size;
    LOG_IF_FAIL(queryInt64Value(id, SELECT_NODE_BY_NODEID_SIZE, size));

    std::optional<std::string> checksum;
    LOG_IF_FAIL(queryIsNullValue(id, SELECT_NODE_BY_NODEID_CHECKSUM, isNull));
    if (isNull) {
        checksum = std::nullopt;
    } else {
        std::string checksumTmp;
        LOG_IF_FAIL(queryStringValue(id, SELECT_NODE_BY_NODEID_CHECKSUM, checksumTmp));
        checksum = std::make_optional(checksumTmp);
    }

    LOG_IF_FAIL(queryIntValue(id, SELECT_NODE_BY_NODEID_STATUS, intResult));
    SyncFileStatus status = static_cast<SyncFileStatus>(intResult);

    LOG_IF_FAIL(queryIntValue(id, SELECT_NODE_BY_NODEID_SYNCING, intResult));
    bool syncing = static_cast<bool>(intResult);

    LOG_IF_FAIL(queryResetAndClearBindings(id));

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
    dbNode.setChecksum(checksum);
    dbNode.setStatus(status);
    dbNode.setSyncing(syncing);

    return true;
}

bool SyncDb::dbId(ReplicaSide side, const SyncPath &path, DbNodeId &dbNodeId, bool &found) {
    const std::scoped_lock lock(_mutex);

    // Find root node
    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID));
    if (!queryNext(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID);
        return false;
    }
    if (!found) {
        return true;
    }

    LOG_IF_FAIL(queryInt64Value(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, 0, dbNodeId));

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID));

    const auto &names = CommonUtility::splitSyncPath(path);
    if (!names.empty()) {
        // Find file node
        std::string id = (side == ReplicaSide::Local ? SELECT_NODE_BY_PARENTNODEID_AND_NAMELOCAL_REQUEST_ID
                                                     : SELECT_NODE_BY_PARENTNODEID_AND_NAMEDRIVE_REQUEST_ID);
        for (const auto &name: names) {
            LOG_IF_FAIL(queryResetAndClearBindings(id));
            LOG_IF_FAIL(queryBindValue(id, 1, dbNodeId));
            LOG_IF_FAIL(queryBindValue(id, 2, name));
            if (!queryNext(id, found)) {
                LOGW_WARN(_logger, L"Error getting query result: " << CommonUtility::s2ws(id) << L" - parentNodeId="
                                                                   << std::to_wstring(dbNodeId) << L" and name="
                                                                   << Utility::formatSyncName(name));
                return false;
            }
            if (!found) {
                return true;
            }
            LOG_IF_FAIL(queryInt64Value(id, 0, dbNodeId));

            LOG_IF_FAIL(queryResetAndClearBindings(id));
        }
    }

    found = true;

    return true;
}

bool SyncDb::clearNodes() {
    const std::scoped_lock lock(_mutex);
    invalidateCache();

    int errId;
    std::string error;

    LOG_IF_FAIL(queryResetAndClearBindings(DELETE_NODES_BUT_ROOT_REQUEST_ID));
    if (!queryExec(DELETE_NODES_BUT_ROOT_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << DELETE_NODES_BUT_ROOT_REQUEST_ID);
        return false;
    }

    return true;
}

// Returns the id of the object from its path
// path is relative to the root directory
bool SyncDb::id(ReplicaSide side, const SyncPath &path, std::optional<NodeId> &nodeId, bool &found) {
    const std::scoped_lock lock(_mutex);

    // Find root node
    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID));
    if (!queryNext(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID);
        return false;
    }
    if (!found) {
        return true;
    }
    DbNodeId nodeDbId;
    LOG_IF_FAIL(queryInt64Value(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, 0, nodeDbId));

    const auto &itemNames = CommonUtility::splitSyncPath(path);
    if (!itemNames.empty()) {
        LOG_IF_FAIL(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID));

        std::string queryId = (side == ReplicaSide::Local ? SELECT_NODE_BY_PARENTNODEID_AND_NAMELOCAL_REQUEST_ID
                                                          : SELECT_NODE_BY_PARENTNODEID_AND_NAMEDRIVE_REQUEST_ID);
        // Find file node
        for (const auto &name: itemNames) {
            LOG_IF_FAIL(queryResetAndClearBindings(queryId));
            LOG_IF_FAIL(queryBindValue(queryId, 1, nodeDbId));
            LOG_IF_FAIL(queryBindValue(queryId, 2, name));
            if (!queryNext(queryId, found)) {
                LOGW_WARN(_logger, L"Error getting query result: " << CommonUtility::s2ws(queryId) << L" - parentNodeId=" << nodeDbId
                                                                   << L" and name=" << Utility::formatSyncName(name));
                return false;
            }
            if (!found) {
                return true;
            }
            LOG_IF_FAIL(queryInt64Value(queryId, 0, nodeDbId));
        }
        bool isNull = false;
        LOG_IF_FAIL(queryIsNullValue(queryId, 1, isNull));
        if (isNull) {
            nodeId = std::nullopt;
        } else {
            NodeId idTmp;
            LOG_IF_FAIL(queryStringValue(queryId, 1, idTmp));
            nodeId = std::make_optional(idTmp);
        }

        LOG_IF_FAIL(queryResetAndClearBindings(queryId));
    } else {
        bool isNull = false;
        LOG_IF_FAIL(queryIsNullValue(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, (side == ReplicaSide::Local ? 3 : 4), isNull));
        if (isNull) {
            nodeId = std::nullopt;
        } else {
            NodeId idTmp;
            LOG_IF_FAIL(
                    queryStringValue(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, (side == ReplicaSide::Local ? 3 : 4), idTmp));
            nodeId = std::make_optional(idTmp);
        }

        LOG_IF_FAIL(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID));
    }

    found = true;
    return true;
}

// Returns the type of the object with ID nodeId
bool SyncDb::type(ReplicaSide side, const NodeId &nodeId, NodeType &type, bool &found) {
    const std::scoped_lock lock(_mutex);

    std::string id = (side == ReplicaSide::Local ? SELECT_NODE_BY_NODEIDLOCAL_ID : SELECT_NODE_BY_NODEIDDRIVE_ID);
    LOG_IF_FAIL(queryResetAndClearBindings(id));
    LOG_IF_FAIL(queryBindValue(id, 1, nodeId));
    if (!queryNext(id, found)) {
        LOG_WARN(_logger, "Error getting query result: "
                                  << id << (side == ReplicaSide::Local ? " - nodeIdLocal=" : " - nodeIdDrive=") << nodeId);
        return false;
    }
    if (!found) {
        return true;
    }
    int result;
    LOG_IF_FAIL(queryIntValue(id, SELECT_NODE_BY_REPLICAID_TYPE, result));
    type = static_cast<NodeType>(result);

    LOG_IF_FAIL(queryResetAndClearBindings(id));

    return true;
}

bool SyncDb::size(ReplicaSide side, const NodeId &nodeId, int64_t &size, bool &found) {
    const std::scoped_lock lock(_mutex);

    std::string id = (side == ReplicaSide::Local ? SELECT_NODE_BY_NODEIDLOCAL_ID : SELECT_NODE_BY_NODEIDDRIVE_ID);
    LOG_IF_FAIL(queryResetAndClearBindings(id));
    LOG_IF_FAIL(queryBindValue(id, 1, nodeId));
    if (!queryNext(id, found)) {
        LOG_WARN(_logger, "Error getting query result: "
                                  << id << (side == ReplicaSide::Local ? " - nodeIdLocal=" : " - nodeIdDrive=") << nodeId);
        return false;
    }
    if (!found) {
        return true;
    }
    LOG_IF_FAIL(queryInt64Value(id, SELECT_NODE_BY_REPLICAID_SIZE, size));

    LOG_IF_FAIL(queryResetAndClearBindings(id));

    return true;
}

bool SyncDb::created(ReplicaSide side, const NodeId &nodeId, std::optional<SyncTime> &time, bool &found) {
    const std::scoped_lock lock(_mutex);

    std::string id = (side == ReplicaSide::Local ? SELECT_NODE_BY_NODEIDLOCAL_ID : SELECT_NODE_BY_NODEIDDRIVE_ID);
    LOG_IF_FAIL(queryResetAndClearBindings(id));
    LOG_IF_FAIL(queryBindValue(id, 1, nodeId));
    if (!queryNext(id, found)) {
        LOG_WARN(_logger, "Error getting query result: "
                                  << id << (side == ReplicaSide::Local ? " - nodeIdLocal=" : " - nodeIdDrive=") << nodeId);
        return false;
    }
    if (!found) {
        return true;
    }
    bool isNull = false;
    LOG_IF_FAIL(queryIsNullValue(id, SELECT_NODE_BY_REPLICAID_CREATED, isNull));
    if (isNull) {
        time = std::nullopt;
    } else {
        SyncTime timeTmp;
        LOG_IF_FAIL(queryInt64Value(id, SELECT_NODE_BY_REPLICAID_CREATED, timeTmp));
        time = std::make_optional(timeTmp);
    }

    LOG_IF_FAIL(queryResetAndClearBindings(id));

    return true;
}

// Returns the lastmodified date of the object with ID nodeId
bool SyncDb::lastModified(ReplicaSide side, const NodeId &nodeId, std::optional<SyncTime> &time, bool &found) {
    const std::scoped_lock lock(_mutex);

    std::string id = (side == ReplicaSide::Local ? SELECT_NODE_BY_NODEIDLOCAL_ID : SELECT_NODE_BY_NODEIDDRIVE_ID);
    LOG_IF_FAIL(queryResetAndClearBindings(id));
    LOG_IF_FAIL(queryBindValue(id, 1, nodeId));
    if (!queryNext(id, found)) {
        LOG_WARN(_logger, "Error getting query result: "
                                  << id << (side == ReplicaSide::Local ? " - nodeIdLocal=" : " - nodeIdDrive=") << nodeId);
        return false;
    }
    if (!found) {
        return true;
    }
    bool isNull = false;
    LOG_IF_FAIL(queryIsNullValue(id, SELECT_NODE_BY_REPLICAID_LASTMOD, isNull));
    if (isNull) {
        time = std::nullopt;
    } else {
        SyncTime timeTmp;
        LOG_IF_FAIL(queryInt64Value(id, SELECT_NODE_BY_REPLICAID_LASTMOD, timeTmp));
        time = std::make_optional(timeTmp);
    }

    LOG_IF_FAIL(queryResetAndClearBindings(id));

    return true;
}

// Returns the parent directory ID of the object with ID nodeId
bool SyncDb::parent(ReplicaSide side, const NodeId &nodeId, NodeId &parentNodeid, bool &found) {
    const std::scoped_lock lock(_mutex);

    std::string id = (side == ReplicaSide::Local ? SELECT_NODE_BY_NODEIDLOCAL_ID : SELECT_NODE_BY_NODEIDDRIVE_ID);
    LOG_IF_FAIL(queryResetAndClearBindings(id));
    LOG_IF_FAIL(queryBindValue(id, 1, nodeId));
    if (!queryNext(id, found)) {
        LOG_WARN(_logger, "Error getting query result: "
                                  << id << (side == ReplicaSide::Local ? " - nodeIdLocal=" : " - nodeIdDrive=") << nodeId);
        return false;
    }
    if (!found) {
        return true;
    }
    DbNodeId parentNodeDbId;
    LOG_IF_FAIL(queryInt64Value(id, SELECT_NODE_BY_REPLICAID_PARENTID, parentNodeDbId));
    LOG_IF_FAIL(queryResetAndClearBindings(id));

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_NODE_BY_NODEID_LITE_ID));
    LOG_IF_FAIL(queryBindValue(SELECT_NODE_BY_NODEID_LITE_ID, 1, parentNodeDbId));
    if (!queryNext(SELECT_NODE_BY_NODEID_LITE_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_NODE_BY_NODEID_LITE_ID
                                                         << (side == ReplicaSide::Local ? " - nodeIdLocal=" : " - nodeIdDrive=")
                                                         << nodeId);
        return false;
    }
    if (!found) {
        return true;
    }
    LOG_IF_FAIL(queryStringValue(SELECT_NODE_BY_NODEID_LITE_ID, (side == ReplicaSide::Local ? 3 : 4), parentNodeid));
    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_NODE_BY_NODEID_LITE_ID));

    return true;
}

// Returns the path from the root node to the node with ID nodeId, concatenating the respective names of the nodes along the
// traversal-path
bool SyncDb::path(ReplicaSide side, const NodeId &nodeId, SyncPath &path, bool &found) {
    const std::scoped_lock lock(_mutex);

    path.clear();
    found = false;

    std::vector<SyncName> itemNames;

    const std::string requestId =
            (side == ReplicaSide::Local ? SELECT_ANCESTORS_NODES_LOCAL_REQUEST_ID : SELECT_ANCESTORS_NODES_DRIVE_REQUEST_ID);

    LOG_IF_FAIL(queryResetAndClearBindings(requestId));
    LOG_IF_FAIL(queryBindValue(requestId, 1, nodeId));

    for (;;) {
        bool hasNext = false;
        if (!queryNext(requestId, hasNext)) {
            LOG_WARN(_logger, "Error getting query result: " << requestId);
            return false;
        }
        if (!hasNext) {
            break;
        }

        SyncName name;
        LOG_IF_FAIL(querySyncNameValue(requestId, side == ReplicaSide::Local ? 0 : 1, name));

        (void) itemNames.emplace_back(name);
    }

    LOG_IF_FAIL(queryResetAndClearBindings(requestId));

    found = !itemNames.empty();

    // Construct path from itemNames' vector
    for (auto nameIt = itemNames.rbegin(); nameIt != itemNames.rend(); ++nameIt) {
        path.append(*nameIt);
    }

    return true;
}

// Returns the name of the object with ID nodeId
bool SyncDb::name(ReplicaSide side, const NodeId &nodeId, SyncName &name, bool &found) {
    const std::scoped_lock lock(_mutex);

    std::string id = (side == ReplicaSide::Local ? SELECT_NODE_BY_NODEIDLOCAL_ID : SELECT_NODE_BY_NODEIDDRIVE_ID);
    LOG_IF_FAIL(queryResetAndClearBindings(id));
    LOG_IF_FAIL(queryBindValue(id, 1, nodeId));
    if (!queryNext(id, found)) {
        LOG_WARN(_logger, "Error getting query result: "
                                  << id << (side == ReplicaSide::Local ? " - nodeIdLocal=" : " - nodeIdDrive=") << nodeId);
        return false;
    }
    if (!found) {
        return true;
    }
    LOG_IF_FAIL(querySyncNameValue(
            id, side == ReplicaSide::Local ? SELECT_NODE_BY_REPLICAID_NAMELOCAL : SELECT_NODE_BY_REPLICAID_NAMEDRIVE, name));
    LOG_IF_FAIL(queryResetAndClearBindings(id));

    return true;
}

// Returns the checksum of the object with ID nodeId
bool SyncDb::checksum(ReplicaSide side, const NodeId &nodeId, std::optional<std::string> &checksum, bool &found) {
    const std::scoped_lock lock(_mutex);

    std::string id = (side == ReplicaSide::Local ? SELECT_NODE_BY_NODEIDLOCAL_ID : SELECT_NODE_BY_NODEIDDRIVE_ID);
    LOG_IF_FAIL(queryResetAndClearBindings(id));
    LOG_IF_FAIL(queryBindValue(id, 1, nodeId));
    if (!queryNext(id, found)) {
        LOG_WARN(_logger, "Error getting query result: "
                                  << id << (side == ReplicaSide::Local ? " - nodeIdLocal=" : " - nodeIdDrive=") << nodeId);
        return false;
    }
    if (!found) {
        return true;
    }
    bool isNull = false;
    LOG_IF_FAIL(queryIsNullValue(id, SELECT_NODE_BY_REPLICAID_CHECKSUM, isNull));
    if (isNull) {
        checksum = std::nullopt;
    } else {
        std::string checksumTmp;
        LOG_IF_FAIL(queryStringValue(id, SELECT_NODE_BY_REPLICAID_CHECKSUM, checksumTmp));
        checksum = std::make_optional(checksumTmp);
    }
    LOG_IF_FAIL(queryResetAndClearBindings(id));

    return true;
}

// Returns the list of IDs contained in snapshot
bool SyncDb::ids(ReplicaSide side, std::vector<NodeId> &ids, bool &found) {
    const std::scoped_lock lock(_mutex);

    // Find root node
    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID));
    if (!queryNext(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID);
        return false;
    }
    if (!found) {
        return true;
    }
    DbNodeId nodeDbId;
    LOG_IF_FAIL(queryInt64Value(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, 0, nodeDbId));

    std::string id;
    LOG_IF_FAIL(queryStringValue(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, (side == ReplicaSide::Local ? 3 : 4), id));
    ids.push_back(id);

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID));

    // Push child nodes' ids
    if (!pushChildIds(side, nodeDbId, ids)) {
        return false;
    }

    return true;
}

bool SyncDb::ids(ReplicaSide side, NodeSet &ids, bool &found) {
    const std::scoped_lock lock(_mutex);

    // Find root node
    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID));
    if (!queryNext(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID);
        return false;
    }
    if (!found) {
        return true;
    }
    DbNodeId nodeDbId;
    LOG_IF_FAIL(queryInt64Value(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, 0, nodeDbId));

    std::string id;
    LOG_IF_FAIL(queryStringValue(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID, (side == ReplicaSide::Local ? 3 : 4), id));
    ids.insert(id);

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_ROOT_REQUEST_ID));

    // Push child nodes' ids
    if (!pushChildIds(side, nodeDbId, ids)) {
        return false;
    }

    return true;
}

// Returns whether node with ID nodeId1 is an ancestor of the node with ID nodeId2 in snapshot
bool SyncDb::ancestor(ReplicaSide side, const NodeId &nodeId1, const NodeId &nodeId2, bool &ret, bool &found) {
    const std::scoped_lock lock(_mutex);

    if (nodeId1 == nodeId2) {
        ret = true;
        return true;
    }

    // Find node 2
    std::string id = (side == ReplicaSide::Local ? SELECT_NODE_BY_NODEIDLOCAL_ID : SELECT_NODE_BY_NODEIDDRIVE_ID);
    LOG_IF_FAIL(queryResetAndClearBindings(id));
    LOG_IF_FAIL(queryBindValue(id, 1, nodeId2));
    if (!queryNext(id, found)) {
        LOG_WARN(_logger, "Error getting query result: "
                                  << id << (side == ReplicaSide::Local ? " - nodeIdLocal=" : " - nodeIdDrive=") << nodeId2);
        return false;
    }
    if (!found) {
        return true;
    }
    bool parentNodeDbIdIsNull;
    LOG_IF_FAIL(queryIsNullValue(id, SELECT_NODE_BY_REPLICAID_PARENTID, parentNodeDbIdIsNull));
    if (parentNodeDbIdIsNull) {
        ret = false;
        return true;
    }

    // Loop on the ancestors
    DbNodeId parentNodeDbId;
    LOG_IF_FAIL(queryInt64Value(id, SELECT_NODE_BY_REPLICAID_PARENTID, parentNodeDbId));
    LOG_IF_FAIL(queryResetAndClearBindings(id));
    do {
        LOG_IF_FAIL(queryResetAndClearBindings(SELECT_NODE_BY_NODEID_LITE_ID));
        LOG_IF_FAIL(queryBindValue(SELECT_NODE_BY_NODEID_LITE_ID, 1, parentNodeDbId));
        if (!queryNext(SELECT_NODE_BY_NODEID_LITE_ID, found)) {
            LOG_WARN(_logger, "Error getting query result: " << SELECT_NODE_BY_NODEID_LITE_ID);
            return false;
        }
        if (!found) {
            return true;
        }

        bool nodeIdIsNull;
        LOG_IF_FAIL(queryIsNullValue(SELECT_NODE_BY_NODEID_LITE_ID, (side == ReplicaSide::Local ? 3 : 4), nodeIdIsNull));
        if (nodeIdIsNull) {
            // Database inconsistency
            LOG_WARN(_logger, "Database inconsistency: node with dbId=" << parentNodeDbId << " is not in snapshot="
                                                                        << (side == ReplicaSide::Local ? "Local" : "Drive")
                                                                        << " while child is");
            found = false;
            return true;
        }
        NodeId nodeId;
        LOG_IF_FAIL(queryStringValue(SELECT_NODE_BY_NODEID_LITE_ID, (side == ReplicaSide::Local ? 3 : 4), nodeId));
        if (nodeId == nodeId1) {
            ret = true;
            return true;
        }

        LOG_IF_FAIL(queryIsNullValue(SELECT_NODE_BY_NODEID_LITE_ID, 0, parentNodeDbIdIsNull));
        if (!parentNodeDbIdIsNull) {
            LOG_IF_FAIL(queryInt64Value(SELECT_NODE_BY_NODEID_LITE_ID, 0, parentNodeDbId));
        }

        LOG_IF_FAIL(queryResetAndClearBindings(SELECT_NODE_BY_NODEID_LITE_ID));
    } while (!parentNodeDbIdIsNull);

    ret = false;
    return true;
}

// Returns database ID for the ID nodeId of the snapshot from replica `side`
bool SyncDb::dbId(ReplicaSide side, const NodeId &nodeId, DbNodeId &dbNodeId, bool &found) {
    const std::scoped_lock lock(_mutex);
    found = false;
    dbNodeId = 0;
    if (side == ReplicaSide::Unknown) {
        LOG_ERROR(_logger, "Call to SyncDb::dbId with snapshot=='ReplicaSide::Unknown'");
        return false;
    }

    std::string id = (side == ReplicaSide::Local ? SELECT_NODE_BY_NODEIDLOCAL_ID : SELECT_NODE_BY_NODEIDDRIVE_ID);
    LOG_IF_FAIL(queryResetAndClearBindings(id));
    LOG_IF_FAIL(queryBindValue(id, 1, nodeId));
    if (!queryNext(id, found)) {
        LOG_WARN(_logger, "Error getting query result: "
                                  << id << (side == ReplicaSide::Local ? " - nodeIdLocal=" : " - nodeIdDrive=") << nodeId);
        return false;
    }
    if (!found) {
        return true;
    }
    LOG_IF_FAIL(queryInt64Value(id, SELECT_NODE_BY_REPLICAID_DBID, dbNodeId));
    LOG_IF_FAIL(queryResetAndClearBindings(id));

    return true;
}

// Returns the ID of the `side` snapshot for the database ID dbNodeId
bool SyncDb::id(ReplicaSide side, DbNodeId dbNodeId, NodeId &nodeId, bool &found) {
    const std::scoped_lock lock(_mutex);

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_NODE_BY_NODEID_LITE_ID));
    LOG_IF_FAIL(queryBindValue(SELECT_NODE_BY_NODEID_LITE_ID, 1, dbNodeId));
    if (!queryNext(SELECT_NODE_BY_NODEID_LITE_ID, found)) {
        LOG_WARN(_logger,
                 "Error getting query result: " << SELECT_NODE_BY_NODEID_LITE_ID << " - nodeId=" << std::to_string(dbNodeId));
        return false;
    }
    if (!found) {
        return true;
    }
    LOG_IF_FAIL(queryStringValue(SELECT_NODE_BY_NODEID_LITE_ID, (side == ReplicaSide::Local ? 3 : 4), nodeId));
    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_NODE_BY_NODEID_LITE_ID));

    return true;
}

bool SyncDb::correspondingNodeId(ReplicaSide side, const NodeId &nodeIdIn, NodeId &nodeIdOut, bool &found) {
    DbNodeId dbNodeId;
    if (!dbId(side, nodeIdIn, dbNodeId, found)) {
        return false;
    }
    if (!found) {
        return true;
    }

    return id(otherSide(side), dbNodeId, nodeIdOut, found);
}

bool SyncDb::updateAllSyncNodes(SyncNodeType type, const NodeSet &nodeIdSet) {
    const std::scoped_lock lock(_mutex);
    int errId;
    std::string error;

    startTransaction();

    // Delete existing SyncNodes
    LOG_IF_FAIL(queryResetAndClearBindings(DELETE_ALL_SYNC_NODE_BY_TYPE_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(DELETE_ALL_SYNC_NODE_BY_TYPE_REQUEST_ID, 1, toInt(type)));
    if (!queryExec(DELETE_ALL_SYNC_NODE_BY_TYPE_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << DELETE_ALL_SYNC_NODE_BY_TYPE_REQUEST_ID);
        rollbackTransaction();
        return false;
    }

    // Insert new SyncNodes
    for (const NodeId &nodeId: nodeIdSet) {
        LOG_IF_FAIL(queryResetAndClearBindings(INSERT_SYNC_NODE_REQUEST_ID));
        LOG_IF_FAIL(queryBindValue(INSERT_SYNC_NODE_REQUEST_ID, 1, nodeId));
        LOG_IF_FAIL(queryBindValue(INSERT_SYNC_NODE_REQUEST_ID, 2, toInt(type)));
        if (!queryExec(INSERT_SYNC_NODE_REQUEST_ID, errId, error)) {
            LOG_WARN(_logger, "Error running query: " << INSERT_SYNC_NODE_REQUEST_ID);
            rollbackTransaction();
            return false;
        }
    }

    commitTransaction();

    return true;
}

bool SyncDb::selectAllSyncNodes(SyncNodeType type, NodeSet &nodeIdSet) {
    const std::scoped_lock lock(_mutex);

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ALL_SYNC_NODE_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(SELECT_ALL_SYNC_NODE_REQUEST_ID, 1, toInt(type)));
    bool found;
    for (;;) {
        if (!queryNext(SELECT_ALL_SYNC_NODE_REQUEST_ID, found)) {
            LOG_WARN(_logger, "Error getting query result: " << SELECT_ALL_SYNC_NODE_REQUEST_ID);
            return false;
        }
        if (!found) {
            break;
        }

        NodeId nodeId;
        LOG_IF_FAIL(queryStringValue(SELECT_ALL_SYNC_NODE_REQUEST_ID, 0, nodeId));

        nodeIdSet.insert(nodeId);
    }
    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ALL_SYNC_NODE_REQUEST_ID));

    return true;
}

bool SyncDb::insertUploadSessionToken(const UploadSessionToken &uploadSessionToken, int64_t &uploadSessionTokenDbId) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    LOG_IF_FAIL(queryResetAndClearBindings(INSERT_UPLOAD_SESSION_TOKEN_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(INSERT_UPLOAD_SESSION_TOKEN_REQUEST_ID, 1, uploadSessionToken.token()));
    if (!queryExecAndGetRowId(INSERT_UPLOAD_SESSION_TOKEN_REQUEST_ID, uploadSessionTokenDbId, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << INSERT_UPLOAD_SESSION_TOKEN_REQUEST_ID);
        return false;
    }

    return true;
}

bool SyncDb::deleteUploadSessionTokenByDbId(int64_t dbId, bool &found) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    LOG_IF_FAIL(queryResetAndClearBindings(DELETE_UPLOAD_SESSION_TOKEN_BY_DBID_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(DELETE_UPLOAD_SESSION_TOKEN_BY_DBID_REQUEST_ID, 1, dbId));
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
    const std::scoped_lock lock(_mutex);

    uploadSessionTokenList.clear();

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ALL_UPLOAD_SESSION_TOKEN_REQUEST_ID));
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
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_UPLOAD_SESSION_TOKEN_REQUEST_ID, 0, id));
        std::string token;
        LOG_IF_FAIL(queryStringValue(SELECT_ALL_UPLOAD_SESSION_TOKEN_REQUEST_ID, 1, token));

        uploadSessionTokenList.emplace_back(id, token);
    }
    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ALL_UPLOAD_SESSION_TOKEN_REQUEST_ID));

    return true;
}

bool SyncDb::deleteAllUploadSessionToken() {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    LOG_IF_FAIL(queryResetAndClearBindings(DELETE_ALL_UPLOAD_SESSION_TOKEN_REQUEST_ID));
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

SyncDbRevision SyncDb::revision() const {
    const std::scoped_lock lock(_mutex);
    return _revision;
}

bool SyncDb::pushChildIds(ReplicaSide side, DbNodeId parentNodeDbId, std::vector<NodeId> &ids) {
    const std::scoped_lock lock(_mutex);
    std::queue<DbNodeId> dbNodeIdQueue;
    dbNodeIdQueue.push(parentNodeDbId);

    while (!dbNodeIdQueue.empty()) {
        DbNodeId dbNodeId = dbNodeIdQueue.front();
        dbNodeIdQueue.pop();

        LOG_IF_FAIL(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID));
        LOG_IF_FAIL(queryBindValue(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, 1, dbNodeId));
        for (;;) {
            bool found;
            if (!queryNext(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, found)) {
                LOG_WARN(_logger, "Error getting query result: " << SELECT_NODE_BY_PARENTNODEID_REQUEST_ID
                                                                 << " - parentNodeId=" << std::to_string(dbNodeId));
                return false;
            }
            if (!found) {
                break;
            }
            bool nodeIdIsNull;
            LOG_IF_FAIL(
                    queryIsNullValue(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, (side == ReplicaSide::Local ? 3 : 4), nodeIdIsNull));
            if (!nodeIdIsNull) {
                // The node exists in the snapshot
                DbNodeId dbChildNodeId;
                LOG_IF_FAIL(queryInt64Value(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, 0, dbChildNodeId));

                NodeId childNodeId;
                LOG_IF_FAIL(queryStringValue(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, (side == ReplicaSide::Local ? 3 : 4),
                                             childNodeId));
                ids.push_back(childNodeId);

                int type;
                LOG_IF_FAIL(queryIntValue(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, 5, type));
                if (static_cast<NodeType>(type) == NodeType::Directory) {
                    dbNodeIdQueue.push(dbChildNodeId);
                }
            }
        }
        LOG_IF_FAIL(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID));
    }

    return true;
}

bool SyncDb::pushChildIds(ReplicaSide side, DbNodeId parentNodeDbId, NodeSet &ids) {
    const std::scoped_lock lock(_mutex);
    std::queue<DbNodeId> dbNodeIdQueue;
    dbNodeIdQueue.push(parentNodeDbId);

    while (!dbNodeIdQueue.empty()) {
        DbNodeId dbNodeId = dbNodeIdQueue.front();
        dbNodeIdQueue.pop();

        LOG_IF_FAIL(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID));
        LOG_IF_FAIL(queryBindValue(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, 1, dbNodeId));
        for (;;) {
            bool found;
            if (!queryNext(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, found)) {
                LOG_WARN(_logger, "Error getting query result: " << SELECT_NODE_BY_PARENTNODEID_REQUEST_ID
                                                                 << " - parentNodeId=" << std::to_string(dbNodeId));
                return false;
            }
            if (!found) {
                break;
            }
            bool nodeIdIsNull;
            LOG_IF_FAIL(
                    queryIsNullValue(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, (side == ReplicaSide::Local ? 3 : 4), nodeIdIsNull));
            if (!nodeIdIsNull) {
                // The node exists in the snapshot
                DbNodeId dbChildNodeId;
                LOG_IF_FAIL(queryInt64Value(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, 0, dbChildNodeId));

                NodeId childNodeId;
                LOG_IF_FAIL(queryStringValue(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, (side == ReplicaSide::Local ? 3 : 4),
                                             childNodeId));
                ids.insert(childNodeId);

                int type;
                LOG_IF_FAIL(queryIntValue(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, 5, type));
                if (static_cast<NodeType>(type) == NodeType::Directory) {
                    dbNodeIdQueue.push(dbChildNodeId);
                }
            }
        }
        LOG_IF_FAIL(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID));
    }

    return true;
}

bool SyncDb::selectAllRenamedNodes(std::vector<DbNode> &dbNodeList, bool onlyColon) {
    const std::scoped_lock lock(_mutex);

    dbNodeList.clear();

    std::string requestId = onlyColon ? SELECT_ALL_RENAMED_COLON_NODES_REQUEST_ID : SELECT_ALL_RENAMED_NODES_REQUEST_ID;

    LOG_IF_FAIL(queryResetAndClearBindings(requestId));

    bool found;
    for (;;) {
        if (!queryNext(requestId, found)) {
            LOG_WARN(_logger, "Error getting query result: " << requestId);
            return false;
        }
        if (!found) {
            break;
        }

        DbNodeId dbNodeId;
        LOG_IF_FAIL(queryInt64Value(requestId, 0, dbNodeId));

        bool isNull = false;
        std::optional<DbNodeId> parentNodeId;
        LOG_IF_FAIL(queryIsNullValue(requestId, 1, isNull));
        if (isNull) {
            parentNodeId = std::nullopt;
        } else {
            DbNodeId dbParentNodeId;
            LOG_IF_FAIL(queryInt64Value(requestId, 1, dbParentNodeId));
            parentNodeId = std::make_optional(dbParentNodeId);
        }

        SyncName nameLocal;
        LOG_IF_FAIL(querySyncNameValue(requestId, 2, nameLocal));
        SyncName nameDrive;
        LOG_IF_FAIL(querySyncNameValue(requestId, 3, nameDrive));

        std::optional<NodeId> nodeIdLocal;
        LOG_IF_FAIL(queryIsNullValue(requestId, 4, isNull));
        if (isNull) {
            nodeIdLocal = std::nullopt;
        } else {
            NodeId nodeIdLocalTmp;
            LOG_IF_FAIL(queryStringValue(requestId, 4, nodeIdLocalTmp));
            nodeIdLocal = std::make_optional(nodeIdLocalTmp);
        }

        std::optional<NodeId> nodeIdDrive;
        LOG_IF_FAIL(queryIsNullValue(requestId, 5, isNull));
        if (isNull) {
            nodeIdDrive = std::nullopt;
        } else {
            NodeId nodeIdDriveTmp;
            LOG_IF_FAIL(queryStringValue(requestId, 5, nodeIdDriveTmp));
            nodeIdDrive = std::make_optional(nodeIdDriveTmp);
        }

        std::optional<SyncTime> created;
        LOG_IF_FAIL(queryIsNullValue(requestId, 6, isNull));
        if (isNull) {
            created = std::nullopt;
        } else {
            SyncTime timeTmp;
            LOG_IF_FAIL(queryInt64Value(requestId, 6, timeTmp));
            created = std::make_optional(timeTmp);
        }

        std::optional<SyncTime> lastModifiedLocal;
        LOG_IF_FAIL(queryIsNullValue(requestId, 7, isNull));
        if (isNull) {
            lastModifiedLocal = std::nullopt;
        } else {
            SyncTime timeTmp;
            LOG_IF_FAIL(queryInt64Value(requestId, 7, timeTmp));
            lastModifiedLocal = std::make_optional(timeTmp);
        }

        std::optional<SyncTime> lastModifiedDrive;
        LOG_IF_FAIL(queryIsNullValue(requestId, 8, isNull));
        if (isNull) {
            lastModifiedDrive = std::nullopt;
        } else {
            SyncTime timeTmp;
            LOG_IF_FAIL(queryInt64Value(requestId, 8, timeTmp));
            lastModifiedDrive = std::make_optional(timeTmp);
        }

        int intResult;
        LOG_IF_FAIL(queryIntValue(requestId, 9, intResult));
        NodeType type = static_cast<NodeType>(intResult);

        int64_t size;
        LOG_IF_FAIL(queryInt64Value(requestId, 10, size));

        std::optional<std::string> checksum;
        LOG_IF_FAIL(queryIsNullValue(requestId, 11, isNull));
        if (isNull) {
            checksum = std::nullopt;
        } else {
            std::string checksumTmp;
            LOG_IF_FAIL(queryStringValue(requestId, 9, checksumTmp));
            checksum = std::make_optional(checksumTmp);
        }

        LOG_IF_FAIL(queryIntValue(requestId, 10, intResult));
        SyncFileStatus status = static_cast<SyncFileStatus>(intResult);

        LOG_IF_FAIL(queryIntValue(requestId, 11, intResult));
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
        dbNode.setChecksum(checksum);
        dbNode.setStatus(status);
        dbNode.setSyncing(syncing);

        dbNodeList.push_back(dbNode);
    }

    LOG_IF_FAIL(queryResetAndClearBindings(requestId));

    return true;
}

bool SyncDb::deleteNodesWithNullParentNodeId() {
    const std::scoped_lock lock(_mutex);
    invalidateCache();

    int errId;
    std::string error;

    LOG_IF_FAIL(queryResetAndClearBindings(DELETE_NODES_WITH_NULL_PARENTNODEID_REQUEST_ID));
    if (!queryExec(DELETE_NODES_WITH_NULL_PARENTNODEID_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << DELETE_NODES_WITH_NULL_PARENTNODEID_REQUEST_ID);
        return false;
    }

    return true;
}

bool SyncDb::pushChildDbIds(DbNodeId parentNodeDbId, std::unordered_set<DbNodeId> &ids) {
    const std::scoped_lock lock(_mutex);
    std::queue<DbNodeId> dbNodeIdQueue;
    dbNodeIdQueue.push(parentNodeDbId);

    while (!dbNodeIdQueue.empty()) {
        DbNodeId dbNodeId = dbNodeIdQueue.front();
        dbNodeIdQueue.pop();
        LOG_IF_FAIL(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID));
        LOG_IF_FAIL(queryBindValue(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, 1, dbNodeId));
        for (;;) {
            bool found;
            if (!queryNext(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, found)) {
                LOG_WARN(_logger, "Error getting query result: " << SELECT_NODE_BY_PARENTNODEID_REQUEST_ID
                                                                 << " - parentNodeId=" << std::to_string(dbNodeId));
                return false;
            }
            if (!found) {
                break;
            }
            bool nodeIdIsNull;
            LOG_IF_FAIL(queryIsNullValue(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, 0, nodeIdIsNull));
            if (!nodeIdIsNull) {
                // The node exists in the snapshot
                DbNodeId dbChildNodeId;
                LOG_IF_FAIL(queryInt64Value(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, 0, dbChildNodeId));
                ids.insert(dbChildNodeId);

                int type;
                LOG_IF_FAIL(queryIntValue(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, 5, type));
                if (static_cast<NodeType>(type) == NodeType::Directory) {
                    dbNodeIdQueue.push(dbChildNodeId);
                }
            }
        }
        LOG_IF_FAIL(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID));
    }

    return true;
}

bool SyncDb::pushChildDbIds(DbNodeId parentNodeDbId, std::unordered_set<NodeIds, NodeIds::HashFunction> &ids) {
    const std::scoped_lock lock(_mutex);
    std::queue<DbNodeId> dbNodeIdQueue;
    dbNodeIdQueue.push(parentNodeDbId);

    while (!dbNodeIdQueue.empty()) {
        DbNodeId dbNodeId = dbNodeIdQueue.front();
        dbNodeIdQueue.pop();

        LOG_IF_FAIL(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID));
        LOG_IF_FAIL(queryBindValue(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, 1, dbNodeId));
        for (;;) {
            bool found = false;
            if (!queryNext(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, found)) {
                LOG_WARN(_logger, "Error getting query result: " << SELECT_NODE_BY_PARENTNODEID_REQUEST_ID
                                                                 << " - parentNodeId=" << std::to_string(dbNodeId));
                return false;
            }
            if (!found) {
                break;
            }
            bool nodeIdIsNull = false;
            LOG_IF_FAIL(queryIsNullValue(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, 0, nodeIdIsNull));
            if (!nodeIdIsNull) {
                // The node exists in the snapshot
                NodeIds childNodeIds;
                LOG_IF_FAIL(queryInt64Value(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, 0, childNodeIds.dbNodeId));
                LOG_IF_FAIL(queryStringValue(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, 3, childNodeIds.localNodeId));
                LOG_IF_FAIL(queryStringValue(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, 4, childNodeIds.remoteNodeId));

                (void) ids.insert(childNodeIds);

                int type = 0;
                LOG_IF_FAIL(queryIntValue(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID, 5, type));
                if (static_cast<NodeType>(type) == NodeType::Directory) {
                    dbNodeIdQueue.push(childNodeIds.dbNodeId);
                }
            }
        }
        LOG_IF_FAIL(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID));
    }

    return true;
}

bool SyncDb::dbNodes(std::unordered_set<DbNode, DbNode::HashFunction> &dbNodes, SyncDbRevision &revision, bool &found) {
    const std::scoped_lock lock(_mutex);
    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ALL_NODES_REQUEST_ID));
    bool atLeastOneFound = false;
    for (;;) {
        if (!queryNext(SELECT_ALL_NODES_REQUEST_ID, atLeastOneFound)) {
            LOG_WARN(_logger, "Error getting query result: " << SELECT_ALL_NODES_REQUEST_ID);
            return false;
        }
        if (!atLeastOneFound) {
            break;
        }
        found = atLeastOneFound || found;
        DbNodeId dbNodeId = 0;
        LOG_IF_FAIL(queryInt64Value(SELECT_ALL_NODES_REQUEST_ID, 0, dbNodeId));

        bool isNull = false;
        std::optional<DbNodeId> parentNodeId;
        LOG_IF_FAIL(queryIsNullValue(SELECT_ALL_NODES_REQUEST_ID, 1, isNull));
        if (isNull) {
            parentNodeId = std::nullopt;
        } else {
            DbNodeId dbParentNodeId = 0;
            LOG_IF_FAIL(queryInt64Value(SELECT_ALL_NODES_REQUEST_ID, 1, dbParentNodeId));
            parentNodeId = std::make_optional(dbParentNodeId);
        }

        SyncName nameLocal;
        LOG_IF_FAIL(querySyncNameValue(SELECT_ALL_NODES_REQUEST_ID, 2, nameLocal));
        SyncName nameDrive;
        LOG_IF_FAIL(querySyncNameValue(SELECT_ALL_NODES_REQUEST_ID, 3, nameDrive));

        std::optional<NodeId> nodeIdLocal;
        LOG_IF_FAIL(queryIsNullValue(SELECT_ALL_NODES_REQUEST_ID, 4, isNull));
        if (isNull) {
            nodeIdLocal = std::nullopt;
        } else {
            NodeId nodeIdLocalTmp;
            LOG_IF_FAIL(queryStringValue(SELECT_ALL_NODES_REQUEST_ID, 4, nodeIdLocalTmp));
            nodeIdLocal = std::make_optional(nodeIdLocalTmp);
        }

        std::optional<NodeId> nodeIdDrive;
        LOG_IF_FAIL(queryIsNullValue(SELECT_ALL_NODES_REQUEST_ID, 5, isNull));
        if (isNull) {
            nodeIdDrive = std::nullopt;
        } else {
            NodeId nodeIdDriveTmp;
            LOG_IF_FAIL(queryStringValue(SELECT_ALL_NODES_REQUEST_ID, 5, nodeIdDriveTmp));
            nodeIdDrive = std::make_optional(nodeIdDriveTmp);
        }

        std::optional<SyncTime> created;
        LOG_IF_FAIL(queryIsNullValue(SELECT_ALL_NODES_REQUEST_ID, 6, isNull));
        if (isNull) {
            created = std::nullopt;
        } else {
            SyncTime timeTmp = 0;
            LOG_IF_FAIL(queryInt64Value(SELECT_ALL_NODES_REQUEST_ID, 6, timeTmp));
            created = std::make_optional(timeTmp);
        }

        std::optional<SyncTime> lastModifiedLocal;
        LOG_IF_FAIL(queryIsNullValue(SELECT_ALL_NODES_REQUEST_ID, 7, isNull));
        if (isNull) {
            lastModifiedLocal = std::nullopt;
        } else {
            SyncTime timeTmp = 0;
            LOG_IF_FAIL(queryInt64Value(SELECT_ALL_NODES_REQUEST_ID, 7, timeTmp));
            lastModifiedLocal = std::make_optional(timeTmp);
        }

        std::optional<SyncTime> lastModifiedDrive;
        LOG_IF_FAIL(queryIsNullValue(SELECT_ALL_NODES_REQUEST_ID, 8, isNull));
        if (isNull) {
            lastModifiedDrive = std::nullopt;
        } else {
            SyncTime timeTmp = 0;
            LOG_IF_FAIL(queryInt64Value(SELECT_ALL_NODES_REQUEST_ID, 8, timeTmp));
            lastModifiedDrive = std::make_optional(timeTmp);
        }

        int intResult = 0;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_NODES_REQUEST_ID, 9, intResult));
        NodeType type = static_cast<NodeType>(intResult);

        int64_t size = 0;
        LOG_IF_FAIL(queryInt64Value(SELECT_ALL_NODES_REQUEST_ID, 10, size));

        std::optional<std::string> checksum;
        LOG_IF_FAIL(queryIsNullValue(SELECT_ALL_NODES_REQUEST_ID, 11, isNull));
        if (isNull) {
            checksum = std::nullopt;
        } else {
            std::string checksumTmp;
            LOG_IF_FAIL(queryStringValue(SELECT_ALL_NODES_REQUEST_ID, 11, checksumTmp));
            checksum = std::make_optional(checksumTmp);
        }

        LOG_IF_FAIL(queryIntValue(SELECT_ALL_NODES_REQUEST_ID, 12, intResult));
        SyncFileStatus status = static_cast<SyncFileStatus>(intResult);

        LOG_IF_FAIL(queryIntValue(SELECT_ALL_NODES_REQUEST_ID, 13, intResult));
        bool syncing = static_cast<bool>(intResult);
        dbNodes.emplace(dbNodeId, parentNodeId, nameLocal, nameDrive, nodeIdLocal, nodeIdDrive, created, lastModifiedLocal,
                        lastModifiedDrive, type, size, checksum, status, syncing);
    }
    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_NODE_BY_PARENTNODEID_REQUEST_ID));
    revision = _revision;

    return true;
}

bool SyncDb::selectNamesWithDistinctEncodings(NamedNodeMap &namedNodeMap) {
    static const char *requestId = "select_node_with_names_and_ids";
    static const char *query = "SELECT nodeId, nameLocal, nameDrive, nodeIdLocal FROM node;";

    if (!createAndPrepareRequest(requestId, query)) return false;

    const std::scoped_lock lock(_mutex);

    LOG_IF_FAIL(queryResetAndClearBindings(requestId));
    bool found = false;
    for (;;) {
        if (!queryNext(requestId, found)) {
            LOG_WARN(_logger, "Error getting query result: " << requestId);
            return false;
        }

        if (!found) break;

        DbNodeId dbNodeId;
        LOG_IF_FAIL(queryInt64Value(requestId, 0, dbNodeId));

        SyncName nameLocal;
        LOG_IF_FAIL(querySyncNameValue(requestId, 1, nameLocal));

        SyncName nfcNormalizedName;
        if (!Utility::normalizedSyncName(nameLocal, nfcNormalizedName, UnicodeNormalization::NFC)) {
            LOGW_DEBUG(_logger, L"Error in Utility::normalizedSyncName: " << Utility::formatSyncName(nameLocal));
            return false;
        }

        SyncName nfdNormalizedName;
        if (!Utility::normalizedSyncName(nameLocal, nfdNormalizedName, UnicodeNormalization::NFD)) {
            LOGW_DEBUG(_logger, L"Error in Utility::normalizedSyncName: " << Utility::formatSyncName(nameLocal));
            return false;
        }

        const bool sameLocalEncodings = (nfcNormalizedName == nfdNormalizedName);

        if (sameLocalEncodings) continue;

        SyncName nameDrive;
        LOG_IF_FAIL(querySyncNameValue(requestId, 2, nameDrive));

        NodeId nodeIdLocal;
        LOG_IF_FAIL(queryStringValue(requestId, 3, nodeIdLocal));

        const IntNodeId intNodeId = std::stoull(nodeIdLocal);
        namedNodeMap.try_emplace(intNodeId, NamedNode{dbNodeId, nameLocal});
    }

    queryFree(requestId);

    return true;
}

bool SyncDb::updateNamesWithDistinctEncodings(const SyncNameMap &localNames) {
    static const char *requestId = UPDATE_NODE_NAME_LOCAL_REQUEST_ID;

    if (!createAndPrepareRequest(requestId, UPDATE_NODE_NAME_LOCAL_REQUEST)) return false;

    for (const auto &[dbNodeId, fileName]: localNames) {
        bool found = false;
        updateNodeLocalName(dbNodeId, fileName, found);
        if (!found) {
            LOGW_WARN(_logger,
                      L"Node with DB id='" << dbNodeId << L"' and " << Utility::formatSyncName(fileName) << L" not found.");
            queryFree(requestId);

            return false;
        }
    }

    queryFree(requestId);

    return true;
}

bool SyncDb::normalizeRemoteNames() {
    const std::scoped_lock lock(_mutex);
    invalidateCache();
    static const char *requestId = "normalize_remote_names";
    static const char *query =
            "UPDATE node "
            "SET nameDrive = normalizeSyncName(nameDrive);";

    if (_sqliteDb->createNormalizeSyncNameFunc() != SQLITE_OK) {
        return false;
    }

    if (!createAndPrepareRequest(requestId, query)) return false;

    int errId = 0;
    std::string error;

    if (!queryExec(requestId, errId, error)) {
        queryFree(requestId);
        return sqlFail(requestId, error);
    }
    queryFree(requestId);

    return true;
}

bool SyncDb::reinstateEncodingOfLocalNames(const std::string &dbFromVersionNumber) {
    if (!CommonUtility::isVersionLower(dbFromVersionNumber, "3.6.7")) return true;

    LOG_DEBUG(_logger, "Upgrade < 3.6.7 Sync DB");

    Sync sync;
    bool found = false;
    ParmsDb::instance()->selectSync(_dbPath, sync, found);
    if (!found) {
        LOGW_WARN(_logger, L"Sync DB with " << Utility::formatSyncPath(_dbPath) << L" not found.");
        return false;
    }

    const SyncPath &localDrivePath = sync.localPath();

    bool exists = false;
    IoError existenceCheckError = IoError::Success;
    if (!IoHelper::checkIfPathExists(localDrivePath, exists, existenceCheckError)) {
        LOGW_WARN(_logger,
                  L"Error in IoHelper::checkIfPathExists" << Utility::formatIoError(localDrivePath, existenceCheckError));
        return false;
    }
    if (!exists) {
        LOGW_INFO(_logger, L"The synchronisation folder " << Utility::formatSyncPath(localDrivePath)
                                                          << L" does not exist anymore. No Sync DB upgrade to do.");
        return true;
    }

    if (!normalizeRemoteNames()) return false;

    LOG_INFO(_logger, "Remote names in SyncDb normalized successfully.");

    NamedNodeMap namedNodeMap;
    if (!selectNamesWithDistinctEncodings(namedNodeMap)) return false;

    IoHelper::DirectoryIterator dir;
    IoError ioError = IoError::Success;
    IoHelper::getDirectoryIterator(localDrivePath, true, ioError, dir);
    if (ioError != IoError::Success) {
        LOGW_WARN(_logger, L"Error in DirectoryIterator: " << Utility::formatIoError(localDrivePath, ioError));
        return (ioError == IoError::NoSuchFileOrDirectory) || (ioError == IoError::AccessDenied);
    }

    SyncNameMap localNames;
    DirectoryEntry entry;
    bool endOfDirectory = false;

    while (dir.next(entry, endOfDirectory, ioError) && !endOfDirectory) {
        NodeId nodeId;
        if (!IoHelper::getNodeId(entry.path(), nodeId)) {
            LOGW_WARN(_logger, L"Could not retrieve the node id of item with " << Utility::formatSyncPath(entry.path()));
            continue;
        }

        const IntNodeId intNodeId = std::stoull(nodeId);
        if (!namedNodeMap.contains(intNodeId)) continue;

        SyncName actualLocalName(entry.path().filename());
        if (actualLocalName != namedNodeMap[intNodeId].localName) {
            localNames.try_emplace(namedNodeMap[intNodeId].dbNodeId, std::move(actualLocalName));
        }
    }

    LOG_INFO(_logger, "Node ids retrieved successfully from disk.");

    if (!updateNamesWithDistinctEncodings(localNames)) return false;

    return true;
}

bool SyncDb::tryToFixDbNodeIdsAfterSyncDirChange(const SyncPath &syncDirPath) {
    SyncDbReadOnlyCache &dbCache = cache();
    if (!dbCache.reloadIfNeeded()) {
        LOGW_WARN(_logger, L"Unable to reload SyncDb cache.");
        return false;
    }

    std::unordered_set<NodeIds, NodeIds::HashFunction> nodeIdsFromDb;
    bool found = false;
    if (!dbCache.ids(nodeIdsFromDb, found)) {
        LOGW_WARN(_logger, L"Unable to get node IDs from SyncDb cache.");
        dbCache.clear();
        return false;
    }
    if (!found) {
        LOGW_WARN(_logger, L"No node IDs found in SyncDb cache.");
        dbCache.clear();
        return false;
    }

    std::list<std::pair<DbNodeId, NodeId>> updatedNodeIds;
    const auto rootDbNodeId = dbCache.rootNode().nodeId();
    for (const auto &nodeIds: nodeIdsFromDb) {
        const DbNodeId dbNodeId = nodeIds.dbNodeId;
        if (dbNodeId == rootDbNodeId) continue; // Skip root node
        SyncPath relativelocalPath;
        SyncPath remotePath;
        if (!dbCache.path(dbNodeId, relativelocalPath, remotePath, found)) {
            LOGW_WARN(_logger, L"Unable to get paths for DbNode ID " << dbNodeId);
            dbCache.clear();
            return false;
        }
        if (!found) {
            LOGW_WARN(_logger, L"DbNode with ID " << dbNodeId << L" not found in cache.");
            dbCache.clear();
            return false;
        }
        SyncPath absoluteLocalPath = syncDirPath / relativelocalPath;
        NodeId newLocalNodeId;

        if (!IoHelper::getNodeId(absoluteLocalPath, newLocalNodeId)) {
            LOGW_WARN(_logger, L"Unable to get new local node ID for "
                                       << Utility::formatSyncPath(absoluteLocalPath)
                                       << L". It might have been deleted or moved, the syncDb cannot be fixed.");
            dbCache.clear();
            return false;
        }
        (void) updatedNodeIds.emplace_back(dbNodeId, newLocalNodeId);
    }

    for (const auto &[dbNodeId, newLocalNodeId]: updatedNodeIds) {
        DbNode dbNode;
        if (!dbCache.node(dbNodeId, dbNode, found)) {
            LOGW_WARN(_logger, L"Unable to get DbNode with ID " << dbNodeId);
            dbCache.clear();
            return false;
        }
        if (!found) {
            LOGW_WARN(_logger, L"DbNode with ID " << dbNodeId << L" not found in cache.");
            dbCache.clear();
            return false;
        }
        dbNode.setNodeIdLocal(newLocalNodeId);

        if (!updateNode(dbNode, found)) {
            LOGW_WARN(_logger, L"Unable to update local node ID for DbNode ID " << dbNodeId);
            dbCache.clear();
            return false;
        }
        if (!found) {
            LOGW_WARN(_logger, L"DbNode with ID " << dbNodeId << L" not found in SyncDb.");
            dbCache.clear();
            return false;
        }

        LOG_INFO(_logger, "Updated DbNode ID " << dbNodeId << " with new local node ID " << newLocalNodeId);
    }
    dbCache.clear();
    return true;
}

} // namespace KDC
