/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "test_utility/localtemporarydirectory.h"

#include "db/dbnode.h"
#include "update_detection/update_detector/node.h"
#include "update_detection/file_system_observer/snapshot/livesnapshot.h"
#include "utility/types.h"

#include <Poco/JSON/Array.h>
#include <Poco/JSON/Object.h>

#include <iostream>

namespace KDC {
class SyncDb;
class Snapshot;
class UpdateTree;
class Node;
class SyncPal;

/**
 * @brief This class aims to provide a simple and efficient way to generate a data structure (Db and update trees) for testing.
 * Two JSON formats are supported:
 *
 * Legacy format: the key is the node ID (lowercase), object values are directories, non-object values are files,
 * and names are automatically set to the uppercase version of the ID:
 * {
 *    "a": { "aa": { "aaa": 1 } },
 *    "b": {},
 *    "c": 1
 * }
 *
 * Extended format: an array under the "content" key allows explicit control over name, type, size and timestamps.
 * The node ID is derived from toLower(name):
 * {
 *    "content": [
 *        { "type": "Directory", "name": "A", "createdAt": 20260601000000, "lastModifiedAt": 20260601000000, "content": [
 *            { "type": "Directory", "name": "AA", "content": [
 *                { "type": "File", "name": "AAA" }
 *            ]}
 *        ]},
 *        { "type": "Directory", "name": "B" },
 *        { "type": "File",      "name": "C", "size": 1234 }
 *    ]
 * }
 *
 * Both examples generate:
 * .
 * ├── A
 * │   └── AA
 * │       └── AAA
 * ├── B
 * └── C
 *
 * where leaf values / "File" types are files, and the other nodes are directories.
 */
class TestSituationGenerator {
    public:
        TestSituationGenerator();
        explicit TestSituationGenerator(std::shared_ptr<SyncPal> syncpal);

        void setSyncpal(std::shared_ptr<SyncPal> syncpal);
        void setSyncDb(const std::shared_ptr<SyncDb> syncDb) { _syncDb = syncDb; }
        void setLocalSnapshot(LiveSnapshot &localSnapshot) { _localLiveSnapshot = localSnapshot; }
        void setRemoteSnapshot(LiveSnapshot &remoteSnapshot) { _remoteLiveSnapshot = remoteSnapshot; }
        void setLocalUpdateTree(const std::shared_ptr<UpdateTree> localUpdateTree) { _localUpdateTree = localUpdateTree; }
        void setRemoteUpdateTree(const std::shared_ptr<UpdateTree> remoteUpdateTree) { _remoteUpdateTree = remoteUpdateTree; }

        void generateInitialSituation(const std::string &jsonInputStr);
        void addItem(NodeType itemType, const std::string &id, const std::string &parentId) const;
        [[nodiscard]] size_t size() const;

        void printTree(ReplicaSide side, std::ostream &out = std::cout) const;

        [[nodiscard]] std::shared_ptr<Node> getNode(ReplicaSide side, const NodeId &id) const;
        bool getDbNode(const NodeId &id, DbNode &dbNode) const;

        // Utility functions used to simulate events in the update tree
        std::shared_ptr<Node> createNode(const ReplicaSide side, const NodeType itemType, const NodeId &id,
                                         const NodeId &parentId, const bool setChangeEvent = true) const;
        std::shared_ptr<Node> createNode(const ReplicaSide side, const NodeType itemType, const NodeId &id,
                                         const std::shared_ptr<Node> parentNode, const bool setChangeEvent = true) const {
            return createNode(side, itemType, id, parentNode ? *parentNode->id() : "", setChangeEvent);
        }
        std::shared_ptr<Node> moveNode(ReplicaSide side, const NodeId &id, const NodeId &newParentId,
                                       const SyncName &newName = {}) const;
        std::shared_ptr<Node> renameNode(ReplicaSide side, const NodeId &id, const SyncName &newName) const;
        std::shared_ptr<Node> editNode(ReplicaSide side, const NodeId &id, SyncTime timeInput = 0) const;
        std::shared_ptr<Node> deleteNode(ReplicaSide side, const NodeId &id) const;

    private:
        struct ItemDesc {
            NodeType type = NodeType::File;
            NodeId id;           // lowercase, used for ID generation
            SyncName name;       // display name stored in all data structures
            SyncTime createdAt = 0;
            SyncTime lastModifiedAt = 0;
            int64_t size = 0;
        };

        [[nodiscard]] NodeId generateId(ReplicaSide side, const NodeId &id) const;

        void addItem(Poco::JSON::Object::Ptr obj, const std::string &parentId = {});
        void addItem(Poco::JSON::Array::Ptr arr, const std::string &parentId);
        void addItem(const ItemDesc &desc, const std::string &parentId) const;

        void insertInAllSnapshot(const ItemDesc &desc, const NodeId &parentId) const;
        [[nodiscard]] DbNodeId insertInDb(const ItemDesc &desc, const NodeId &parentId) const;
        /**
         * @brief Insert a new node in the update tree.
         * @param side Replica side for the update tree (Local or Remote).
         * @param itemType Directory or File.
         * @param id File ID in lowercase.
         * @param parentId Parent file ID in lowercase. If empty, the parent is the root node.
         * @param dbNodeId DB ID of the node to insert.
         * @return A pointer to the generated node.
         */
        [[nodiscard]] std::shared_ptr<Node> insertInUpdateTree(ReplicaSide side, NodeType itemType, const NodeId &id,
                                                               const NodeId &parentId, std::optional<DbNodeId> dbNodeId) const;
        [[nodiscard]] std::shared_ptr<Node> insertInUpdateTree(ReplicaSide side, const ItemDesc &desc,
                                                               const NodeId &parentId, std::optional<DbNodeId> dbNodeId) const;
        void insertInAllUpdateTrees(const ItemDesc &desc, const NodeId &parentId, DbNodeId dbNodeId) const;

        LiveSnapshot &liveSnapshot(const ReplicaSide side) const {
            return side == ReplicaSide::Local ? _localLiveSnapshot->get() : _remoteLiveSnapshot->get();
        }

        std::shared_ptr<UpdateTree> updateTree(const ReplicaSide side) const {
            return side == ReplicaSide::Local ? _localUpdateTree : _remoteUpdateTree;
        }

        const LocalTemporaryDirectory _temporaryDirectory = LocalTemporaryDirectory("TestSituationGenerator");

        std::shared_ptr<SyncDb> _syncDb;
        std::optional<std::reference_wrapper<LiveSnapshot>> _localLiveSnapshot;
        std::optional<std::reference_wrapper<LiveSnapshot>> _remoteLiveSnapshot;

        std::shared_ptr<UpdateTree> _localUpdateTree;
        std::shared_ptr<UpdateTree> _remoteUpdateTree;
};

} // namespace KDC
