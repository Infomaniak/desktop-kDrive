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
#include "snapshotitem.h"
#include "libcommonserver/log/log.h"
#include "requests/parameterscache.h"

#include <filesystem>
#include <queue>
#include <vector>
#include <clocale>

#include <log4cplus/loggingmacros.h>

namespace KDC {

Snapshot::Snapshot(ReplicaSide side, const DbNode &dbNode) :
    _side(side), _rootFolderId(side == ReplicaSide::Local ? dbNode.nodeIdLocal().value() : dbNode.nodeIdRemote().value()) {
    _items.insert({_rootFolderId, SnapshotItem(_rootFolderId)});
}

Snapshot::~Snapshot() {
    _items.clear();
}

Snapshot &Snapshot::operator=(Snapshot &other) {
    if (this != &other) {
        const std::scoped_lock lock(_mutex, other._mutex);

        assert(_side == other._side);
        assert(_rootFolderId == other._rootFolderId);

        _items = other._items;
        _isValid = other._isValid;
        _copy = true;
    }

    return *this;
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
        assert(false);
        return false;
    }

    if (newItem.parentId() == newItem.id()) {
        LOG_WARN(Log::instance()->getLogger(), "Parent ID equals item ID " << newItem.id().c_str());
        assert(false);
        return false;
    }

    // Check if `newItem` already exists with the same path but a different Id
    if (auto itNewParent = _items.find(newItem.parentId()); itNewParent != _items.end()) {
        for (const NodeId &childId: itNewParent->second.childrenIds()) {
            auto child = _items.find(childId);
            if (child == _items.end()) {
                assert(false && "Child not found in snapshot");
                LOG_WARN(Log::instance()->getLogger(), "Child " << childId.c_str() << " not found in snapshot");
                continue;
            }
            if (child->second.normalizedName() == newItem.normalizedName() && child->second.id() != newItem.id()) {
                LOGW_DEBUG(Log::instance()->getLogger(),
                           L"Item: " << SyncName2WStr(newItem.name()) << L" (" << Utility::s2ws(newItem.id())
                                     << L") already exists in parent: " << Utility::s2ws(newItem.parentId())
                                     << L" with a different id. Removing it and adding the new one.");
                removeItem(childId);
                break; // There should be (at most) only one item with the same name in a folder
            }
        }
    }

    const SnapshotItem &prevItem = _items[newItem.id()];

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
    _items[newItem.id()].copyExceptChildren(newItem);

    if (parentChanged || !isOrphan(newItem.id())) {
        startUpdate();
    }

    if (ParametersCache::isExtendedLogEnabled()) {
        LOGW_DEBUG(Log::instance()->getLogger(), L"Item: " << SyncName2WStr(newItem.name()) << L" ("
                                                           << Utility::s2ws(newItem.id()) << L") updated at:"
                                                           << newItem.lastModified());
    }

    return true;
}

bool Snapshot::removeItem(const NodeId id) {
    const std::scoped_lock lock(_mutex);

    if (id.empty()) {
        assert(false);
        return false;
    }

    auto it = _items.find(id);
    if (it == _items.end()) {
        return true; // Nothing to delete
    }

    if (!isOrphan(id)) {
        startUpdate();
    }

    // First remove all children
    removeChildrenRecursively(id);

    // Remove it also from its parent's children
    if (auto parentIt = _items.find(it->second.parentId()); parentIt != _items.end()) {
        parentIt->second.removeChildren(id);
    }

    _items.erase(id);

    if (ParametersCache::isExtendedLogEnabled()) {
        LOG_DEBUG(Log::instance()->getLogger(), "Item " << id.c_str() << " removed from " << _side << " snapshot.");
    }

    return true;
}

NodeId Snapshot::itemId(const SyncPath &path) const {
    const std::scoped_lock lock(_mutex);

    NodeId ret;
    auto itemIt = _items.find(_rootFolderId);

    for (auto pathIt = path.begin(); pathIt != path.end(); pathIt++) {
#ifndef _WIN32
        if (pathIt->lexically_normal() == SyncPath(Str("/")).lexically_normal()) {
            continue;
        }
#endif // _WIN32

        bool idFound = false;
        for (const NodeId &childId: itemIt->second.childrenIds()) {
            if (name(childId) == *pathIt) {
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

NodeId Snapshot::parentId(const NodeId &itemId) const {
    const std::scoped_lock lock(_mutex);
    NodeId ret;
    if (auto it = _items.find(itemId); it != _items.end()) {
        ret = it->second.parentId();
    }
    return ret;
}

bool Snapshot::setParentId(const NodeId &itemId, const NodeId &newParentId) {
    const std::scoped_lock lock(_mutex);
    if (auto it = _items.find(itemId); it != _items.end()) {
        it->second.setParentId(newParentId);

        if (!isOrphan(itemId)) {
            startUpdate();
        }
        return true;
    }

    return false;
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
    bool parentIsRoot = false;
    NodeId id = itemId;

    {
        const std::scoped_lock lock(_mutex);
        while (!parentIsRoot) {
            if (const auto it = _items.find(id); it != _items.end()) {
                if (_copy) {
                    if (!it->second.path().empty()) {
                        path = it->second.path();
                        break;
                    };
                }

                ancestors.push_back({it->first, it->second.name()});
                id = it->second.parentId();
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
        if (_copy) {
            const auto it = _items.find(ancestors.back().first);
            assert(it != _items.end());
            it->second.setPath(path);
        }
        ancestors.pop_back();

        // Trick to ignore items with pattern like "X:" in their name on Windows - Begin
        if (path.parent_path() != tmpParentPath) {
            ignore = true;
            return false;
        }
        tmpParentPath = path;
        // Trick to ignore items with pattern like "X:" in their name on Windows - End
    }

    return ok;
}

SyncName Snapshot::name(const NodeId &itemId) const {
    const std::scoped_lock lock(_mutex);
    SyncName ret;

    if (const auto it = _items.find(itemId); it != _items.cend()) {
        ret = it->second.name();
    }

    return ret;
}

bool Snapshot::setName(const NodeId &itemId, const SyncName &newName) {
    const std::scoped_lock lock(_mutex);
    if (auto it = _items.find(itemId); it != _items.end()) {
        it->second.setName(newName);

        if (!isOrphan(itemId)) {
            startUpdate();
        }
        return true;
    }
    return false;
}

SyncTime Snapshot::createdAt(const NodeId &itemId) const {
    const std::scoped_lock lock(_mutex);
    SyncTime ret = 0;
    if (auto it = _items.find(itemId); it != _items.end()) {
        ret = it->second.createdAt();
    }
    return ret;
}

bool Snapshot::setCreatedAt(const NodeId &itemId, SyncTime newTime) {
    const std::scoped_lock lock(_mutex);
    if (auto it = _items.find(itemId); it != _items.end()) {
        it->second.setCreatedAt(newTime);

        if (!isOrphan(itemId)) {
            startUpdate();
        }
        return true;
    }
    return false;
}

SyncTime Snapshot::lastModified(const NodeId &itemId) const {
    const std::scoped_lock lock(_mutex);
    SyncTime ret = 0;
    if (auto it = _items.find(itemId); it != _items.end()) {
        ret = it->second.lastModified();
    }
    return ret;
}

bool Snapshot::setLastModified(const NodeId &itemId, SyncTime newTime) {
    const std::scoped_lock lock(_mutex);
    if (auto it = _items.find(itemId); it != _items.end()) {
        it->second.setLastModified(newTime);

        if (!isOrphan(itemId)) {
            startUpdate();
        }
        return true;
    }
    return false;
}

NodeType Snapshot::type(const NodeId &itemId) const {
    const std::scoped_lock lock(_mutex);
    NodeType ret = NodeType::Unknown;
    if (auto it = _items.find(itemId); it != _items.end()) {
        ret = it->second.type();
    }
    return ret;
}

int64_t Snapshot::size(const NodeId &itemId) const {
    const std::scoped_lock lock(_mutex);
    int64_t ret = 0;
    if (type(itemId) == NodeType::Directory) {
        std::unordered_set<NodeId> childrenIds;
        getChildrenIds(itemId, childrenIds);
        for (auto &childId: childrenIds) {
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

std::string Snapshot::contentChecksum(const NodeId &itemId) const {
    const std::scoped_lock lock(_mutex);
    std::string ret;
    if (auto it = _items.find(itemId); it != _items.end()) {
        ret = it->second.contentChecksum();
    }
    return ret;
}

bool Snapshot::setContentChecksum(const NodeId &itemId, const std::string &newChecksum) {
    const std::scoped_lock lock(_mutex);
    // Note: do not call "startUpdate" here since the computation of content checksum is asynchronous
    if (auto it = _items.find(itemId); it != _items.end()) {
        it->second.setContentChecksum(newChecksum);
        return true;
    }
    return false;
}

bool Snapshot::canWrite(const NodeId &itemId) const {
    const std::scoped_lock lock(_mutex);
    bool ret = true;
    if (auto it = _items.find(itemId); it != _items.end()) {
        ret = it->second.canWrite();
    }
    return ret;
}

bool Snapshot::canShare(const NodeId &itemId) const {
    const std::scoped_lock lock(_mutex);
    bool ret = true;
    if (auto it = _items.find(itemId); it != _items.end()) {
        ret = it->second.canShare();
    }
    return ret;
}

bool Snapshot::clearContentChecksum(const NodeId &itemId) {
    const std::scoped_lock lock(_mutex);
    return setContentChecksum(itemId, "");
}

bool Snapshot::exists(const NodeId &itemId) const {
    const std::scoped_lock lock(_mutex);
    if (auto it = _items.find(itemId); it != _items.end() && !isOrphan(itemId)) {
        return true;
    }
    return false;
}

bool Snapshot::pathExists(const SyncPath &path) const {
    const std::scoped_lock lock(_mutex);
    return !itemId(path).empty();
}

bool Snapshot::isLink(const NodeId &itemId) const {
    const std::scoped_lock lock(_mutex);
    bool ret = false;
    if (auto it = _items.find(itemId); it != _items.end()) {
        ret = it->second.isLink();
    }
    return ret;
}

bool Snapshot::getChildrenIds(const NodeId &itemId, std::unordered_set<NodeId> &childrenIds) const {
    const std::scoped_lock lock(_mutex);
    if (auto it = _items.find(itemId); it != _items.end()) {
        childrenIds = it->second.childrenIds();
        return true;
    }
    return false;
}

void Snapshot::ids(std::unordered_set<NodeId> &ids) const {
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
            LOG_WARN(Log::instance()->getLogger(), "Parent ID equals item ID " << nextParentId.c_str());
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

bool Snapshot::isValid() const {
    const std::scoped_lock lock(_mutex);
    return _isValid;
}

void Snapshot::setValid(bool newIsValid) {
    const std::scoped_lock lock(_mutex);
    _isValid = newIsValid;
}

bool Snapshot::checkIntegrityRecursively() const {
    return checkIntegrityRecursively(rootFolderId());
}

bool Snapshot::checkIntegrityRecursively(const NodeId &parentId) const {
    // Check that we do not have the same file twice in the same folder
    const auto &parentItem = _items.at(parentId);
    std::set<SyncName> names;
    for (auto childId = parentItem.childrenIds().begin(), end = parentItem.childrenIds().end(); childId != end; childId++) {
        if (!checkIntegrityRecursively(*childId)) {
            return false;
        }

        auto result = names.insert(_items.at(*childId).name());
        if (!result.second) {
            LOGW_WARN(Log::instance()->getLogger(),
                      L"Snapshot integrity check failed, the folder named: \""
                              << SyncName2WStr(parentItem.name()) << L"\"(" << Utility::s2ws(parentItem.id()) << L") contains: \""
                              << SyncName2WStr(_items.at(*childId).name()) << L"\" twice with two different NodeIds");
            return false;
        }
    }
    return true;
}

void Snapshot::removeChildrenRecursively(const NodeId &parentId) {
    auto parentIt = _items.find(parentId);
    if (parentIt == _items.end()) {
        return;
    }

    for (const NodeId &childId: parentIt->second.childrenIds()) {
        removeChildrenRecursively(childId);
        _items.erase(childId);
    }
}

} // namespace KDC
