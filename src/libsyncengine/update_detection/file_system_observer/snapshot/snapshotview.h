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
#include "snapshotrevisionhandler.h"
#include "db/dbnode.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>

namespace KDC {

class SnapshotView {
    public:
        ~SnapshotView();

        NodeId itemId(const SyncPath &path) const;
        NodeId parentId(const NodeId &itemId) const;
        SyncName name(const NodeId &itemId) const;
        virtual bool path(const NodeId &itemId, SyncPath &path, bool &ignore) const noexcept;
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

        bool isValid() const;
        SnapshotRevision revision() const;

        bool checkIntegrityRecursively() const;

    protected:
        SnapshotView();
        SnapshotView(SnapshotView const &);
        SnapshotView &operator=(const SnapshotView &other) = delete;

        mutable std::recursive_mutex _mutex;

        class SnapshotItemUnorderedMap : public std::unordered_map<NodeId, std::shared_ptr<SnapshotItem>> {
            public:
                // To ensure the integrity of the liveSnapshot, we need to make sure that it is not used anywhere (i.e., as a child of
                // another item) when removing it from the main map.
                void erase(const NodeId &id) {
                    const auto it = std::unordered_map<NodeId, std::shared_ptr<SnapshotItem>>::find(id);
                    assert(it->second.use_count() == 1);
                    (void) std::unordered_map<NodeId, std::shared_ptr<SnapshotItem>>::erase(it);
                }
        };
        SnapshotItemUnorderedMap _items; // key: id
        bool getChildren(const NodeId &itemId, std::unordered_set<std::shared_ptr<SnapshotItem>> &children) const;
        std::shared_ptr<SnapshotItem> findItem(const NodeId &itemId) const;
        bool checkIntegrityRecursively(const std::shared_ptr<SnapshotItem> &parent) const;

        void setSide(ReplicaSide side) { _side = side; }
        void setRootFolderId(const NodeId &nodeId) { _rootFolderId = nodeId; }
        std::shared_ptr<SnapshotRevisionHandler> const revisionHandler() { return _revisionHandlder; }
        bool _isValid = false;

    private:
        ReplicaSide _side = ReplicaSide::Unknown;
        NodeId _rootFolderId;
        std::shared_ptr<SnapshotRevisionHandler> _revisionHandlder;

        friend class TestSnapshot;
};

} // namespace KDC
