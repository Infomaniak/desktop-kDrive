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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "storagemeasurecache.h"

#include <algorithm>

namespace KDC {

bool StorageMeasureCache::isActiveStatus(const SyncStatus status) {
    return status == SyncStatus::Starting || status == SyncStatus::Running || status == SyncStatus::PauseAsked ||
           status == SyncStatus::StopAsked;
}

const StorageMeasureCache::Entry *StorageMeasureCache::find(const SyncDbId syncDbId, const SyncPath &syncRoot) const {
    const auto entry = _entries.find(syncDbId);
    if (entry == _entries.end() || entry->second.syncRoot != syncRoot) {
        return nullptr;
    }

    return &entry->second;
}

bool StorageMeasureCache::isKnown(const SyncDbId syncDbId, const SyncPath &syncRoot) const {
    return find(syncDbId, syncRoot) != nullptr;
}

bool StorageMeasureCache::needsMeasure(const SyncDbId syncDbId, const SyncPath &syncRoot) const {
    const Entry *const entry = find(syncDbId, syncRoot);
    return entry == nullptr || !entry->measured || _dirtySyncs.contains(syncDbId);
}

std::optional<uint64_t> StorageMeasureCache::bytesOnDevice(const std::vector<StorageScanTarget> &syncs,
                                                           const dev_t device) const {
    uint64_t total = 0;
    for (const auto &sync: syncs) {
        const Entry *const entry = find(sync.syncDbId, sync.syncRoot);
        if (entry == nullptr) {
            return std::nullopt;
        }
        if (entry->device != device) {
            continue;
        }
        if (!entry->measured) {
            return std::nullopt;
        }
        total += entry->bytes;
    }
    return total;
}

void StorageMeasureCache::recordSelectedMeasure(const SyncDbId syncDbId, const SyncPath &syncRoot,
                                                const std::optional<StorageVolume> &volume, const uint64_t bytes,
                                                const bool syncActive) {
    _entries[syncDbId] = Entry{.syncRoot = syncRoot,
                               .device = volume.has_value() ? volume->device : 0,
                               .bytes = bytes,
                               .measured = true,
                               .volume = volume};
    clearDirtyUnlessActive(syncDbId, syncActive);
}

std::optional<uint64_t> StorageMeasureCache::recordOtherSync(const StorageScanUpdate &update, const bool syncActive) {
    if (!update.device.has_value()) {
        return std::nullopt;
    }

    // The device is remembered even for a synchronization stored on another volume, so it is not looked up as unknown
    // again; its bytes are measured only once the selected volume holds it.
    auto &entry = _entries[update.syncDbId];
    if (entry.syncRoot != update.syncRoot) {
        entry = Entry{.syncRoot = update.syncRoot};
    }
    entry.device = *update.device;
    if (update.bytes.has_value()) {
        entry.bytes = *update.bytes;
        entry.measured = true;
        clearDirtyUnlessActive(update.syncDbId, syncActive);
    }

    if (update.onVolume && entry.measured) {
        return entry.bytes;
    }
    return std::nullopt;
}

void StorageMeasureCache::setVolume(const SyncDbId syncDbId, const std::optional<StorageVolume> &volume) {
    if (const auto entry = _entries.find(syncDbId); entry != _entries.end()) {
        entry->second.volume = volume;
    }
}

void StorageMeasureCache::erase(const SyncDbId syncDbId) {
    (void) _entries.erase(syncDbId);
}

void StorageMeasureCache::retain(const std::unordered_set<SyncDbId> &syncDbIds) {
    (void) std::erase_if(_entries, [&syncDbIds](const auto &entry) { return !syncDbIds.contains(entry.first); });
    (void) std::erase_if(_dirtySyncs, [&syncDbIds](const SyncDbId syncDbId) { return !syncDbIds.contains(syncDbId); });
    (void) std::erase_if(_observedStatuses, [&syncDbIds](const auto &entry) { return !syncDbIds.contains(entry.first); });
}

void StorageMeasureCache::markDirty(const SyncDbId syncDbId) {
    (void) _dirtySyncs.insert(syncDbId);
}

bool StorageMeasureCache::isDirty(const SyncDbId syncDbId) const {
    return _dirtySyncs.contains(syncDbId);
}

bool StorageMeasureCache::observeStatus(const SyncDbId syncDbId, const SyncStatus status) {
    const bool nextStatusIsActive = isActiveStatus(status);
    if (nextStatusIsActive) {
        markDirty(syncDbId);
    }

    const auto previous = _observedStatuses.find(syncDbId);
    const bool leftActiveStatus = previous != _observedStatuses.end() && isActiveStatus(previous->second) && !nextStatusIsActive;
    _observedStatuses[syncDbId] = status;
    return leftActiveStatus;
}

void StorageMeasureCache::forgetStatus(const SyncDbId syncDbId) {
    (void) _observedStatuses.erase(syncDbId);
}

/** A measure taken while its synchronization is still active is outdated at once, so it stays dirty. */
void StorageMeasureCache::clearDirtyUnlessActive(const SyncDbId syncDbId, const bool syncActive) {
    if (!syncActive) {
        (void) _dirtySyncs.erase(syncDbId);
    }
}

} // namespace KDC
