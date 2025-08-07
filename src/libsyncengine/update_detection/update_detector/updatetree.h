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

#include "node.h"
#include "syncpal/sharedobject.h"
#include "db/dbnode.h"
#include "libcommon/utility/types.h"

#include <unordered_map>


namespace KDC {

class UpdateTree : public SharedObject {
    public:
        UpdateTree(const ReplicaSide side, const DbNode &dbNode);
        ~UpdateTree();

        void insertNode(std::shared_ptr<Node> node);
        [[nodiscard]] bool deleteNode(std::shared_ptr<Node> node, int depth = 1);
        [[nodiscard]] bool deleteNode(const NodeId &id);
        [[nodiscard]] inline const ReplicaSide &side() const { return _side; }
        inline std::shared_ptr<Node> rootNode() { return _rootNode; }
        inline std::unordered_map<NodeId, std::shared_ptr<Node>> &nodes() { return _nodes; }
        inline std::unordered_map<NodeId, NodeId> &previousIdSet() { return _previousIdSet; }
        std::shared_ptr<Node> getNodeByPath(const SyncPath &path);
        std::shared_ptr<Node> getNodeByPathNormalized(const SyncPath &path);
        std::shared_ptr<Node> getNodeById(const NodeId &nodeId);
        bool exists(const NodeId &id);

        /** Checks if ancestorItem is an ancestor of item.
         * @return true indicates that ancestorItem is an ancestor of item
         */
        bool isAncestor(const NodeId &nodeId, const NodeId &ancestorNodeId) const;

        void markAllNodesUnprocessed();
        void clear();
        void init();

        [[nodiscard]] bool updateNodeId(std::shared_ptr<Node> node, const NodeId &newId);
        inline void setRootFolderId(const NodeId &nodeId) { _rootNode->setId(std::make_optional<NodeId>(nodeId)); }

        /**
         * Draw the update tree in the log file for debugging purpose
         */
        void drawUpdateTree(uint16_t step = 0);

    private:
        void drawUpdateTreeRow(std::shared_ptr<Node> node, SyncName &treeStr, uint64_t depth = 0);

        std::unordered_map<NodeId, std::shared_ptr<Node>> _nodes;
        std::shared_ptr<Node> _rootNode;
        ReplicaSide _side;

        // key : previousID, value : newID
        std::unordered_map<NodeId, NodeId> _previousIdSet;

        friend class TestUpdateTree;
        friend class TestUpdateTreeWorker;
};

} // namespace KDC
