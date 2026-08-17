/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "activitystore.h"

#include <QLoggingCategory>

#include <algorithm>
#include <ranges>
#include <utility>

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcActivityStore, "gui.v4.activitystore", QtInfoMsg)

/** @brief Returns whether an operation identifier can safely be used for deduplication. */
bool isValidOperationId(const UniqueId operationId) {
    return operationId > 0;
}

/** @brief Returns whether an activity is currently being synchronized. */
bool isInProgress(const SyncFileStatus status) {
    return status == SyncFileStatus::Syncing;
}

/** @brief Returns whether an activity ended in a state that still requires user attention. */
bool isFailed(const SyncFileStatus status) {
    switch (status) {
        case SyncFileStatus::Success:
        case SyncFileStatus::Syncing:
            return false;
        case SyncFileStatus::Unknown:
        case SyncFileStatus::Error:
        case SyncFileStatus::Conflict:
        case SyncFileStatus::Inconsistency:
        case SyncFileStatus::Ignored:
        case SyncFileStatus::EnumEnd:
            return true;
    }
    return true;
}

/** @brief Returns whether two activities identify the same node through a non-empty local or remote identifier. */
bool hasMatchingNodeId(const ActivityEntry &entry, const SyncFileItemInfo &item) {
    const bool sameLocalNode = !item.localNodeId().isEmpty() && item.localNodeId() == entry.localNodeId;
    const bool sameRemoteNode = !item.remoteNodeId().isEmpty() && item.remoteNodeId() == entry.remoteNodeId;
    return sameLocalNode || sameRemoteNode;
}

/** @brief Removes failed activities superseded by a successful or in-progress activity for the same node. */
void removeSupersededFailures(std::vector<ActivityEntry> &entries, const SyncFileItemInfo &item) {
    if (isFailed(item.status())) {
        return;
    }

    (void) std::erase_if(
            entries, [&item](const ActivityEntry &entry) { return isFailed(entry.status) && hasMatchingNodeId(entry, item); });
}
} // namespace

ActivityStore::ActivityStore(QObject *const parent) :
    QObject(parent) {}

void ActivityStore::ingest(const SyncDbId syncDbId, const SyncFileItemInfo &item) {
    if (syncDbId <= 0) {
        qCWarning(lcActivityStore) << "Activity ignored for invalid synchronization | syncDbId:" << syncDbId;
        return;
    }

    if (item.status() == SyncFileStatus::EnumEnd) {
        qCWarning(lcActivityStore) << "Activity ignored for unsupported status | syncDbId:" << syncDbId
                                   << "/ status:" << static_cast<int32_t>(item.status());
        return;
    }

    if (item.direction() == SyncDirection::Unknown || item.direction() == SyncDirection::EnumEnd) {
        qCWarning(lcActivityStore) << "Activity received with unknown source | syncDbId:" << syncDbId
                                   << "/ direction:" << static_cast<int32_t>(item.direction());
    }

    auto &entries = _activitiesBySyncDbId[syncDbId];
    if (isValidOperationId(item.operationId())) {
        if (const auto entryIt = std::ranges::find(entries, item.operationId(), &ActivityEntry::operationId);
            entryIt != entries.end()) {
            if (!isInProgress(entryIt->status)) {
                return;
            }

            *entryIt = makeEntry(syncDbId, item, entryIt->localId);
            removeSupersededFailures(entries, item);
            emit activitiesChanged(syncDbId);
            return;
        }
    }

    removeSupersededFailures(entries, item);
    auto entry = makeEntry(syncDbId, item, _nextLocalId++);
    entries.push_back(std::move(entry));
    enforceCapacity(entries);
    emit activitiesChanged(syncDbId);
}

std::vector<ActivityEntry> ActivityStore::activities(const SyncDbId syncDbId) const {
    const auto syncIt = _activitiesBySyncDbId.find(syncDbId);
    return syncIt == _activitiesBySyncDbId.end() ? std::vector<ActivityEntry>{} : syncIt->second;
}

void ActivityStore::removeInProgress(const SyncDbId syncDbId) {
    const auto syncIt = _activitiesBySyncDbId.find(syncDbId);
    if (syncIt == _activitiesBySyncDbId.end()) {
        return;
    }

    if (auto &entries = syncIt->second;
        std::erase_if(entries, [](const ActivityEntry &entry) { return isInProgress(entry.status); }) == 0) {
        return;
    }

    emit activitiesChanged(syncDbId);
}

void ActivityStore::removeSync(const SyncDbId syncDbId) {
    if (_activitiesBySyncDbId.erase(syncDbId) == 0) {
        return;
    }
    emit activitiesChanged(syncDbId);
}

void ActivityStore::retainSyncs(const std::unordered_set<SyncDbId> &syncDbIds) {
    std::vector<SyncDbId> removedSyncDbIds;
    for (auto syncIt = _activitiesBySyncDbId.begin(); syncIt != _activitiesBySyncDbId.end();) {
        if (syncDbIds.contains(syncIt->first)) {
            ++syncIt;
            continue;
        }

        removedSyncDbIds.push_back(syncIt->first);
        syncIt = _activitiesBySyncDbId.erase(syncIt);
    }

    for (const SyncDbId syncDbId: removedSyncDbIds) {
        emit activitiesChanged(syncDbId);
    }
}

void ActivityStore::clear() {
    std::vector<SyncDbId> removedSyncDbIds;
    removedSyncDbIds.reserve(_activitiesBySyncDbId.size());
    for (const SyncDbId syncDbId: _activitiesBySyncDbId | std::views::keys) {
        removedSyncDbIds.push_back(syncDbId);
    }
    _activitiesBySyncDbId.clear();

    for (const SyncDbId syncDbId: removedSyncDbIds) {
        emit activitiesChanged(syncDbId);
    }
}

/**
 * @brief Builds a process-local activity entry from a server DTO.
 * @param syncDbId Database identifier of the owning synchronization.
 * @param item Activity DTO received from the server.
 * @param localId Stable process-local identifier assigned to the entry.
 * @return A fully populated activity entry with a fresh receive timestamp and sequence number.
 */
ActivityEntry ActivityStore::makeEntry(const SyncDbId syncDbId, const SyncFileItemInfo &item, const GenericId localId) {
    ActivityEntry entry;
    entry.localId = localId;
    entry.syncDbId = syncDbId;
    entry.operationId = item.operationId();
    entry.nodeType = item.type();
    entry.path = QStr2Path(item.path());
    entry.newPath = QStr2Path(item.newPath());
    entry.localNodeId = item.localNodeId();
    entry.remoteNodeId = item.remoteNodeId();
    entry.direction = item.direction();
    entry.instruction = item.instruction();
    entry.status = item.status();
    entry.size = item.size();
    entry.progress = item.progress();
    entry.receivedAtUtc = QDateTime::currentDateTimeUtc();
    entry.receivedSequence = _nextReceivedSequence++;
    return entry;
}

/**
 * @brief Trims a synchronization's activity collection to ActivityStore::maxActivitiesPerSync entries.
 *
 * While the collection exceeds its limit, the activity with the oldest ActivityEntry::receivedSequence is removed,
 * regardless of its status. The entry inserted immediately before this call has the newest sequence and is therefore
 * always retained. This guarantees that a new error remains visible even when every retained activity is in progress.
 *
 * @param entries Mutable activity collection to trim.
 */
void ActivityStore::enforceCapacity(std::vector<ActivityEntry> &entries) {
    while (entries.size() > ActivityStore::maxActivitiesPerSync) {
        const auto oldestIt = std::ranges::min_element(entries, {}, &ActivityEntry::receivedSequence);
        (void) entries.erase(oldestIt);
    }
}

} // namespace KDC
