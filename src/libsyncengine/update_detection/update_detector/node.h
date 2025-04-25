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

#include "utility/types.h"
#include "libcommonserver/utility/utility.h"
#include "libcommon/utility/logiffail.h"

#include <algorithm>
#include <vector>
#include <optional>
#include <unordered_map>

namespace KDC {

class Node {
    public:
        class MoveOriginInfos {
            public:
                MoveOriginInfos() = default;
                MoveOriginInfos(const MoveOriginInfos &) = default;
                MoveOriginInfos(const SyncPath &path, const NodeId &parentNodeId) :
                    _isValid(true), _path(path), _parentNodeId(parentNodeId) {}

                MoveOriginInfos &operator=(const MoveOriginInfos &newMoveOriginInfos) {
                    LOG_IF_FAIL(Log::instance()->getLogger(), newMoveOriginInfos.isValid());
                    _isValid = newMoveOriginInfos.isValid();
                    _path = newMoveOriginInfos.path();
                    _parentNodeId = newMoveOriginInfos.parentNodeId();
                    return *this;
                }
                const SyncPath &path() const;
                const NodeId &parentNodeId() const;

            private:
                bool isValid() const;
                bool _isValid = false;
                SyncPath _path = ":\0/:\0"; // Invalid path for increased safety
                NodeId _parentNodeId = "-1"; // Invalid node id for increased safety
                friend class Node;
                friend class TestUpdateTreeWorker;
        };

    public:
        const static Node _nullNode;

        Node(const std::optional<DbNodeId> &idb, const ReplicaSide &side, const SyncName &name, NodeType type,
             OperationType changeEvents, const std::optional<NodeId> &id, std::optional<SyncTime> createdAt,
             std::optional<SyncTime> lastmodified, int64_t size, std::shared_ptr<Node> parentNode);

        Node(const std::optional<DbNodeId> &idb, const ReplicaSide &side, const SyncName &name, NodeType type,
             OperationType changeEvents, const std::optional<NodeId> &id, std::optional<SyncTime> createdAt,
             std::optional<SyncTime> lastmodified, int64_t size, std::shared_ptr<Node> parentNode,
             const MoveOriginInfos &moveOriginInfos);

        Node(const ReplicaSide &side, const SyncName &name, NodeType type, OperationType changeEvents,
             const std::optional<NodeId> &id, std::optional<SyncTime> createdAt, std::optional<SyncTime> lastmodified,
             int64_t size, std::shared_ptr<Node> parentNode);

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

        bool operator==(const Node &n) const;

        inline std::optional<DbNodeId> idb() const { return _idb; }
        inline ReplicaSide side() const { return _side; }
        inline const SyncName &name() const { return _name; }
        inline NodeType type() const { return _type; }
        inline InconsistencyType inconsistencyType() const { return _inconsistencyType; }
        inline OperationType changeEvents() const { return _changeEvents; }
        inline std::optional<SyncTime> createdAt() const { return _createdAt; }
        inline std::optional<SyncTime> lastmodified() const { return _lastModified; }
        inline int64_t size() const { return _size; }
        inline std::optional<NodeId> id() const { return _id; }
        inline std::optional<NodeId> previousId() const { return _previousId; }
        inline NodeStatus status() const { return _status; }
        inline std::shared_ptr<Node> parentNode() const { return _parentNode; }
        inline const MoveOriginInfos &moveOriginInfos() const { return _moveOriginInfos; }
        inline const std::vector<ConflictType> &conflictsAlreadyConsidered() const { return _conflictsAlreadyConsidered; }
        inline bool hasConflictAlreadyConsidered(const ConflictType conf) const {
            return std::count(_conflictsAlreadyConsidered.cbegin(), _conflictsAlreadyConsidered.cend(), conf) > 0;
        }

        inline void setIdb(const std::optional<DbNodeId> &idb) { _idb = idb; }
        void setName(const SyncName &name) { _name = name; }
        inline void setInconsistencyType(InconsistencyType newInconsistencyType) { _inconsistencyType = newInconsistencyType; }
        inline void addInconsistencyType(InconsistencyType newInconsistencyType) { _inconsistencyType |= newInconsistencyType; }
        inline void setCreatedAt(const std::optional<SyncTime> &createdAt) { _createdAt = createdAt; }
        inline void setLastModified(const std::optional<SyncTime> &lastmodified) { _lastModified = lastmodified; }
        inline void setSize(int64_t size) { _size = size; }
        inline void setPreviousId(const std::optional<NodeId> &previousNodeId) { _previousId = previousNodeId; }
        bool setParentNode(const std::shared_ptr<Node> &parentNode);
        inline void setMoveOriginInfos(const MoveOriginInfos &moveOriginInfos) { _moveOriginInfos = moveOriginInfos; }
        inline void setStatus(const NodeStatus &status) { _status = status; }

        inline std::unordered_map<NodeId, std::shared_ptr<Node>> &children() { return _childrenById; }
        std::shared_ptr<Node> findChildren(const SyncName &name, const NodeId &nodeId = "");
        std::shared_ptr<Node> findChildrenById(const NodeId &nodeId);
        [[nodiscard]] bool insertChildren(std::shared_ptr<Node> child);
        size_t deleteChildren(std::shared_ptr<Node> child);
        size_t deleteChildren(const NodeId &childId);
        /**
         * @brief Retrieve the child node based on its name, which is assumed to be normalized. Filter out the nodes with change
         * events of type `except`.
         * @param normalizedName Make sure to provide a normalized name.
         * @param except The event type to filter out.
         * @return A pointer to the node if found, `nullptr` otherwise.
         */
        std::shared_ptr<Node> getChildExcept(const SyncName &normalizedName, OperationType except);

        void setChangeEvents(OperationType ops);
        void insertChangeEvent(OperationType op);
        void deleteChangeEvent(const OperationType op) { _changeEvents ^= op; }
        void clearChangeEvents() { _changeEvents = OperationType::None; }
        bool hasChangeEvent() const { return _changeEvents != OperationType::None; }
        bool hasChangeEvent(const OperationType op) const { return (_changeEvents & op) == op; }

        void insertConflictAlreadyConsidered(const ConflictType &conf) { _conflictsAlreadyConsidered.push_back(conf); }
        void clearConflictAlreadyConsidered() { _conflictsAlreadyConsidered.clear(); }

        bool isEditFromDeleteCreate() const;

        [[nodiscard]] bool isRoot() const;
        [[nodiscard]] bool isCommonDocumentsFolder() const;
        [[nodiscard]] bool isSharedFolder() const;
        [[nodiscard]] bool isParentOf(std::shared_ptr<const Node> potentialChild) const;

        [[nodiscard]] SyncPath getPath() const;

        [[nodiscard]] inline bool isTmp() const { return _isTmp; }
        inline void setIsTmp(bool newIsTmp) { _isTmp = newIsTmp; }

    private:
        friend class UpdateTree;
        // The node id should not be changed without also changing the map in the UpdateTree and the parent/child relationship in
        // other nodes
        inline void setId(const std::optional<NodeId> &nodeId) { _id = nodeId; }

        std::optional<DbNodeId> _idb = std::nullopt;
        ReplicaSide _side = ReplicaSide::Unknown;
        SyncName _name; // This name is NFC-normalized by constructors and setters.
        InconsistencyType _inconsistencyType = InconsistencyType::None;
        NodeType _type = NodeType::Unknown;
        OperationType _changeEvents = OperationType::None;
        std::optional<NodeId> _id = std::nullopt;
        std::optional<NodeId> _previousId = std::nullopt;
        std::optional<SyncTime> _createdAt = std::nullopt;
        std::optional<SyncTime> _lastModified = std::nullopt;
        int64_t _size = 0;
        NodeStatus _status = NodeStatus::Unprocessed; // node was already processed during reconciliation
        std::unordered_map<NodeId, std::shared_ptr<Node>> _childrenById;
        std::shared_ptr<Node> _parentNode;
        // For moved items
        MoveOriginInfos _moveOriginInfos;
        // For conflicts resolutions
        std::vector<ConflictType> _conflictsAlreadyConsidered;

        bool _isTmp = false;

        [[nodiscard]] bool isParentValid(std::shared_ptr<const Node> parentNode) const;
        friend class TestNode;
};

} // namespace KDC
