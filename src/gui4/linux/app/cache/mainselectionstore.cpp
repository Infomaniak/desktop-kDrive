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

Q_LOGGING_CATEGORY(lcMainSelectionStore, "gui.v4.mainselectionstore", QtInfoMsg)

namespace KDC {

MainSelectionStore::MainSelectionStore(AppCache &cache, QObject *const parent) :
    QObject(parent),
    _cache(cache) {
    (void) connect(&_cache, &AppCache::syncsChanged, this, &MainSelectionStore::handleSyncsChanged);
    (void) connect(&_cache, &AppCache::usersChanged, this, &MainSelectionStore::handleContextDataChanged);
    (void) connect(&_cache, &AppCache::accountsChanged, this, &MainSelectionStore::handleContextDataChanged);
    (void) connect(&_cache, &AppCache::drivesChanged, this, &MainSelectionStore::handleContextDataChanged);
    (void) connect(&_cache, &AppCache::syncErrorsChanged, this, &MainSelectionStore::handleContextDataChanged);
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
        qCWarning(lcMainSelectionStore) << "Requested sync not in context, falling back | syncDbId:" << typedSyncDbId;
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

void MainSelectionStore::handleSyncsChanged() {
    const auto previousContext = currentSyncContext();
    const auto previousCurrentSyncDbId = _currentSyncDbId;
    ensureValidSelection();
    if (_currentSyncDbId != 0 && _currentSyncDbId == previousCurrentSyncDbId && previousContext != currentSyncContext()) {
        emit currentSyncContextChanged();
    }
}

void MainSelectionStore::handleContextDataChanged() {
    const auto previousCurrentSyncDbId = _currentSyncDbId;
    if (_currentSyncDbId != 0) {
        ensureValidSelection();
    }
    if (_currentSyncDbId != 0 && _currentSyncDbId == previousCurrentSyncDbId) {
        emit currentSyncContextChanged();
    }
}

void MainSelectionStore::setCurrentSyncDbId(const SyncDbId syncDbId) {
    if (_currentSyncDbId == syncDbId) {
        return;
    }
    qCDebug(lcMainSelectionStore) << "Current sync changed | from:" << _currentSyncDbId << "/ to:" << syncDbId;
    _currentSyncDbId = syncDbId;
    emit currentSyncDbIdChanged();
    emit currentSyncContextChanged();
}

SyncDbId MainSelectionStore::firstAvailableSyncDbId() const {
    const auto contexts = _cache.syncContexts();
    if (contexts.empty()) {
        return 0;
    }
    return contexts.front().syncInfo.dbId();
}

} // namespace KDC
