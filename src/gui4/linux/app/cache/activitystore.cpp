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
bool isInProgress(const ActivityStatus status) {
    return status == ActivityStatus::InProgress;
}
} // namespace

ActivityStore::ActivityStore(QObject *const parent) :
    QObject(parent) {}

void ActivityStore::ingest(const SyncDbId syncDbId, const SyncFileItemInfo &item) {
    if (syncDbId <= 0) {
        qCWarning(lcActivityStore) << "Activity ignored for invalid synchronization | syncDbId:" << syncDbId;
        return;
    }

    const auto normalizedStatus = normalizeStatus(item.status());
    if (!normalizedStatus.has_value()) {
        qCWarning(lcActivityStore) << "Activity ignored for unsupported status | syncDbId:" << syncDbId
                                   << "/ status:" << static_cast<int32_t>(item.status());
        return;
    }

    const ActivitySource source = normalizeSource(item.direction());
    if (source == ActivitySource::Unknown) {
        qCWarning(lcActivityStore) << "Activity received with unknown source | syncDbId:" << syncDbId
                                   << "/ direction:" << static_cast<int32_t>(item.direction());
    }

    auto &entries = _activitiesBySyncDbId[syncDbId];
    if (updateExistingOperation(syncDbId, entries, item, *normalizedStatus, source)) {
        return;
    }

    auto entry = makeEntry(syncDbId, item, *normalizedStatus, source, _nextLocalId++);
    const GenericId insertedLocalId = entry.localId;
    entries.push_back(std::move(entry));
    if (enforceCapacity(syncDbId, insertedLocalId, entries)) {
        emit activitiesChanged(syncDbId);
    }
}

std::vector<ActivityEntry> ActivityStore::activities(const SyncDbId syncDbId) const {
    const auto syncIt = _activitiesBySyncDbId.find(syncDbId);
    return syncIt == _activitiesBySyncDbId.end() ? std::vector<ActivityEntry>{} : syncIt->second;
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
 * @brief Maps a server file status to the reduced presentation status used by Activities.
 * @param status File status received from the server.
 * @return The corresponding presentation status, or std::nullopt when the value requires unsupported-status handling.
 */
std::optional<ActivityStatus> ActivityStore::normalizeStatus(const SyncFileStatus status) {
    switch (status) {
        case SyncFileStatus::Success:
            return ActivityStatus::Synchronized;
        case SyncFileStatus::Syncing:
            return ActivityStatus::InProgress;
        case SyncFileStatus::Error:
        case SyncFileStatus::Conflict:
        case SyncFileStatus::Inconsistency:
        case SyncFileStatus::Unknown:
        case SyncFileStatus::Ignored:
            return ActivityStatus::Failed;
        case SyncFileStatus::EnumEnd:
            return std::nullopt;
    }
    return std::nullopt;
}

/**
 * @brief Maps a server synchronization direction to its presentation source.
 * @param direction Direction received from the server.
 * @return The corresponding presentation source, or ActivitySource::Unknown when no source can be inferred.
 */
ActivitySource ActivityStore::normalizeSource(const SyncDirection direction) {
    switch (direction) {
        case SyncDirection::Up:
            return ActivitySource::Computer;
        case SyncDirection::Down:
            return ActivitySource::Web;
        case SyncDirection::Unknown:
        case SyncDirection::EnumEnd:
            return ActivitySource::Unknown;
    }
    return ActivitySource::Unknown;
}

/**
 * @brief Updates an existing operation or absorbs a late event for an operation that is no longer in progress.
 *
 * Invalid and unknown operation identifiers return false so the caller can insert a distinct activity. An in-progress
 * entry is replaced while preserving its local identifier. Entries that are no longer in progress absorb duplicate or
 * regressive events without emitting a change.
 *
 * @param syncDbId Database identifier of the owning synchronization.
 * @param entries Mutable activity collection for that synchronization.
 * @param item Latest activity DTO received for the operation.
 * @param status Normalized presentation status.
 * @param source Normalized presentation source.
 * @return true when the operation identifier was already present, whether updated or deliberately ignored; false when a
 * new entry must be inserted.
 */
bool ActivityStore::updateExistingOperation(const SyncDbId syncDbId, std::vector<ActivityEntry> &entries,
                                            const SyncFileItemInfo &item, const ActivityStatus status,
                                            const ActivitySource source) {
    if (!isValidOperationId(item.operationId())) {
        return false;
    }

    const auto entryIt = std::ranges::find(entries, item.operationId(), &ActivityEntry::operationId);
    if (entryIt == entries.end()) {
        return false;
    }
    if (!isInProgress(entryIt->status)) {
        return true;
    }

    *entryIt = makeEntry(syncDbId, item, status, source, entryIt->localId);
    emit activitiesChanged(syncDbId);
    return true;
}

/**
 * @brief Builds a process-local activity entry from a server DTO.
 * @param syncDbId Database identifier of the owning synchronization.
 * @param item Activity DTO received from the server.
 * @param status Normalized presentation status.
 * @param source Normalized presentation source.
 * @param localId Stable process-local identifier assigned to the entry.
 * @return A fully populated activity entry with a fresh receive timestamp and sequence number.
 */
ActivityEntry ActivityStore::makeEntry(const SyncDbId syncDbId, const SyncFileItemInfo &item, const ActivityStatus status,
                                       const ActivitySource source, const GenericId localId) {
    ActivityEntry entry;
    entry.localId = localId;
    entry.syncDbId = syncDbId;
    entry.operationId = item.operationId();
    entry.nodeType = item.type();
    entry.path = item.path();
    entry.newPath = item.newPath();
    entry.localNodeId = item.localNodeId();
    entry.remoteNodeId = item.remoteNodeId();
    entry.direction = item.direction();
    entry.instruction = item.instruction();
    entry.rawStatus = item.status();
    entry.status = status;
    entry.source = source;
    entry.size = item.size();
    entry.progress = item.progress();
    entry.receivedAtUtc = QDateTime::currentDateTimeUtc();
    entry.receivedSequence = _nextReceivedSequence++;
    return entry;
}

/**
 * @brief Trims a synchronization's activity collection to ActivityStore::maxActivitiesPerSync entries.
 *
 * Eviction preserves active work whenever possible. While the collection exceeds its limit, the oldest entry that is no
 * longer in progress is removed first, using ActivityEntry::receivedSequence as the stable age discriminator. If every
 * retained entry is still in progress, the oldest one is removed instead and the exceptional condition is logged.
 *
 * The newly inserted entry can itself be selected for eviction. This happens, for example, when 500 in-progress entries
 * are already retained and the new entry is the only completed candidate. Returning whether that entry survived lets
 * the caller avoid emitting activitiesChanged() when insertion followed by eviction leaves the observable collection
 * unchanged.
 *
 * @param syncDbId Database identifier used to contextualize capacity warnings.
 * @param insertedLocalId Process-local identifier of the entry inserted immediately before this call.
 * @param entries Mutable activity collection to trim.
 * @return true if the newly inserted entry remains in the collection; false if capacity enforcement evicted it.
 */
bool ActivityStore::enforceCapacity(const SyncDbId syncDbId, const GenericId insertedLocalId,
                                    std::vector<ActivityEntry> &entries) {
    bool insertedEntryRetained = true;
    while (entries.size() > ActivityStore::maxActivitiesPerSync) {
        auto oldestCompletedIt = entries.end();
        for (auto entryIt = entries.begin(); entryIt != entries.end(); ++entryIt) {
            if (isInProgress(entryIt->status)) {
                continue;
            }
            if (oldestCompletedIt == entries.end() || entryIt->receivedSequence < oldestCompletedIt->receivedSequence) {
                oldestCompletedIt = entryIt;
            }
        }

        if (oldestCompletedIt != entries.end()) {
            insertedEntryRetained = insertedEntryRetained && oldestCompletedIt->localId != insertedLocalId;
            entries.erase(oldestCompletedIt);
            continue;
        }

        const auto oldestIt = std::ranges::min_element(entries, {}, &ActivityEntry::receivedSequence);
        qCWarning(lcActivityStore) << "Activity capacity reached with only in-progress operations; evicting oldest"
                                   << "| syncDbId:" << syncDbId << "/ capacity:" << maxActivitiesPerSync;
        insertedEntryRetained = insertedEntryRetained && oldestIt->localId != insertedLocalId;
        entries.erase(oldestIt);
    }
    return insertedEntryRetained;
}

} // namespace KDC
