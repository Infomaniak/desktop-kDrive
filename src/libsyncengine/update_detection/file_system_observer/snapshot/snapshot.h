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

        bool updateItem(const SnapshotItem &item);
        bool removeItem(const NodeId &id);

        NodeId itemId(const SyncPath &path);
        NodeId parentId(const NodeId &itemId);
        bool setParentId(const NodeId &itemId, const NodeId &newParentId);
        bool path(const NodeId &itemId, SyncPath &path) const noexcept;
        SyncName name(const NodeId &itemId);
        bool setName(const NodeId &itemId, const SyncName &newName);
        SyncTime createdAt(const NodeId &itemId);
        bool setCreatedAt(const NodeId &itemId, SyncTime newTime);
        SyncTime lastModified(const NodeId &itemId);
        bool setLastModified(const NodeId &itemId, SyncTime newTime);
        NodeType type(const NodeId &itemId);
        int64_t size(const NodeId &itemId);
        std::string contentChecksum(const NodeId &itemId);
        bool setContentChecksum(const NodeId &itemId, const std::string &newChecksum);
        bool canWrite(const NodeId &itemId);
        bool canShare(const NodeId &itemId);
        bool clearContentChecksum(const NodeId &itemId);
        bool exists(const NodeId &itemId);
        bool pathExists(const SyncPath &path);
        bool isLink(const NodeId &itemId);

        bool getChildrenIds(const NodeId &itemId, std::unordered_set<NodeId> &childrenIds);

        void ids(std::unordered_set<NodeId> &ids);
        /** Checks if ancestorItem is an ancestor of item.
         * @return true indicates that ancestorItem is an ancestor of item
         */
        bool isAncestor(const NodeId &itemId, const NodeId &ancestorItemId);
        bool isOrphan(const NodeId &itemId);

        inline ReplicaSide side() const { return _side; }

        inline NodeId rootFolderId() const { return _rootFolderId; }
        inline void setRootFolderId(const NodeId &nodeId) { _rootFolderId = nodeId; }

        bool isEmpty();
        uint64_t nbItems();

        bool isValid();
        void setValid(bool newIsValid);

    private:
        void removeChildrenRecursively(const NodeId &parentId);

        ReplicaSide _side = ReplicaSideUnknown;
        NodeId _rootFolderId;
        std::unordered_map<NodeId, SnapshotItem> _items;  // key: id
        bool _isValid = false;
        mutable std::recursive_mutex _mutex;

        friend class TestSnapshot;
};

}  // namespace KDC
