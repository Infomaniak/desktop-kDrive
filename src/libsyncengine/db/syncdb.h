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

#include "dbnode.h"
#include "syncdbreadonlycache.h"
#include "libcommon/utility/types.h"
#include "libcommonserver/db/db.h"
#include "db/uploadsessiontoken.h"

#include <log4cplus/loggingmacros.h>

#include <unordered_set>

namespace KDC {

class SyncDb : public Db {
    public:
        SyncDb(const std::string &dbPath, const std::string &version, const std::string &targetNodeId = std::string());

        std::string dbType() const override { return "Sync"; }

        bool create(bool &retry) override;
        bool prepare() override;
        bool upgrade(const std::string &fromVersion, const std::string &toVersion) override;

        bool initData();

        bool insertNode(const DbNode &node, DbNodeId &dbNodeId,
                        bool &constraintError); // The local and remote names of an inserted node are normalized.
        bool insertNode(const DbNode &node); // The local and remote names of an inserted node are normalized.
        bool updateNode(const DbNode &node, bool &found);
        bool updateNodeStatus(DbNodeId nodeId, SyncFileStatus status, bool &found);
        bool updateNodesSyncing(bool syncing);
        bool updateNodeSyncing(DbNodeId nodeId, bool syncing, bool &found);
        bool deleteNode(DbNodeId nodeId, bool &found);
        bool selectStatus(DbNodeId nodeId, SyncFileStatus &status, bool &found);
        bool selectSyncing(DbNodeId nodeId, bool &syncing, bool &found);
        bool clearNodes();

        // Getters with replica IDs
        bool id(ReplicaSide side, const SyncPath &path, std::optional<NodeId> &nodeId, bool &found);
        bool id(ReplicaSide side, DbNodeId dbNodeId, NodeId &nodeId, bool &found);
        bool type(ReplicaSide side, const NodeId &nodeId, NodeType &type, bool &found);
        bool size(ReplicaSide side, const NodeId &nodeId, int64_t &size, bool &found);
        bool created(ReplicaSide side, const NodeId &nodeId, std::optional<SyncTime> &time, bool &found);
        bool lastModified(ReplicaSide side, const NodeId &nodeId, std::optional<SyncTime> &time, bool &found);
        bool parent(ReplicaSide side, const NodeId &nodeId, NodeId &parentNodeid, bool &found);
        bool path(ReplicaSide side, const NodeId &nodeId, SyncPath &path, bool &found);
        bool name(ReplicaSide side, const NodeId &nodeId, SyncName &name, bool &found);
        bool checksum(ReplicaSide side, const NodeId &nodeId, std::optional<std::string> &cs, bool &found);
        bool ids(ReplicaSide side, std::vector<NodeId> &ids, bool &found);
        bool ids(ReplicaSide side, NodeSet &ids, bool &found);
        bool ancestor(ReplicaSide side, const NodeId &nodeId1, const NodeId &nodeId2, bool &ret, bool &found);
        bool dbId(ReplicaSide side, const NodeId &nodeId, DbNodeId &dbNodeId, bool &found);
        bool dbId(ReplicaSide side, const SyncPath &path, DbNodeId &dbNodeId, bool &found);
        bool correspondingNodeId(ReplicaSide side, const NodeId &nodeIdIn, NodeId &nodeIdOut, bool &found);
        bool node(ReplicaSide side, const NodeId &nodeId, DbNode &dbNode, bool &found);

        // Getters with db IDs
        bool dbIds(std::unordered_set<DbNodeId> &ids, bool &found);
        bool ids(std::unordered_set<NodeIds, NodeIds::HashFunction> &ids, bool &found);

        bool path(DbNodeId dbNodeId, SyncPath &localPath, SyncPath &remotePath, bool &found);
        bool node(DbNodeId dbNodeId, DbNode &dbNode, bool &found);
        bool pushChildDbIds(DbNodeId parentNodeDbId, std::unordered_set<DbNodeId> &ids);
        bool pushChildDbIds(DbNodeId parentNodeDbId, std::unordered_set<NodeIds, NodeIds::HashFunction> &ids);
        bool dbNodes(std::unordered_set<DbNode, DbNode::HashFunction> &nodes, SyncDbRevision &revision, bool &found);
        bool status(ReplicaSide side, const SyncPath &path, SyncFileStatus &status, bool &found);
        bool status(ReplicaSide side, const NodeId &nodeId, SyncFileStatus &status, bool &found);
        bool setStatus(ReplicaSide side, const SyncPath &path, SyncFileStatus status, bool &found);
        bool setStatus(ReplicaSide side, const NodeId &nodeId, SyncFileStatus status, bool &found);
        bool syncing(ReplicaSide side, const SyncPath &path, bool &syncing, bool &found);
        bool setSyncing(ReplicaSide side, const SyncPath &path, bool syncing, bool &found);

        bool deleteSyncNode(const NodeId &nodeId, bool &found);
        bool updateAllSyncNodes(SyncNodeType type, const NodeSet &nodeIdSet);
        bool selectAllSyncNodes(SyncNodeType type, NodeSet &nodeIdSet);

        bool selectAllRenamedNodes(std::vector<DbNode> &dbNodeList, bool onlyColon);
        bool deleteNodesWithNullParentNodeId();

        bool insertUploadSessionToken(const UploadSessionToken &uploadSessionToken, int64_t &uploadSessionTokenDbId);
        bool deleteUploadSessionTokenByDbId(int64_t dbId, bool &found);
        bool selectAllUploadSessionTokens(std::vector<UploadSessionToken> &uploadSessionTokenList);
        bool deleteAllUploadSessionToken();

        static DbNode driveRootNode() { return _driveRootNode; }
        DbNode rootNode() { return _rootNode; }

        bool setTargetNodeId(const std::string &targetNodeId, bool &found);

        SyncDbRevision revision() const;
        SyncDbReadOnlyCache &cache() { return _cache; }

        // Fix the local node IDs after a sync directory nodeId change.
        // This can happen when the sync directory is moved between two disks, after a migration from an other device (Apple
        // migration assistant, etc.).
        // This is a best effort, if at least one of the items inside the sync directory has been deleted or moved, the process
        // will fail and the user should be prompted to create a new sync directory.
        bool tryToFixDbNodeIdsAfterSyncDirChange(const SyncPath &syncDirPath);

    protected:
        virtual bool updateNames(const char *requestId, const SyncName &localName, const SyncName &remoteName);

    private:
        static DbNode _driveRootNode;
        DbNode _rootNode = _driveRootNode;
        SyncDbRevision _revision = 1;
        SyncDbReadOnlyCache _cache;
        void invalidateCache() { ++_revision; }
        bool pushChildIds(ReplicaSide side, DbNodeId parentNodeDbId, std::vector<NodeId> &ids);
        bool pushChildIds(ReplicaSide side, DbNodeId parentNodeDbId, NodeSet &ids);

        // Helpers
        bool checkNodeIds(const DbNode &node);

        // Fixes
        bool updateNodeLocalName(DbNodeId nodeId, const SyncName &localName, bool &found);
        struct NamedNode {
                DbNodeId dbNodeId{-1};
                SyncName localName;
        };
        using IntNodeId = unsigned long long;
        using NamedNodeMap = std::unordered_map<IntNodeId, NamedNode>;
        bool selectNamesWithDistinctEncodings(NamedNodeMap &namedNodeMap);
        using SyncNameMap = std::unordered_map<DbNodeId, SyncName>;
        bool updateNamesWithDistinctEncodings(const SyncNameMap &localNames);
        // Fix issue introduced in version 3.6.3: re-normalize all file and directory names of a DB node.
        bool normalizeRemoteNames();
        // Use the actual encoding of local file names in DB.
        bool reinstateEncodingOfLocalNames(const std::string &dbFromVersionNumber);

        friend class TestSyncDb;
};

} // namespace KDC
