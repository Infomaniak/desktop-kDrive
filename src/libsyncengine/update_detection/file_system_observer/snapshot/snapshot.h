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

#include "snapshotitem.h"
#include "snapshotrevisionhandler.h"
#include "db/dbnode.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>

namespace KDC {

class Snapshot {
    public:
        Snapshot(const Snapshot &);
        virtual ~Snapshot() = default;
        NodeId itemId(const SyncPath &path) const;
        NodeId parentId(const NodeId &itemId) const;
        virtual bool path(const NodeId &itemId, SyncPath &path, bool &ignore) const noexcept;
        SyncName name(const NodeId &itemId) const;
        SyncTime createdAt(const NodeId &itemId) const;
        SyncTime lastModified(const NodeId &itemId) const;
        NodeType type(const NodeId &itemId) const;
        int64_t size(const NodeId &itemId) const;
        std::string contentChecksum(const NodeId &itemId) const;
        bool canWrite(const NodeId &itemId) const;
        bool canShare(const NodeId &itemId) const;
        bool exists(const NodeId &itemId) const;
        bool pathExists(const SyncPath &path) const;
        bool isLink(const NodeId &itemId) const;
        SnapshotRevision lastChangeRevision(const NodeId &itemId) const;
        bool getChildrenIds(const NodeId &itemId, NodeSet &childrenIds) const;
        NodeSet getDescendantIds(const NodeId &itemId) const;

        void ids(NodeSet &ids) const;
        /** Checks if ancestorItem is an ancestor of item.
         * @return true indicates that ancestorItem is an ancestor of item
         */
        bool isAncestor(const NodeId &itemId, const NodeId &ancestorItemId) const;
        bool isOrphan(const NodeId &itemId) const;

        [[nodiscard]] inline ReplicaSide side() const { return _side; }

        [[nodiscard]] inline NodeId rootFolderId() const { return _rootFolderId; }

        bool isEmpty() const;
        uint64_t nbItems() const;

        virtual SnapshotRevision revision() const;

    protected:
        Snapshot(ReplicaSide side, const NodeId &rootFolderId);

        class SnapshotItemUnorderedMap : public std::unordered_map<NodeId, std::shared_ptr<SnapshotItem>> {
            public:
                // To ensure the integrity of the snapshot, we need to make sure that it is not used anywhere (i.e., as a
                // child of another item) when removing it from the main map.
                void erase(const NodeId &id) {
                    const auto it = std::unordered_map<NodeId, std::shared_ptr<SnapshotItem>>::find(id);
                    assert(it->second.use_count() == 1);
                    (void) std::unordered_map<NodeId, std::shared_ptr<SnapshotItem>>::erase(it);
                }
        };

        SnapshotItemUnorderedMap _items; // key: id

        std::shared_ptr<SnapshotItem> findItem(const NodeId &itemId) const;
        mutable std::recursive_mutex _mutex;

    private:
        SnapshotRevision _revision = 0;
        bool getChildren(const NodeId &itemId, std::unordered_set<std::shared_ptr<SnapshotItem>> &children) const;
        void getDescendantIds(const NodeId &itemId, NodeSet &descendantIds) const;

        ReplicaSide _side = ReplicaSide::Unknown;
        NodeId _rootFolderId;


        friend class TestSnapshot;
};

class ConstSnapshot : public Snapshot {
    public:
        explicit ConstSnapshot(const Snapshot &other) :
            Snapshot(other) {}

    private:
        // Prevent any derived class to modify the snapshot content.
        using Snapshot::_items;
        using Snapshot::_mutex;
        using Snapshot::findItem;
};
} // namespace KDC
