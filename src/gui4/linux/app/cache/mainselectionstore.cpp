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

#include <algorithm>

Q_LOGGING_CATEGORY(lcMainSelectionStore, "gui.v4.mainselectionstore", QtInfoMsg)

namespace KDC {

MainSelectionStore::MainSelectionStore(AppCache &cache, QObject *const parent) :
    QObject(parent),
    _cache(cache) {
    (void) connect(&_cache, &AppCache::syncsChanged, this, &MainSelectionStore::handleCacheGraphChanged);
    (void) connect(&_cache, &AppCache::usersChanged, this, &MainSelectionStore::handleCacheGraphChanged);
    (void) connect(&_cache, &AppCache::accountsChanged, this, &MainSelectionStore::handleCacheGraphChanged);
    (void) connect(&_cache, &AppCache::drivesChanged, this, &MainSelectionStore::handleCacheGraphChanged);
    (void) connect(&_cache, &AppCache::syncErrorsChanged, this, &MainSelectionStore::handleCacheGraphChanged);
    (void) connect(&_cache, &AppCache::syncRuntimeInfoChanged, this, [this](const SyncDbId syncDbId) {
        if (syncDbId == _currentSyncDbId) {
            emit currentSyncRuntimeInfoChanged();
        }
    });
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

std::optional<SyncRuntimeInfo> MainSelectionStore::currentSyncRuntimeInfo() const {
    if (_currentSyncDbId == 0) {
        return std::nullopt;
    }
    return _cache.syncRuntimeInfo(_currentSyncDbId);
}

void MainSelectionStore::selectSync(const qint64 syncDbId) {
    const auto typedSyncDbId = static_cast<SyncDbId>(syncDbId);
    _lastRequestedSyncDbId = typedSyncDbId;
    if (const auto context = _cache.syncContext(typedSyncDbId); !context.has_value()) {
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
    if (_currentSyncDbId != 0 && _cache.syncContext(_currentSyncDbId).has_value()) {
        return;
    }

    if (_lastRequestedSyncDbId != 0) {
        const auto requestedSyncContext = _cache.syncContext(_lastRequestedSyncDbId);
        if (requestedSyncContext.has_value()) {
            setCurrentSyncDbId(_lastRequestedSyncDbId);
            return;
        }
    }

    SyncDbId fallbackSyncDbId = firstClassicSyncDbId();
    if (fallbackSyncDbId == 0) {
        fallbackSyncDbId = firstAvailableSyncDbId();
    }
    setCurrentSyncDbId(fallbackSyncDbId);
}

void MainSelectionStore::handleCacheGraphChanged() {
    const auto previousSyncContext = currentSyncContext();
    const auto previousSyncDbId = _currentSyncDbId;
    ensureValidSelection();
    if (_currentSyncDbId == previousSyncDbId && previousSyncContext != currentSyncContext()) {
        emit currentContextChanged();
    }
}

void MainSelectionStore::setCurrentSyncDbId(const SyncDbId syncDbId) {
    if (_currentSyncDbId == syncDbId) {
        return;
    }

    qCDebug(lcMainSelectionStore) << "Current synchronization changed | syncDbId:" << _currentSyncDbId << "=>" << syncDbId;
    _currentSyncDbId = syncDbId;
    emit currentSyncDbIdChanged();
    emit currentSyncRuntimeInfoChanged();
    emit currentContextChanged();
}

SyncDbId MainSelectionStore::firstClassicSyncDbId() const {
    const auto contexts = _cache.syncContexts();
    const auto classicContext =
            std::ranges::find_if(contexts, [](const SyncContext &context) { return context.syncInfo.targetNodeId().empty(); });
    return classicContext == contexts.end() ? 0 : classicContext->syncInfo.dbId();
}

SyncDbId MainSelectionStore::firstAvailableSyncDbId() const {
    const auto contexts = _cache.syncContexts();
    if (contexts.empty()) {
        return 0;
    }
    return contexts.front().syncInfo.dbId();
}

} // namespace KDC
