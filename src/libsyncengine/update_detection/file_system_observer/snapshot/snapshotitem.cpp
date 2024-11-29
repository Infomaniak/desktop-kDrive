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

#include "snapshotitem.h"

namespace KDC {

SnapshotItem::SnapshotItem() {}

SnapshotItem::SnapshotItem(const NodeId &id) : _id(id) {}

SnapshotItem::SnapshotItem(const NodeId &id, const NodeId &parentId, const SyncName &name, SyncTime createdAt,
                           SyncTime lastModified, NodeType type, int64_t size, bool isLink /*= false*/, bool canWrite /*= true*/,
                           bool canShare /*= true*/) :
    _id(id),
    _parentId(parentId), _name(name), _createdAt(createdAt), _lastModified(lastModified), _type(type), _size(size),
    _isLink(isLink), _canWrite(canWrite), _canShare(canShare) {
    setName(name); // Needed for the computation of _normalizedName
}

SnapshotItem::SnapshotItem(const SnapshotItem &other) {
    *this = other;
}

SnapshotItem &SnapshotItem::operator=(const SnapshotItem &other) {
    copyExceptChildren(other);
    _childrenIds = other.childrenIds();

    return *this;
}

void SnapshotItem::copyExceptChildren(const SnapshotItem &other) {
    // Update all attributes but children list
    _id = other.id();
    _parentId = other.parentId();
    _name = other.name();
    _createdAt = other.createdAt();
    _lastModified = other.lastModified();
    _type = other.type();
    _size = other.size();
    _isLink = other.isLink();
    _contentChecksum = other.contentChecksum();
    _canWrite = other.canWrite();
    _canShare = other.canShare();
}

void SnapshotItem::addChildren(const NodeId &id) {
    _childrenIds.insert(id);
}

void SnapshotItem::removeChildren(const NodeId &id) {
    _childrenIds.erase(id);
}

} // namespace KDC
