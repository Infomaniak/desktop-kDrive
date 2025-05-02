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
#include "snapshotrevisionhandler.h"

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
        void setId(const NodeId &id);
        [[nodiscard]] const NodeId &parentId() const { return _parentId; }
        void setParentId(const NodeId &newParentId);
        [[nodiscard]] const std::unordered_set<std::shared_ptr<SnapshotItem>> &children() const { return _children; }
        [[nodiscard]] const SyncName &name() const { return _name; }
        [[nodiscard]] const SyncName &normalizedName() const { return _normalizedName; }
        [[nodiscard]] const size_t &normalizedNameSize() const { return _normalizedNameSize; }
        void setName(const SyncName &newName);
        [[nodiscard]] SyncTime createdAt() const { return _createdAt; }
        void setCreatedAt(const SyncTime newCreatedAt);
        [[nodiscard]] SyncTime lastModified() const { return _lastModified; }
        void setLastModified(const SyncTime newLastModified);
        [[nodiscard]] NodeType type() const { return _type; }
        void setType(const NodeType type);
        [[nodiscard]] int64_t size() const;
        void setSize(const int64_t newSize);
        [[nodiscard]] bool isLink() const { return _isLink; }
        void setIsLink(const bool isLink);
        [[nodiscard]] const std::string &contentChecksum() const { return _contentChecksum; }
        void setContentChecksum(const std::string &newChecksum);
        [[nodiscard]] bool canWrite() const { return _canWrite; }
        void setCanWrite(const bool canWrite);
        [[nodiscard]] bool canShare() const { return _canShare; }
        void setCanShare(bool canShare);
        void setLastChangedSnapshotVersion(SnapshotRevision snapshotVersion);
        SnapshotRevision lastChangeRevision() const { return _lastChangeRevision; }

        void setSnapshotRevisionHandler(const std::shared_ptr<SnapshotRevisionHandler> &snapshotRevisionHandler) {
            _snapshotRevisionHandler = snapshotRevisionHandler;
            _lastChangeRevision = _snapshotRevisionHandler ? _snapshotRevisionHandler->nextVersion() : 0;
        }
        SnapshotItem &operator=(const SnapshotItem &other);

        void copyExceptChildren(const SnapshotItem &other);
        void addChild(const std::shared_ptr<SnapshotItem> &child);
        void removeChild(const std::shared_ptr<SnapshotItem> &child);
        void removeAllChildren();

    private:
        NodeId _id;
        NodeId _parentId;

        SyncName _name;
        SyncName _normalizedName;
        int16_t _normalizedNameSize = 0;
        SyncTime _createdAt = 0;
        SyncTime _lastModified = 0;
        NodeType _type = NodeType::Unknown;
        int64_t _size = 0;
        bool _isLink = false;
        std::string _contentChecksum;
        bool _canWrite = true;
        bool _canShare = true;
        std::unordered_set<std::shared_ptr<SnapshotItem>> _children;
        SnapshotRevision _lastChangeRevision = 0; // The revision of the snapshot corresponding to the last change of this item.
        std::shared_ptr<SnapshotRevisionHandler> _snapshotRevisionHandler;
        mutable SyncPath _path; // The item relative path. Cached value. To use only on a snapshot copy, not a real time one.

        [[nodiscard]] SyncPath path() const { return _path; }
        void setPath(const SyncPath &path) const { _path = path; }

        friend class Snapshot;
        // friend bool Snapshot::path(const NodeId &, SyncPath &, bool &) const noexcept;
};

} // namespace KDC
