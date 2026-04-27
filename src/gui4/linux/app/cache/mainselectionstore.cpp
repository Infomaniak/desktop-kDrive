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

#include "mainselectionstore.h"

namespace KDC {

MainSelectionStore::MainSelectionStore(AppCache &cache, QObject *const parent) :
    QObject(parent),
    _cache(cache) {
    (void) connect(&_cache, &AppCache::usersChanged, this, &MainSelectionStore::ensureValidSelection);
    (void) connect(&_cache, &AppCache::accountsChanged, this, &MainSelectionStore::ensureValidSelection);
    (void) connect(&_cache, &AppCache::drivesChanged, this, &MainSelectionStore::ensureValidSelection);
    (void) connect(&_cache, &AppCache::syncsChanged, this, &MainSelectionStore::ensureValidSelection);
}

qint64 MainSelectionStore::currentSyncDbId() const {
    return static_cast<qint64>(_currentSyncDbId);
}

std::optional<SyncContext> MainSelectionStore::currentSyncContext() const {
    if (_currentSyncDbId == 0) {
        return std::nullopt;
    }
    return _cache.syncContext(_currentSyncDbId);
}

std::vector<SyncContext> MainSelectionStore::syncContexts() const {
    return _cache.syncContexts();
}

void MainSelectionStore::selectSync(const qint64 syncDbId) {
    const auto typedSyncDbId = static_cast<SyncDbId>(syncDbId);
    _lastRequestedSyncDbId = typedSyncDbId;
    if (typedSyncDbId != 0 && !_cache.syncContext(typedSyncDbId)) {
        ensureValidSelection();
        return;
    }
    setCurrentSyncDbId(typedSyncDbId);
}

void MainSelectionStore::clearSelection() {
    _lastRequestedSyncDbId = 0;
    setCurrentSyncDbId(0);
}

void MainSelectionStore::ensureValidSelection() {
    if (_currentSyncDbId != 0 && _cache.syncContext(_currentSyncDbId)) {
        return;
    }

    if (_lastRequestedSyncDbId != 0 && _cache.syncContext(_lastRequestedSyncDbId)) {
        setCurrentSyncDbId(_lastRequestedSyncDbId);
        return;
    }

    setCurrentSyncDbId(firstAvailableSyncDbId());
}

void MainSelectionStore::setCurrentSyncDbId(const SyncDbId syncDbId) {
    if (_currentSyncDbId == syncDbId) {
        return;
    }
    _currentSyncDbId = syncDbId;
    emit currentSyncDbIdChanged();
    emit currentSyncContextChanged();
}

SyncDbId MainSelectionStore::firstAvailableSyncDbId() const {
    const auto contexts = _cache.syncContexts();
    if (contexts.empty()) {
        return 0;
    }
    return contexts.front().sync.dbId();
}

} // namespace KDC
