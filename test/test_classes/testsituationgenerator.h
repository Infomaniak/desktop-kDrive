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

#include "test_utility/localtemporarydirectory.h"

#include "db/dbnode.h"
#include "update_detection/update_detector/node.h"
#include "update_detection/file_system_observer/snapshot/livesnapshot.h"
#include "utility/types.h"

#include <Poco/JSON/Object.h>

namespace KDC {
class SyncDb;
class Snapshot;
class UpdateTree;
class Node;
class SyncPal;

/**
 * @brief This class aims to provide a simple and efficient way to generate a data structure (Db and update trees) for testing.
 * The structure is provided as a JSON file. Only the ID in lowercase must be provided in the JSON file. The name will be, by
 * default, the same as the ID but in uppercase. For example, the following JSON :
 * {
 *    "a":
 *    {
 *        "aa":
 *        {
 *            "aaa": 1
 *        }
 *    },
 *    "b": {},
 *    "c": {}
 * }
 * will generate this tree:
 * .
 * ├── A
 * │   └── AA
 * │       └── AAA
 * ├── B
 * └── C
 *
 * where "AAA" is a file, and the other nodes are directories.
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

        [[nodiscard]] std::shared_ptr<Node> getNode(ReplicaSide side, const NodeId &id) const;
        bool getDbNode(const NodeId &id, DbNode &dbNode) const;

        // Utility functions used to simulate events in the update tree
        std::shared_ptr<Node> createNode(const ReplicaSide side, const NodeType itemType, const NodeId &id,
                                         const NodeId &parentId, const bool setChangeEvent = true) const;
        std::shared_ptr<Node> createNode(const ReplicaSide side, const NodeType itemType, const NodeId &id,
                                         const std::shared_ptr<Node> &parentNode, const bool setChangeEvent = true) const {
            return createNode(side, itemType, id, parentNode ? *parentNode->id() : "", setChangeEvent);
        }
        std::shared_ptr<Node> moveNode(ReplicaSide side, const NodeId &id, const NodeId &newParentId,
                                       const SyncName &newName = {}) const;
        std::shared_ptr<Node> renameNode(ReplicaSide side, const NodeId &id, const SyncName &newName) const;
        std::shared_ptr<Node> editNode(ReplicaSide side, const NodeId &id, SyncTime timeInput = 0) const;
        std::shared_ptr<Node> deleteNode(ReplicaSide side, const NodeId &id) const;

    private:
        [[nodiscard]] NodeId generateId(ReplicaSide side, const NodeId &id) const;

        void addItem(Poco::JSON::Object::Ptr obj, const std::string &parentId = {});

        void insertInAllSnapshot(NodeType itemType, const NodeId &id, const NodeId &parentId) const;
        [[nodiscard]] DbNodeId insertInDb(NodeType itemType, const NodeId &id, const NodeId &parentId) const;
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
        void insertInAllUpdateTrees(NodeType itemType, const NodeId &id, const NodeId &parentId, DbNodeId dbNodeId) const;

        LiveSnapshot &liveSnapshot(const ReplicaSide side) const {
            return side == ReplicaSide::Local ? _localLiveSnapshot->get() : _remoteLiveSnapshot->get();
        }

        std::shared_ptr<UpdateTree> updateTree(const ReplicaSide side) const {
            return side == ReplicaSide::Local ? _localUpdateTree : _remoteUpdateTree;
        }

        LocalTemporaryDirectory _temporaryDirectory = LocalTemporaryDirectory("testSituationGenerator");

        std::shared_ptr<SyncDb> _syncDb;
        std::optional<std::reference_wrapper<LiveSnapshot>> _localLiveSnapshot;
        std::optional<std::reference_wrapper<LiveSnapshot>> _remoteLiveSnapshot;

        std::shared_ptr<UpdateTree> _localUpdateTree;
        std::shared_ptr<UpdateTree> _remoteUpdateTree;
};

} // namespace KDC
