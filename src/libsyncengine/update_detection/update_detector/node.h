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

#include "utility/types.h"
#include "libcommonserver/utility/utility.h"

#include <algorithm>
#include <vector>
#include <optional>
#include <unordered_map>

namespace KDC {

class Node {
    public:
        const static Node _nullNode;

        Node(const std::optional<DbNodeId> &idb, const ReplicaSide &side, const SyncName &name, NodeType type,
             const std::optional<NodeId> &id, std::optional<SyncTime> createdAt, std::optional<SyncTime> lastmodified,
             int64_t size);

        Node(const std::optional<DbNodeId> &idb, const ReplicaSide &side, const SyncName &name, NodeType type, int changeEvents,
             const std::optional<NodeId> &id, std::optional<SyncTime> createdAt, std::optional<SyncTime> lastmodified,
             int64_t size, std::shared_ptr<Node> parentNode, std::optional<SyncPath> moveOrigin = std::nullopt,
             std::optional<DbNodeId> moveOriginParentDbId = std::nullopt);

        /**
         * @brief Node
         *
         * Node constructor for temporary nodes only
         * A temporary ID is generated and flag _isTmp is set to true
         *
         * @param side
         * @param name
         * @param type
         * @param parentNode
         */
        Node(const ReplicaSide &side, const SyncName &name, NodeType type, std::shared_ptr<Node> parentNode);

        Node();

        bool operator==(const Node &n) const;
        inline bool operator!=(const Node &n) const { return !(n == *this); }

        inline std::optional<DbNodeId> idb() const { return _idb; }
        inline ReplicaSide side() const { return _side; }
        inline SyncName name() const { return _name; }
        inline NodeType type() const { return _type; }
        inline InconsistencyType inconsistencyType() const { return _inconsistencyType; }
        inline int changeEvents() const { return _changeEvents; }
        inline std::optional<SyncTime> createdAt() const { return _createdAt; }
        inline std::optional<SyncTime> lastmodified() const { return _lastModified; }
        inline int64_t size() { return _size; }
        inline std::optional<NodeId> id() const { return _id; }
        inline std::optional<NodeId> previousId() const { return _previousId; }
        inline NodeStatus status() const { return _status; }
        inline std::shared_ptr<Node> parentNode() const { return _parentNode; }
        inline const std::optional<SyncPath> &moveOrigin() const { return _moveOrigin; }
        inline const std::optional<DbNodeId> &moveOriginParentDbId() const { return _moveOriginParentDbId; }
        inline const std::vector<ConflictType> &conflictsAlreadyConsidered() const { return _conflictsAlreadyConsidered; }
        inline bool hasConflictAlreadyConsidered(const ConflictType conf) {
            return std::count(_conflictsAlreadyConsidered.cbegin(), _conflictsAlreadyConsidered.cend(), conf) > 0;
        }

        inline void setIdb(const std::optional<DbNodeId> &idb) { _idb = idb; }
        void setName(const SyncName &name);
        inline void setInconsistencyType(InconsistencyType newInconsistencyType) { _inconsistencyType = newInconsistencyType; }
        inline void addInconsistencyType(InconsistencyType newInconsistencyType) { _inconsistencyType |= newInconsistencyType; }
        inline void setCreatedAt(const std::optional<SyncTime> &createdAt) { _createdAt = createdAt; }
        inline void setLastModified(const std::optional<SyncTime> &lastmodified) { _lastModified = lastmodified; }
        inline void setSize(int64_t size) { _size = size; }
        inline void setId(const std::optional<NodeId> &nodeId) { _id = nodeId; }
        inline void setPreviousId(const std::optional<NodeId> &previousNodeId) { _previousId = previousNodeId; }
        inline void setParentNode(const std::shared_ptr<Node> &parentNode) { _parentNode = parentNode; }
        inline void setMoveOrigin(const std::optional<SyncPath> &moveOrigin) { _moveOrigin = moveOrigin; }
        inline void setMoveOriginParentDbId(const std::optional<DbNodeId> &moveOriginParentDbId) {
            _moveOriginParentDbId = moveOriginParentDbId;
        }
        inline void setStatus(const NodeStatus &status) { _status = status; }

        inline std::unordered_map<NodeId, std::shared_ptr<Node>> &children() { return _childrenById; }
        std::shared_ptr<Node> findChildren(const SyncName &name, const NodeId &nodeId = "");
        std::shared_ptr<Node> findChildrenById(const NodeId &nodeId);
        bool insertChildren(std::shared_ptr<Node> child);
        size_t deleteChildren(std::shared_ptr<Node> child);
        size_t deleteChildren(const NodeId &childId);
        std::shared_ptr<Node> getChildExcept(SyncName name, OperationType except);

        inline void setChangeEvents(const int ops) { _changeEvents = ops; }
        inline void insertChangeEvent(const OperationType &op) { _changeEvents |= op; }
        inline void deleteChangeEvent(const OperationType &op) { _changeEvents ^= op; }
        inline void clearChangeEvents() { _changeEvents = OperationTypeNone; }
        inline bool hasChangeEvent() const { return _changeEvents != OperationTypeNone; }
        inline bool hasChangeEvent(const int op) const { return _changeEvents & op; }

        inline void insertConflictAlreadyConsidered(const ConflictType &conf) { _conflictsAlreadyConsidered.push_back(conf); }
        inline void clearConflictAlreadyConsidered() { _conflictsAlreadyConsidered.clear(); }

        bool isEditFromDeleteCreate() const;

        [[nodiscard]] bool isRoot() const;
        [[nodiscard]] bool isCommonDocumentsFolder() const;
        [[nodiscard]] bool isSharedFolder() const;

        [[nodiscard]] SyncPath getPath() const;

        [[nodiscard]] inline bool isTmp() const { return _isTmp; }
        inline void setIsTmp(bool newIsTmp) { _isTmp = newIsTmp; }

    private:
        std::optional<DbNodeId> _idb = std::nullopt;
        ReplicaSide _side = ReplicaSideUnknown;
        SyncName _name;  // /!\ Must be in NFC form
        InconsistencyType _inconsistencyType = InconsistencyTypeNone;
        NodeType _type = NodeTypeUnknown;
        int _changeEvents = OperationTypeNone;
        std::optional<NodeId> _id = std::nullopt;
        std::optional<NodeId> _previousId = std::nullopt;
        std::optional<SyncTime> _createdAt = std::nullopt;
        std::optional<SyncTime> _lastModified = std::nullopt;
        int64_t _size = 0;
        NodeStatus _status = NodeStatusUnprocessed;  // node was already processed during reconciliation
        std::unordered_map<NodeId, std::shared_ptr<Node>> _childrenById;
        std::shared_ptr<Node> _parentNode = nullptr;
        // For moved items
        std::optional<SyncPath> _moveOrigin = std::nullopt;            // path before it was moved
        std::optional<DbNodeId> _moveOriginParentDbId = std::nullopt;  // parent dir id before it was moved
        // For conflicts resolutions
        std::vector<ConflictType> _conflictsAlreadyConsidered;

        bool _isTmp = false;
};

}  // namespace KDC
