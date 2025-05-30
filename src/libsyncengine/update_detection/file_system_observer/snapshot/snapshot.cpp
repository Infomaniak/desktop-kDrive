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

Snapshot::Snapshot(ReplicaSide side, const DbNode &dbNode) :
    _side(side),
    _rootFolderId(side == ReplicaSide::Local ? dbNode.nodeIdLocal().value() : dbNode.nodeIdRemote().value()) {
    _revisionHandlder = std::make_shared<SnapshotRevisionHandler>();
    _items.try_emplace(_rootFolderId, std::make_shared<SnapshotItem>(_rootFolderId))
            .first->second->setSnapshotRevisionHandler(_revisionHandlder);
}

Snapshot::~Snapshot() {
    _items.clear();
}

Snapshot &Snapshot::operator=(const Snapshot &other) {
    if (this != &other) {
        const std::scoped_lock lock(_mutex, other._mutex);

        assert(_side == other._side);
        assert(_rootFolderId == other._rootFolderId);
        _items.clear();
        *_revisionHandlder = *other._revisionHandlder;

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
                item->addChild(_items.at(childId)); // Add the new pointer
            }
        }

        _isValid = other._isValid;
        _copy = true;
    }

    return *this;
}

void Snapshot::init() {
    const std::scoped_lock lock(_mutex);
    startUpdate();

    _items.clear();
    (void) _items.try_emplace(_rootFolderId, std::make_shared<SnapshotItem>(_rootFolderId))
            .first->second->setSnapshotRevisionHandler(_revisionHandlder);

    _isValid = false;
}

bool Snapshot::updateItem(const SnapshotItem &newItem) {
    const std::scoped_lock lock(_mutex);

    if (newItem.parentId().empty()) {
        LOG_WARN(Log::instance()->getLogger(), "Parent ID is empty for item " << newItem.id());
        assert(false);
        return false;
    }

    if (newItem.parentId() == newItem.id()) {
        LOG_WARN(Log::instance()->getLogger(), "Parent ID equals item ID " << newItem.id());
        assert(false);
        return false;
    }

    // Check if `newItem` already exists with the same path but a different Id
    if (const auto newParent = findItem(newItem.parentId()); newParent) {
        for (const auto &child: newParent->children()) {
            if (child->normalizedName() == newItem.normalizedName() && child->id() != newItem.id()) {
                LOGW_DEBUG(Log::instance()->getLogger(),
                           L"Item: " << SyncName2WStr(newItem.name()) << L" (" << Utility::s2ws(newItem.id())
                                     << L") already exists in parent: " << Utility::s2ws(newItem.parentId())
                                     << L" with a different id. Removing it and adding the new one.");
                auto child2 = child; // removeItem cannot be called on a const ref, we need to make a copy.
                removeItem(child2);
                break; // There should be (at most) only one item with the same name in a folder
            }
        }
    }

    bool parentChanged = false;
    auto item = findItem(newItem.id());
    // Update old parent's children lists if the item already exists
    if (item) {
        parentChanged = item->id() != _rootFolderId && item->parentId() != newItem.parentId();
        // Remove children from previous parent
        if (parentChanged) {
            if (const auto previousParent = findItem(item->parentId()); previousParent) {
                previousParent->removeChild(item);
            }
        }

    } else {
        // Item does not exist yet, create it
        parentChanged = true;
        item = std::make_shared<SnapshotItem>(newItem.id());
        item->setSnapshotRevisionHandler(_revisionHandlder);
        (void) _items.try_emplace(newItem.id(), item);
    }

    // Update item
    item->copyExceptChildren(newItem);

    if (parentChanged) {
        // Add children to new parent
        if (auto newParent = findItem(newItem.parentId()); !newParent) {
            // New parent not found, create it
            LOG_DEBUG(Log::instance()->getLogger(), "Parent " << newItem.parentId() << " does not exist yet, creating it");
            newParent = std::make_shared<SnapshotItem>(newItem.parentId());
            newParent->setSnapshotRevisionHandler(_revisionHandlder);
            (void) _items.try_emplace(newItem.parentId(), newParent);
            newParent->addChild(item);
        } else {
            newParent->addChild(item);
        }
    }
    if (parentChanged || !isOrphan(item->id())) {
        startUpdate();
    }

    if (ParametersCache::isExtendedLogEnabled()) {
        LOGW_DEBUG(Log::instance()->getLogger(), L"Item: " << SyncName2WStr(item->name()) << L" (" << Utility::s2ws(item->id())
                                                           << L") updated at:" << item->lastModified());
    }
    return true;
}

bool Snapshot::removeItem(const NodeId itemId) {
    if (auto item = findItem(itemId); item) {
        return removeItem(item);
    }
    return true; // Nothing to delete
}

bool Snapshot::removeItem(std::shared_ptr<SnapshotItem> &item) {
    const std::scoped_lock lock(_mutex);

    if (!item) {
        assert(false);
        return false;
    }

    if (!isOrphan(item->id())) {
        startUpdate();
    }

    // First remove all children
    removeChildrenRecursively(item);

    // Remove it also from its parent's children
    if (const auto parentItem = findItem(item->parentId()); parentItem) {
        parentItem->removeChild(item);
    }
    const NodeId itemId = item->id();
    item.reset();
    _items.erase(itemId);

    if (ParametersCache::isExtendedLogEnabled()) {
        LOG_DEBUG(Log::instance()->getLogger(), "Item " << itemId << " removed from " << _side << " snapshot.");
    }

    return true;
}

NodeId Snapshot::itemId(const SyncPath &path) const {
    const std::scoped_lock lock(_mutex);

    NodeId ret;
    auto item = _items.at(_rootFolderId);
    LOG_IF_FAIL(Log::instance()->getLogger(), item);

    for (auto pathIt = path.begin(); pathIt != path.end(); pathIt++) {
#ifndef _WIN32
        if (pathIt->lexically_normal() == SyncPath(Str("/")).lexically_normal()) {
            continue;
        }
#endif // _WIN32

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
                if (_copy) {
                    if (!item->path().empty()) {
                        path = item->path();
                        break;
                    }
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
        if (_copy) {
            const auto item = findItem(ancestors.back().first);
            assert(item);
            item->setPath(path);
        }
        ancestors.pop_back();
    }

    // Trick to ignore items with a pattern like "X:" in their names on Windows.
    // Since only relative path are stored in the snapshot, the root name should always be empty.
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

bool Snapshot::setName(const NodeId &itemId, const SyncName &newName) {
    const std::scoped_lock lock(_mutex);
    if (const auto item = findItem(itemId); item) {
        item->setName(newName);
        if (!isOrphan(itemId)) {
            startUpdate();
        }
        return true;
    }
    return false;
}

SyncTime Snapshot::createdAt(const NodeId &itemId) const {
    const std::scoped_lock lock(_mutex);
    if (const auto item = findItem(itemId); item) {
        return item->createdAt();
    }
    return 0;
}

bool Snapshot::setCreatedAt(const NodeId &itemId, SyncTime newTime) {
    const std::scoped_lock lock(_mutex);
    if (const auto item = findItem(itemId); item) {
        item->setCreatedAt(newTime);
        if (!isOrphan(itemId)) {
            startUpdate();
        }
        return true;
    }
    return false;
}

SyncTime Snapshot::lastModified(const NodeId &itemId) const {
    const std::scoped_lock lock(_mutex);
    if (const auto item = findItem(itemId); item) {
        return item->lastModified();
    }
    return 0;
}

bool Snapshot::setLastModified(const NodeId &itemId, SyncTime newTime) {
    const std::scoped_lock lock(_mutex);
    if (const auto it = _items.find(itemId); it != _items.end()) {
        it->second->setLastModified(newTime);

        if (!isOrphan(itemId)) {
            startUpdate();
        }
        return true;
    }
    return false;
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

bool Snapshot::setContentChecksum(const NodeId &itemId, const std::string &newChecksum) {
    const std::scoped_lock lock(_mutex);
    // Note: do not call "startUpdate" here since the computation of content checksum is asynchronous
    if (const auto item = findItem(itemId); item) {
        item->setContentChecksum(newChecksum);
        return true;
    }
    return false;
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

bool Snapshot::clearContentChecksum(const NodeId &itemId) {
    const std::scoped_lock lock(_mutex);
    return setContentChecksum(itemId, "");
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

bool Snapshot::isValid() const {
    const std::scoped_lock lock(_mutex);
    return _isValid;
}

void Snapshot::setValid(bool newIsValid) {
    const std::scoped_lock lock(_mutex);
    _isValid = newIsValid;
}

SnapshotRevision Snapshot::revision() const {
    return _revisionHandlder->revision();
}

bool Snapshot::checkIntegrityRecursively() const {
    return checkIntegrityRecursively(_items.at(rootFolderId()));
}

bool Snapshot::checkIntegrityRecursively(const std::shared_ptr<SnapshotItem> &parentItem) const {
    // Check that we do not have the same file twice in the same folder
    std::set<SyncName> names;
    for (const auto &child: parentItem->children()) {
        if (!checkIntegrityRecursively(child)) {
            return false;
        }

        const auto result = names.insert(child->name());
        if (!result.second) {
            LOGW_WARN(Log::instance()->getLogger(),
                      L"Snapshot integrity check failed, the folder named: \""
                              << SyncName2WStr(parentItem->name()) << L"\"(" << Utility::s2ws(parentItem->id())
                              << L") contains: \"" << SyncName2WStr(child->name()) << L"\" twice with two different NodeIds");
            return false;
        }
    }
    return true;
}

void Snapshot::removeChildrenRecursively(const std::shared_ptr<SnapshotItem> &parent) {
    auto it = parent->children().begin();
    while (it != parent->children().end()) {
        const auto &child = *it;
        const NodeId childId = child->id();

        removeChildrenRecursively(child);
        ++it;
        parent->removeChild(child);
        _items.erase(childId);
    }
}

} // namespace KDC
