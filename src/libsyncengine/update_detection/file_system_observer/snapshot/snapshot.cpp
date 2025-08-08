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

#include "snapshot.h"
#include "libcommonserver/log/log.h"
#include "requests/parameterscache.h"

#include <filesystem>
#include <queue>
#include <vector>
#include <clocale>

#include <log4cplus/loggingmacros.h>

namespace KDC {

Snapshot::Snapshot(ReplicaSide side, const NodeId &rootFolderId) :
    _side(side),
    _rootFolderId(rootFolderId) {}

Snapshot::Snapshot(Snapshot const &other) {
    if (this != &other) {
        const std::scoped_lock lock(_mutex, other._mutex);
        _side = other._side;
        _rootFolderId = other._rootFolderId;
        _items.clear();
        _revision = other.revision();

        for (const auto &item: other._items) {
            _items.try_emplace(item.first, std::make_shared<SnapshotItem>(*item.second));
        }

        // Update the child list
        for (const auto &[_, item]: _items) {
            NodeSet childrenIds;
            for (const auto &child: item->children()) {
                (void) childrenIds.insert(child->id());
            }
            item->removeAllChildren();
            for (const auto &childId: childrenIds) {
                const auto itemIt = _items.find(childId);
                if (itemIt == _items.end()) {
                    LOG_WARN(Log::instance()->getLogger(), "Item id=" << childId << " not found in snapshot");
                    continue;
                }

                item->addChild(itemIt->second); // Add the new pointer
            }
        }
    }
}

NodeId Snapshot::itemId(const SyncPath &path) const {
    const std::scoped_lock lock(_mutex);

    const auto rootItemIt = _items.find(rootFolderId());
    if (rootItemIt == _items.end()) {
        LOG_WARN(Log::instance()->getLogger(), "Root folder id not found in snapshot");
        return "";
    }

    auto item = rootItemIt->second;
    for (auto pathIt = path.begin(); pathIt != path.end(); pathIt++) {
#ifndef KD_WINDOWS
        if (pathIt->lexically_normal() == SyncPath(Str("/")).lexically_normal()) {
            continue;
        }
#endif

        bool idFound = false;
        for (const auto &child: item->children()) {
            if (child->name() == *pathIt) {
                item = child;
                idFound = true;
                break;
            }
        }

        if (!idFound) {
            return "";
        }
    }

    return item->id();
}

NodeId Snapshot::parentId(const NodeId &itemId) const {
    const std::scoped_lock lock(_mutex);
    NodeId ret;
    if (const auto item = findItem(itemId); item) {
        ret = item->parentId();
    }
    return ret;
}

bool Snapshot::path(const NodeId &itemId, SyncPath &path, bool &ignore) const noexcept {
    path.clear();
    ignore = false;

    if (itemId.empty()) {
        LOG_WARN(Log::instance()->getLogger(), "Error in Snapshot::path: empty item ID argument.");
        return false;
    }

    bool ok = true;
    std::deque<std::pair<NodeId, SyncName>> ancestors;
    {
        bool parentIsRoot = false;
        NodeId id = itemId;
        const std::scoped_lock lock(_mutex);
        while (!parentIsRoot) {
            if (const auto item = findItem(id); item) {
                if (!item->path().empty()) {
                    path = item->path();
                    break;
                }
                ancestors.push_back({item->id(), item->name()});
                id = item->parentId();
                parentIsRoot = id == _rootFolderId;
                continue;
            }
            ok = false;
            break;
        }
    }

    // Construct path
    SyncPath tmpParentPath(path);
    while (!ancestors.empty()) {
        path /= ancestors.back().second;
        const auto item = findItem(ancestors.back().first);
        assert(item);
        item->setPath(path);

        ancestors.pop_back();
    }

    // Trick to ignore items with a pattern like "X:" in their names on Windows.
    // Since only relative path are stored in the liveSnapshot, the root name should always be empty.
    // If it is not empty, that means that the item name is invalid.
    if (!path.root_name().empty()) {
        ignore = true;
        return false;
    }

    return ok;
}

SyncName Snapshot::name(const NodeId &itemId) const {
    const std::scoped_lock lock(_mutex);
    if (const auto item = findItem(itemId); item) {
        return item->name();
    }
    return SyncName();
}

SyncTime Snapshot::createdAt(const NodeId &itemId) const {
    const std::scoped_lock lock(_mutex);
    if (const auto item = findItem(itemId); item) {
        return item->createdAt();
    }
    return 0;
}

SyncTime Snapshot::lastModified(const NodeId &itemId) const {
    const std::scoped_lock lock(_mutex);
    if (const auto item = findItem(itemId); item) {
        return item->lastModified();
    }
    return 0;
}

NodeType Snapshot::type(const NodeId &itemId) const {
    const std::scoped_lock lock(_mutex);
    if (const auto item = findItem(itemId); item) {
        return item->type();
    }
    return NodeType::Unknown;
}

int64_t Snapshot::size(const NodeId &itemId) const {
    const std::scoped_lock lock(_mutex);
    if (const auto item = findItem(itemId); item) {
        return item->size();
    }
    return 0;
}

std::string Snapshot::contentChecksum(const NodeId &itemId) const {
    const std::scoped_lock lock(_mutex);
    if (const auto item = findItem(itemId); item) {
        return item->contentChecksum();
    }
    return "";
}

bool Snapshot::canWrite(const NodeId &itemId) const {
    const std::scoped_lock lock(_mutex);
    if (const auto item = findItem(itemId); item) {
        return item->canWrite();
    }
    return true;
}

bool Snapshot::canShare(const NodeId &itemId) const {
    const std::scoped_lock lock(_mutex);
    if (const auto item = findItem(itemId); item) {
        return item->canShare();
    }
    return true;
}

bool Snapshot::exists(const NodeId &itemId) const {
    const std::scoped_lock lock(_mutex);
    return findItem(itemId) && !isOrphan(itemId);
}

bool Snapshot::pathExists(const SyncPath &path) const {
    const std::scoped_lock lock(_mutex);
    return !itemId(path).empty();
}

bool Snapshot::isLink(const NodeId &itemId) const {
    const std::scoped_lock lock(_mutex);
    if (const auto item = findItem(itemId); item) {
        return item->isLink();
    }
    return false;
}

SnapshotRevision Snapshot::lastChangeRevision(const NodeId &itemId) const {
    const std::scoped_lock lock(_mutex);
    if (const auto item = findItem(itemId); item) {
        return item->lastChangeRevision();
    }
    return 0;
}

bool Snapshot::getChildren(const NodeId &itemId, std::unordered_set<std::shared_ptr<SnapshotItem>> &children) const {
    const std::scoped_lock lock(_mutex);
    if (const auto item = findItem(itemId); item) {
        children = item->children();
        return true;
    }
    return false;
}

std::shared_ptr<SnapshotItem> Snapshot::findItem(const NodeId &itemId) const {
    const std::scoped_lock lock(_mutex);
    if (const auto it = _items.find(itemId); it != _items.end()) {
        return it->second;
    }
    return nullptr;
}

bool Snapshot::getChildrenIds(const NodeId &itemId, NodeSet &childrenIds) const {
    const std::scoped_lock lock(_mutex);
    std::unordered_set<std::shared_ptr<SnapshotItem>> children;
    if (!getChildren(itemId, children)) {
        return false;
    }
    childrenIds.clear();
    for (const auto &child: children) {
        (void) childrenIds.insert(child->id());
    }
    return true;
}

NodeSet Snapshot::getDescendantIds(const NodeId &itemId) const {
    const std::scoped_lock lock(_mutex);
    NodeSet descendantIds;

    getDescendantIds(itemId, descendantIds);

    return descendantIds;
}

void Snapshot::getDescendantIds(const NodeId &itemId, NodeSet &descendantIds) const {
    const std::scoped_lock lock(_mutex);

    std::unordered_set<std::shared_ptr<SnapshotItem>> children;
    getChildren(itemId, children);

    for (const auto &child: children) {
        (void) descendantIds.insert(child->id());
        getDescendantIds(child->id(), descendantIds);
    }
}

void Snapshot::ids(NodeSet &ids) const {
    const std::scoped_lock lock(_mutex);
    ids.clear();
    for (const auto &[id, _]: _items) {
        ids.insert(id);
    }
}

bool Snapshot::isAncestor(const NodeId &itemId, const NodeId &ancestorItemId) const {
    const std::scoped_lock lock(_mutex);
    if (itemId == _rootFolderId) {
        // Root directory cannot have any ancestor
        return false;
    }

    NodeId directParentId = parentId(itemId);
    if (directParentId == ancestorItemId) {
        return true;
    }

    if (directParentId == _rootFolderId) {
        // We have reached the root directory
        return false;
    }

    return isAncestor(directParentId, ancestorItemId);
}

bool Snapshot::isOrphan(const NodeId &itemId) const {
    if (itemId == _rootFolderId) {
        return false;
    }

    NodeId nextParentId = parentId(itemId);
    while (nextParentId != _rootFolderId) {
        const NodeId tmpNextParentId = parentId(nextParentId);
        if (tmpNextParentId.empty()) {
            return true;
        }
        if (tmpNextParentId == nextParentId) {
            // Should not happen
            LOG_WARN(Log::instance()->getLogger(), "Parent ID equals item ID " << nextParentId);
            assert(false);
            break;
        }
        nextParentId = tmpNextParentId;
    }
    return false;
}

bool Snapshot::isEmpty() const {
    const std::scoped_lock lock(_mutex);
    return _items.empty();
}

uint64_t Snapshot::nbItems() const {
    const std::scoped_lock lock(_mutex);
    return _items.size();
}


SnapshotRevision Snapshot::revision() const {
    return _revision;
}

} // namespace KDC
