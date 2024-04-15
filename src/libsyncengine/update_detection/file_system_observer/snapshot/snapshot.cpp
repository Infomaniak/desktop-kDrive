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

#include "snapshot.h"
#include "libcommonserver/log/log.h"

#include <filesystem>
#include <queue>
#include <vector>
#include <clocale>

#include <log4cplus/loggingmacros.h>

namespace KDC {

Snapshot::Snapshot(ReplicaSide side, const DbNode &dbNode)
    : _side(side),
      _rootFolderId(side == ReplicaSide::ReplicaSideLocal ? dbNode.nodeIdLocal().value() : dbNode.nodeIdRemote().value()) {
    _items.insert({_rootFolderId, SnapshotItem(_rootFolderId)});
}

void Snapshot::init() {
    const std::scoped_lock lock(_mutex);
    startUpdate();

    _items.clear();
    _items.insert({_rootFolderId, SnapshotItem(_rootFolderId)});

    _isValid = false;
}

bool Snapshot::updateItem(const SnapshotItem &newItem) {
    const std::scoped_lock lock(_mutex);

    if (newItem.parentId().empty()) {
        LOG_WARN(Log::instance()->getLogger(), "Parent ID is empty for item " << newItem.id().c_str());
        return false;
    }

    SnapshotItem &prevItem = _items[newItem.id()];

    // Update parent's children lists
    bool parentChanged = false;
    if (prevItem.id() != _rootFolderId && prevItem.parentId() != newItem.parentId()) {
        parentChanged = true;

        // Remove children from previous parent
        auto itParent = _items.find(prevItem.parentId());
        if (itParent != _items.end()) {
            itParent->second.removeChildren(newItem.id());
        }

        // Add children to new parent
        itParent = _items.find(newItem.parentId());
        if (itParent == _items.end()) {
            // New parent not found, create it
            LOG_DEBUG(Log::instance()->getLogger(),
                      "Parent " << newItem.parentId().c_str() << " does not exist yet, creating it");
            itParent = _items.insert({newItem.parentId(), SnapshotItem(newItem.parentId())}).first;
        }

        itParent->second.addChildren(newItem.id());
    }

    // Update item
    _items[newItem.id()] = newItem;

    if (parentChanged || !isOrphan(newItem.id())) {
        startUpdate();
    }

    return true;
}

bool Snapshot::removeItem(const NodeId &id) {
    const std::scoped_lock lock(_mutex);

    if (id.empty()) {
        return false;
    }

    auto it = _items.find(id);
    if (it == _items.end()) {
        return true;  // Nothing to delete
    }

    if (!isOrphan(id)) {
        startUpdate();
    }

    // First remove all children
    removeChildrenRecursively(id);

    // Remove it also from its parent's children
    auto parentIt = _items.find(it->second.parentId());
    if (parentIt != _items.end()) {
        parentIt->second.removeChildren(id);
    }

    _items.erase(id);
    return true;
}

NodeId Snapshot::itemId(const SyncPath &path) {
    const std::scoped_lock lock(_mutex);

    NodeId ret;
    auto itemIt = _items.find(_rootFolderId);

    for (auto pathIt = path.begin(); pathIt != path.end(); pathIt++) {
        if (pathIt->native() == Str("/")) {
            continue;
        }

        bool idFound = false;
        for (const NodeId &childId : itemIt->second.childrenIds()) {
            if (name(childId) == Utility::normalizedSyncName(*pathIt)) {
                itemIt = _items.find(childId);
                ret = childId;
                idFound = true;
                break;
            }
        }

        if (!idFound) {
            return "";
        }
    }

    return ret;
}

NodeId Snapshot::parentId(const NodeId &itemId) {
    const std::scoped_lock lock(_mutex);
    NodeId ret;
    auto it = _items.find(itemId);
    if (it != _items.end()) {
        ret = it->second.parentId();
    }
    return ret;
}

bool Snapshot::setParentId(const NodeId &itemId, const NodeId &newParentId) {
    const std::scoped_lock lock(_mutex);
    auto it = _items.find(itemId);
    if (it != _items.end()) {
        it->second.setParentId(newParentId);

        if (!isOrphan(itemId)) {
            startUpdate();
        }
        return true;
    }

    return false;
}

bool Snapshot::path(const NodeId &itemId, SyncPath &path) {
    const std::scoped_lock lock(_mutex);
    bool ok = true;
    std::deque<SyncName> names;

    bool parentIsRoot = false;
    NodeId id = itemId;
    while (!parentIsRoot) {
        auto it = _items.find(id);
        if (it != _items.end()) {
            names.push_back(it->second.name());
            id = it->second.parentId();
            parentIsRoot = id == _rootFolderId;
            continue;
        }

        ok = false;
        break;
    }

    // Construct path
    path.clear();
    SyncName tmp;
    while (!names.empty()) {
        tmp.append(names.back());
        tmp.append(Str("/"));
        names.pop_back();
    }
    tmp.pop_back();  // Remove the last '/'
    path = tmp;

    return ok;
}

SyncName Snapshot::name(const NodeId &itemId) {
    const std::scoped_lock lock(_mutex);
    SyncName ret;
    auto it = _items.find(itemId);
    if (it != _items.end()) {
        ret = it->second.name();
    }
    return ret;
}

bool Snapshot::setName(const NodeId &itemId, const SyncName &newName) {
    const std::scoped_lock lock(_mutex);
    auto it = _items.find(itemId);
    if (it != _items.end()) {
        it->second.setName(newName);

        if (!isOrphan(itemId)) {
            startUpdate();
        }
        return true;
    }
    return false;
}

SyncTime Snapshot::createdAt(const NodeId &itemId) {
    const std::scoped_lock lock(_mutex);
    SyncTime ret = 0;
    auto it = _items.find(itemId);
    if (it != _items.end()) {
        ret = it->second.createdAt();
    }
    return ret;
}

bool Snapshot::setCreatedAt(const NodeId &itemId, SyncTime newTime) {
    const std::scoped_lock lock(_mutex);
    auto it = _items.find(itemId);
    if (it != _items.end()) {
        it->second.setCreatedAt(newTime);

        if (!isOrphan(itemId)) {
            startUpdate();
        }
        return true;
    }
    return false;
}

SyncTime Snapshot::lastModifed(const NodeId &itemId) {
    const std::scoped_lock lock(_mutex);
    SyncTime ret = 0;
    auto it = _items.find(itemId);
    if (it != _items.end()) {
        ret = it->second.lastModified();
    }
    return ret;
}

bool Snapshot::setLastModifed(const NodeId &itemId, SyncTime newTime) {
    const std::scoped_lock lock(_mutex);
    auto it = _items.find(itemId);
    if (it != _items.end()) {
        it->second.setLastModified(newTime);

        if (!isOrphan(itemId)) {
            startUpdate();
        }
        return true;
    }
    return false;
}

NodeType Snapshot::type(const NodeId &itemId) {
    const std::scoped_lock lock(_mutex);
    NodeType ret = NodeTypeUnknown;
    auto it = _items.find(itemId);
    if (it != _items.end()) {
        ret = it->second.type();
    }
    return ret;
}

int64_t Snapshot::size(const NodeId &itemId) {
    const std::scoped_lock lock(_mutex);
    int64_t ret = 0;
    if (type(itemId) == NodeTypeDirectory) {
        std::unordered_set<NodeId> childrenIds;
        getChildrenIds(itemId, childrenIds);
        for (auto &childId : childrenIds) {
            ret += size(childId);
        }
    } else {
        auto it = _items.find(itemId);
        if (it != _items.end()) {
            ret = it->second.size();
        }
    }
    return ret;
}

std::string Snapshot::contentChecksum(const NodeId &itemId) {
    const std::scoped_lock lock(_mutex);
    std::string ret;
    auto it = _items.find(itemId);
    if (it != _items.end()) {
        ret = it->second.contentChecksum();
    }
    return ret;
}

bool Snapshot::setContentChecksum(const NodeId &itemId, const std::string &newChecksum) {
    const std::scoped_lock lock(_mutex);
    // Note: do not call "startUpdate" here since the computation of content checksum is asynchronous
    auto it = _items.find(itemId);
    if (it != _items.end()) {
        it->second.setContentChecksum(newChecksum);
        return true;
    }
    return false;
}

bool Snapshot::canWrite(const NodeId &itemId) {
    const std::scoped_lock lock(_mutex);
    bool ret = true;
    auto it = _items.find(itemId);
    if (it != _items.end()) {
        ret = it->second.canWrite();
    }
    return ret;
}

bool Snapshot::canShare(const NodeId &itemId) {
    const std::scoped_lock lock(_mutex);
    bool ret = true;
    auto it = _items.find(itemId);
    if (it != _items.end()) {
        ret = it->second.canShare();
    }
    return ret;
}

bool Snapshot::clearContentChecksum(const NodeId &itemId) {
    const std::scoped_lock lock(_mutex);
    return setContentChecksum(itemId, "");
}

bool Snapshot::exists(const NodeId &itemId) {
    const std::scoped_lock lock(_mutex);
    auto it = _items.find(itemId);
    if (it != _items.end() && !isOrphan(itemId)) {
        return true;
    }
    return false;
}

bool Snapshot::pathExists(const SyncPath &path) {
    const std::scoped_lock lock(_mutex);
    return !itemId(path).empty();
}

bool Snapshot::isLink(const NodeId &itemId) {
    const std::scoped_lock lock(_mutex);
    bool ret = false;
    auto it = _items.find(itemId);
    if (it != _items.end()) {
        ret = it->second.isLink();
    }
    return ret;
}

bool Snapshot::getChildrenIds(const NodeId &itemId, std::unordered_set<NodeId> &childrenIds) {
    const std::scoped_lock lock(_mutex);
    auto it = _items.find(itemId);
    if (it != _items.end()) {
        childrenIds = it->second.childrenIds();
        return true;
    }
    return false;
}

void Snapshot::ids(std::unordered_set<NodeId> &ids) {
    const std::scoped_lock lock(_mutex);
    ids.clear();
    for (const auto &it : _items) {
        ids.insert(it.first);
    }
}

bool Snapshot::isAncestor(const NodeId &itemId, const NodeId &ancestorItemId) {
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

bool Snapshot::isOrphan(const NodeId &itemId) {
    if (itemId == _rootFolderId) {
        return false;
    }

    NodeId nextParentId = parentId(itemId);
    while (nextParentId != _rootFolderId) {
        nextParentId = parentId(nextParentId);
        if (nextParentId.empty()) {
            return true;
        }
    }
    return false;
}

bool Snapshot::isEmpty() {
    const std::scoped_lock lock(_mutex);
    return _items.empty();
}

uint64_t Snapshot::nbItems() {
    const std::scoped_lock lock(_mutex);
    return _items.size();
}

bool Snapshot::isValid() {
    const std::scoped_lock lock(_mutex);
    return _isValid;
}

void Snapshot::setValid(bool newIsValid) {
    const std::scoped_lock lock(_mutex);
    _isValid = newIsValid;
}

void Snapshot::removeChildrenRecursively(const NodeId &parentId) {
    auto parentIt = _items.find(parentId);
    if (parentIt == _items.end()) {
        return;
    }

    for (const NodeId &childId : parentIt->second.childrenIds()) {
        removeChildrenRecursively(childId);
        _items.erase(childId);
    }
}

}  // namespace KDC
