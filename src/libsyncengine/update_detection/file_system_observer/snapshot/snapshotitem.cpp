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

#include "snapshotitem.h"

namespace KDC {

SnapshotItem::SnapshotItem(const NodeId &id) :
    _id(id) {}

SnapshotItem::SnapshotItem(const NodeId &id, const NodeId &parentId, const SyncName &name, const SyncTime createdAt,
                           const SyncTime lastModified, const NodeType type, const int64_t size, const bool isLink,
                           const bool canWrite, const bool canShare) :
    _id(id),
    _parentId(parentId),
    _name(name),
    _createdAt(createdAt),
    _lastModified(lastModified),
    _type(type),
    _size(size),
    _isLink(isLink),
    _canWrite(canWrite),
    _canShare(canShare) {
    setName(name); // Needed for the computation of _normalizedName
}

SnapshotItem::SnapshotItem(const SnapshotItem &other) {
    *this = other;
}

void SnapshotItem::setId(const NodeId &id) {
    _id = id;
    _lastChangeRevision = _snapshotRevisionHandler ? _snapshotRevisionHandler->nextVersion() : 0;
}

void SnapshotItem::setParentId(const NodeId &newParentId) {
    _parentId = newParentId;
    _lastChangeRevision = _snapshotRevisionHandler ? _snapshotRevisionHandler->nextVersion() : 0;
}

void SnapshotItem::setName(const SyncName &newName) {
    _name = newName;
    if (!Utility::normalizedSyncName(newName, _normalizedName)) {
        _normalizedName = newName;
        LOGW_WARN(Log::instance()->getLogger(), L"Failed to normalize: " << Utility::formatSyncName(newName));
    }
    _lastChangeRevision = _snapshotRevisionHandler ? _snapshotRevisionHandler->nextVersion() : 0;
}

void SnapshotItem::setCreatedAt(const SyncTime newCreatedAt) {
    _createdAt = newCreatedAt;
    _lastChangeRevision = _snapshotRevisionHandler ? _snapshotRevisionHandler->nextVersion() : 0;
}

void SnapshotItem::setLastModified(const SyncTime newLastModified) {
    _lastModified = newLastModified;
    _lastChangeRevision = _snapshotRevisionHandler ? _snapshotRevisionHandler->nextVersion() : 0;
}

void SnapshotItem::setType(const NodeType type) {
    _type = type;
    _lastChangeRevision = _snapshotRevisionHandler ? _snapshotRevisionHandler->nextVersion() : 0;
}

int64_t SnapshotItem::size() const {
    int64_t ret = 0;
    if (!_children.empty()) {
        for (auto &child: _children) {
            ret += child->size();
        }
    } else {
        ret = _size;
    }
    return ret;
}

void SnapshotItem::setSize(const int64_t newSize) {
    _size = newSize;
    _lastChangeRevision = _snapshotRevisionHandler ? _snapshotRevisionHandler->nextVersion() : 0;
}

void SnapshotItem::setIsLink(const bool isLink) {
    _isLink = isLink;
    _lastChangeRevision = _snapshotRevisionHandler ? _snapshotRevisionHandler->nextVersion() : 0;
}

void SnapshotItem::setContentChecksum(const std::string &newChecksum) {
    _contentChecksum = newChecksum;
    _lastChangeRevision = _snapshotRevisionHandler ? _snapshotRevisionHandler->nextVersion() : 0;
}

void SnapshotItem::setCanWrite(const bool canWrite) {
    _canWrite = canWrite;
    _lastChangeRevision = _snapshotRevisionHandler ? _snapshotRevisionHandler->nextVersion() : 0;
}

void SnapshotItem::setCanShare(bool canShare) {
    _canShare = canShare;
}

void SnapshotItem::setLastChangedSnapshotVersion(SnapshotRevision snapshotVersion) {
    if (snapshotVersion > _lastChangeRevision) {
        _lastChangeRevision = snapshotVersion;
    } else {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"SnapshotItem::setLastChangedSnapshotVersion: "
                  L"Trying to set a lower version than the current one. Current version: "
                          << _lastChangeRevision << L", new version: " << snapshotVersion << L" on " << CommonUtility::s2ws(_id));
    }
}

SnapshotItem &SnapshotItem::operator=(const SnapshotItem &other) {
    copyExceptChildren(other);
    _children = other.children();
    _snapshotRevisionHandler = nullptr;
    return *this;
}

void SnapshotItem::copyExceptChildren(const SnapshotItem &other) {
    // Update all attributes but children list
    _id = other.id();
    _parentId = other.parentId();
    _name = other.name();
    _normalizedName = other.normalizedName();
    _createdAt = other.createdAt();
    _lastModified = other.lastModified();
    _type = other.type();
    _size = other.size();
    _isLink = other.isLink();
    _contentChecksum = other.contentChecksum();
    _canWrite = other.canWrite();
    _canShare = other.canShare();
    _path = other.path();
    if (other.lastChangeRevision() == 0 && _snapshotRevisionHandler) {
        _lastChangeRevision = _snapshotRevisionHandler->nextVersion();
    } else {
        _lastChangeRevision = other.lastChangeRevision();
    }
}

void SnapshotItem::addChild(const std::shared_ptr<SnapshotItem> &child) {
    _children.insert(child);
}

void SnapshotItem::removeChild(const std::shared_ptr<SnapshotItem> &child) {
    _children.erase(child);
}

void SnapshotItem::removeAllChildren() {
    _children.clear();
}

} // namespace KDC
