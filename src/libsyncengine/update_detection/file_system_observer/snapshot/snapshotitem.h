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

#include "libcommon/utility/types.h"
#include "libcommonserver/utility/utility.h"

#include <string>
#include <unordered_set>

namespace KDC {

class SnapshotItem {
    public:
        SnapshotItem();
        SnapshotItem(const NodeId &id);
        SnapshotItem(const NodeId &id, const NodeId &parentId, const SyncName &name, SyncTime createdAt, SyncTime lastModified,
                     NodeType type, int64_t size, bool canWrite = true, const std::string &contentChecksum = "",
                     bool canShare = true);
        SnapshotItem(const SnapshotItem &other);

        inline NodeId id() const { return _id; }
        inline void setId(const NodeId &id) { _id = id; }
        inline NodeId parentId() const { return _parentId; }
        inline void setParentId(const NodeId &newParentId) { _parentId = newParentId; }
        inline const std::unordered_set<NodeId> &childrenIds() const { return _childrenIds; }
        inline void setChildrenIds(const std::unordered_set<NodeId> &newChildrenIds) { _childrenIds = newChildrenIds; }
        inline SyncName name() const { return _name; }
        inline void setName(const SyncName &newName) { _name = Utility::normalizedSyncName(newName); }
        inline SyncTime createdAt() const { return _createdAt; }
        inline void setCreatedAt(SyncTime newCreatedAt) { _createdAt = newCreatedAt; }
        inline SyncTime lastModified() const { return _lastModified; }
        inline void setLastModified(SyncTime newLastModified) { _lastModified = newLastModified; }
        inline NodeType type() const { return _type; }
        inline void setType(NodeType type) { _type = type; }
        inline int64_t size() const { return _size; }
        inline void setSize(uint64_t newSize) { _size = newSize; }
        inline std::string contentChecksum() const { return _contentChecksum; }
        inline void setContentChecksum(const std::string &newChecksum) { _contentChecksum = newChecksum; }
        inline bool canWrite() const { return _canWrite; }
        inline void setCanWrite(bool canWrite) { _canWrite = canWrite; }
        inline bool canShare() const { return _canShare; }
        inline void setCanShare(bool canShare) { _canShare = canShare; }
        inline bool isLink() const { return _isLink; }
        inline void setIsLink(bool isLink) { _isLink = isLink; }
        SnapshotItem &operator=(const SnapshotItem &other);

        void addChildren(const NodeId &id);
        void removeChildren(const NodeId &id);

    private:
        NodeId _id;
        NodeId _parentId;

        SyncName _name;
        SyncTime _createdAt = 0;
        SyncTime _lastModified = 0;
        NodeType _type = NodeTypeUnknown;
        int64_t _size = 0;
        std::string _contentChecksum;
        bool _canWrite = true;
        bool _canShare = true;
        bool _isLink = false;

        std::unordered_set<NodeId> _childrenIds;
};

}  // namespace KDC
