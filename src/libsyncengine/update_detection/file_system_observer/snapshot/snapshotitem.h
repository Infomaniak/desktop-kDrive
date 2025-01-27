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

#include "libcommon/utility/types.h"
#include "libcommonserver/utility/utility.h"

#include <string>
#include <unordered_set>

namespace KDC {

class Snapshot;

class SnapshotItem {
    public:
        SnapshotItem() = default;
        explicit SnapshotItem(const NodeId &id);
        SnapshotItem(const NodeId &id, const NodeId &parentId, const SyncName &name, SyncTime createdAt, SyncTime lastModified,
                     NodeType type, int64_t size, bool isLink, bool canWrite, bool canShare);
        SnapshotItem(const SnapshotItem &other);

        [[nodiscard]] const NodeId &id() const { return _id; }
        void setId(const NodeId &id) { _id = id; }
        [[nodiscard]] const NodeId &parentId() const { return _parentId; }
        void setParentId(const NodeId &newParentId) { _parentId = newParentId; }
        [[nodiscard]] const std::unordered_set<NodeId> &childrenIds() const { return _childrenIds; }
        void setChildrenIds(const std::unordered_set<NodeId> &newChildrenIds) { _childrenIds = newChildrenIds; }
        [[nodiscard]] const SyncName &name() const { return _name; }
        [[nodiscard]] const SyncName &normalizedName() const { return _normalizedName; }
        void setName(const SyncName &newName) {
            _name = newName;
            if (!Utility::normalizedSyncName(newName, _normalizedName)) {
                _normalizedName = newName;
                LOGW_WARN(Log::instance()->getLogger(), L"Failed to normalize: " << Utility::formatSyncName(newName));
            }
        }
        [[nodiscard]] SyncTime createdAt() const { return _createdAt; }
        void setCreatedAt(const SyncTime newCreatedAt) { _createdAt = newCreatedAt; }
        [[nodiscard]] SyncTime lastModified() const { return _lastModified; }
        void setLastModified(const SyncTime newLastModified) { _lastModified = newLastModified; }
        [[nodiscard]] NodeType type() const { return _type; }
        void setType(const NodeType type) { _type = type; }
        [[nodiscard]] int64_t size() const { return _size; }
        void setSize(const int64_t newSize) { _size = newSize; }
        [[nodiscard]] bool isLink() const { return _isLink; }
        void setIsLink(const bool isLink) { _isLink = isLink; }
        [[nodiscard]] const std::string &contentChecksum() const { return _contentChecksum; }
        void setContentChecksum(const std::string &newChecksum) { _contentChecksum = newChecksum; }
        [[nodiscard]] bool canWrite() const { return _canWrite; }
        void setCanWrite(const bool canWrite) { _canWrite = canWrite; }
        [[nodiscard]] bool canShare() const { return _canShare; }
        void setCanShare(bool canShare) { _canShare = canShare; }

        SnapshotItem &operator=(const SnapshotItem &other);

        void copyExceptChildren(const SnapshotItem &other);
        void addChildren(const NodeId &id);
        void removeChildren(const NodeId &id);

    private:
        NodeId _id;
        NodeId _parentId;

        SyncName _name;
        SyncName _normalizedName;
        SyncTime _createdAt = 0;
        SyncTime _lastModified = 0;
        NodeType _type = NodeType::Unknown;
        int64_t _size = 0;
        bool _isLink = false;
        std::string _contentChecksum;
        bool _canWrite = true;
        bool _canShare = true;

        std::unordered_set<NodeId> _childrenIds;

        mutable SyncPath _path; // The item relative path. Cached value. To use only on a snapshot copy, not a real time one.

        [[nodiscard]] SyncPath path() const { return _path; }
        void setPath(const SyncPath &path) const { _path = path; }

        friend class Snapshot;
        // friend bool Snapshot::path(const NodeId &, SyncPath &, bool &) const noexcept;
};

} // namespace KDC
