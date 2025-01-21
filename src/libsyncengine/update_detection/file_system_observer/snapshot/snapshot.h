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

#include "syncpal/sharedobject.h"
#include "snapshotitem.h"
#include "db/dbnode.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>

namespace KDC {

class Snapshot : public SharedObject {
    public:
        Snapshot(ReplicaSide side, const DbNode &dbNode);
        ~Snapshot();

        Snapshot(Snapshot const &) = delete;
        Snapshot &operator=(Snapshot &other);

        void init();

        bool updateItem(const SnapshotItem &newItem);
        bool removeItem(const NodeId id); // Do not pass by reference to avoid dangling references

        NodeId itemId(const SyncPath &path) const;
        NodeId parentId(const NodeId &itemId) const;
        bool setParentId(const NodeId &itemId, const NodeId &newParentId);
        bool path(const NodeId &itemId, SyncPath &path, bool &ignore) const noexcept;
        SyncName name(const NodeId &itemId) const;
        bool setName(const NodeId &itemId, const SyncName &newName);
        SyncTime createdAt(const NodeId &itemId) const;
        bool setCreatedAt(const NodeId &itemId, SyncTime newTime);
        SyncTime lastModified(const NodeId &itemId) const;
        bool setLastModified(const NodeId &itemId, SyncTime newTime);
        NodeType type(const NodeId &itemId) const;
        int64_t size(const NodeId &itemId) const;
        std::string contentChecksum(const NodeId &itemId) const;
        bool setContentChecksum(const NodeId &itemId, const std::string &newChecksum);
        bool canWrite(const NodeId &itemId) const;
        bool canShare(const NodeId &itemId) const;
        bool clearContentChecksum(const NodeId &itemId);
        bool exists(const NodeId &itemId) const;
        bool pathExists(const SyncPath &path) const;
        bool isLink(const NodeId &itemId) const;

        bool getChildrenIds(const NodeId &itemId, std::unordered_set<NodeId> &childrenIds) const;

        void ids(std::unordered_set<NodeId> &ids) const;
        /** Checks if ancestorItem is an ancestor of item.
         * @return true indicates that ancestorItem is an ancestor of item
         */
        bool isAncestor(const NodeId &itemId, const NodeId &ancestorItemId) const;
        bool isOrphan(const NodeId &itemId) const;

        [[nodiscard]] inline ReplicaSide side() const { return _side; }

        [[nodiscard]] inline NodeId rootFolderId() const { return _rootFolderId; }
        inline void setRootFolderId(const NodeId &nodeId) { _rootFolderId = nodeId; }

        bool isEmpty() const;
        uint64_t nbItems() const;

        bool isValid() const;
        void setValid(bool newIsValid);

        bool checkIntegrityRecursively() const;

    private:
        void removeChildrenRecursively(const NodeId &parentId);
        bool checkIntegrityRecursively(const NodeId &parentId) const;

        ReplicaSide _side = ReplicaSide::Unknown;
        NodeId _rootFolderId;
        std::unordered_map<NodeId, SnapshotItem> _items; // key: id
        bool _isValid = false;
        bool _copy = false; // false for a real time snapshot, true for a copy

        mutable std::recursive_mutex _mutex;
};

} // namespace KDC
