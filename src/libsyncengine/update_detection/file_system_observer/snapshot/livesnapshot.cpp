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

#include "livesnapshot.h"
#include "libcommonserver/log/log.h"
#include "requests/parameterscache.h"

#include <filesystem>
#include <queue>
#include <vector>
#include <clocale>

#include <log4cplus/loggingmacros.h>

namespace KDC {

LiveSnapshot::LiveSnapshot(const ReplicaSide side, const DbNode &dbNode) :
    Snapshot(side, side == ReplicaSide::Local ? dbNode.nodeIdLocal().value() : dbNode.nodeIdRemote().value()) {
    _revisionHandlder = std::make_shared<SnapshotRevisionHandler>();
    _items.try_emplace(rootFolderId(), std::make_shared<SnapshotItem>(rootFolderId()))
            .first->second->setSnapshotRevisionHandler(_revisionHandlder);
}

void LiveSnapshot::init() {
    const std::scoped_lock lock(_mutex);
    startUpdate();

    _items.clear();
    auto [res, _] = _items.try_emplace(rootFolderId(), std::make_shared<SnapshotItem>(rootFolderId()));
    auto newItemPtr = res->second;
    newItemPtr->setSnapshotRevisionHandler(_revisionHandlder);

    _isValid = false;
}

bool LiveSnapshot::updateItem(const SnapshotItem &newItem) {
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
                           L"Item: " << Utility::formatSyncName(newItem.name()) << L" (" << CommonUtility::s2ws(newItem.id())
                                     << L") already exists in parent: " << CommonUtility::s2ws(newItem.parentId())
                                     << L" with a different id. Removing it and adding the new one.");
                auto child2 = child; // removeItem cannot be called on a const ref, we need to make a copy.
                if (!removeItem(child2)) return false;
                break; // There should be at most one item with the same normalized name in a folder.
            }
        }
    }

    bool parentChanged = false;
    auto item = findItem(newItem.id());
    // Update old parent's children lists if the item already exists
    if (item) {
        parentChanged = item->id() != rootFolderId() && item->parentId() != newItem.parentId();
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
            LOG_DEBUG(Log::instance()->getLogger(),
                      "Parent " << newItem.parentId().c_str() << " does not exist yet, creating it");
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
        LOGW_DEBUG(Log::instance()->getLogger(), L"Item: " << Utility::formatSyncName(item->name()) << L" ("
                                                           << CommonUtility::s2ws(item->id()) << L") updated at:"
                                                           << item->lastModified());
    }
    return true;
}

bool LiveSnapshot::removeItem(const NodeId itemId) {
    if (auto item = findItem(itemId); item) {
        return removeItem(item);
    }
    return true; // Nothing to delete
}

bool LiveSnapshot::removeItem(std::shared_ptr<SnapshotItem> &item) {
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
        LOG_DEBUG(Log::instance()->getLogger(), "Item " << itemId << " removed from " << side() << " snapshot.");
    }

    return true;
}

bool LiveSnapshot::path(const NodeId &itemId, SyncPath &path, bool &ignore) const noexcept {
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
                ancestors.emplace_back(item->id(), item->name());
                id = item->parentId();
                parentIsRoot = id == rootFolderId();
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

bool LiveSnapshot::setName(const NodeId &itemId, const SyncName &newName) {
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

bool LiveSnapshot::setCreatedAt(const NodeId &itemId, const SyncTime newTime) {
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

bool LiveSnapshot::setLastModified(const NodeId &itemId, const SyncTime newTime) {
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

bool LiveSnapshot::setContentChecksum(const NodeId &itemId, const std::string &newChecksum) {
    const std::scoped_lock lock(_mutex);
    // Note: do not call "startUpdate" here since the computation of content checksum is asynchronous
    if (const auto item = findItem(itemId); item) {
        item->setContentChecksum(newChecksum);
        return true;
    }
    return false;
}

bool LiveSnapshot::clearContentChecksum(const NodeId &itemId) {
    const std::scoped_lock lock(_mutex);
    return setContentChecksum(itemId, "");
}


bool LiveSnapshot::isValid() const {
    const std::scoped_lock lock(_mutex);
    return _isValid;
}

void LiveSnapshot::setValid(const bool newIsValid) {
    const std::scoped_lock lock(_mutex);
    _isValid = newIsValid;
}

SnapshotRevision LiveSnapshot::revision() const {
    return _revisionHandlder->revision();
}

void LiveSnapshot::removeChildrenRecursively(const std::shared_ptr<SnapshotItem> &parent) {
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
