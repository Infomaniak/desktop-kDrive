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

qint64 MainSelectionStore::currentDriveDbId() const {
    return static_cast<qint64>(_currentDriveDbId);
}

qint64 MainSelectionStore::currentSyncDbId() const {
    return static_cast<qint64>(_currentSyncDbId);
}

std::optional<DriveContext> MainSelectionStore::currentDriveContext() const {
    if (_currentDriveDbId == 0) {
        return std::nullopt;
    }
    return _cache.driveContext(_currentDriveDbId);
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
    const auto context = _cache.syncContext(typedSyncDbId);
    if (!context.has_value()) {
        qCWarning(lcMainSelectionStore) << "Requested sync not in context, falling back | syncDbId:" << typedSyncDbId;
        ensureValidSelection();
        return;
    }
    _lastRequestedDriveDbId = context->drive.dbId();
    setCurrentSelection(context->drive.dbId(), typedSyncDbId);
}

void MainSelectionStore::selectDrive(const qint64 driveDbId) {
    const auto typedDriveDbId = static_cast<DriveDbId>(driveDbId);
    const auto context = _cache.driveContext(typedDriveDbId);
    if (!context.has_value() || !context->syncInfos.empty()) {
        qCWarning(lcMainSelectionStore) << "Requested drive-only context is unavailable, falling back | driveDbId:"
                                        << typedDriveDbId;
        ensureValidSelection();
        return;
    }
    _lastRequestedDriveDbId = typedDriveDbId;
    _lastRequestedSyncDbId = 0;
    setCurrentSelection(typedDriveDbId, 0);
}

void MainSelectionStore::clearSelection() {
    _lastRequestedDriveDbId = 0;
    _lastRequestedSyncDbId = 0;
    setCurrentSelection(0, 0);
}

void MainSelectionStore::ensureValidSelection() {
    if (_currentSyncDbId != 0) {
        const auto currentSyncContext = _cache.syncContext(_currentSyncDbId);
        if (currentSyncContext.has_value()) {
            setCurrentSelection(currentSyncContext->drive.dbId(), _currentSyncDbId);
            return;
        }

        const auto formerDriveContext = _cache.driveContext(_currentDriveDbId);
        if (formerDriveContext.has_value() && formerDriveContext->syncInfos.empty()) {
            setCurrentSelection(_currentDriveDbId, 0);
            return;
        }
    } else if (_currentDriveDbId != 0) {
        const auto currentDriveContext = _cache.driveContext(_currentDriveDbId);
        if (currentDriveContext.has_value() && currentDriveContext->syncInfos.empty()) {
            return;
        }
    }

    if (_lastRequestedSyncDbId != 0) {
        const auto requestedSyncContext = _cache.syncContext(_lastRequestedSyncDbId);
        if (requestedSyncContext.has_value()) {
            setCurrentSelection(requestedSyncContext->drive.dbId(), _lastRequestedSyncDbId);
            return;
        }
    }

    if (_lastRequestedDriveDbId != 0) {
        const auto requestedDriveContext = _cache.driveContext(_lastRequestedDriveDbId);
        if (requestedDriveContext.has_value() && requestedDriveContext->syncInfos.empty()) {
            setCurrentSelection(_lastRequestedDriveDbId, 0);
            return;
        }
    }

    SyncDbId fallbackSyncDbId = firstClassicSyncDbId();
    if (fallbackSyncDbId == 0) {
        fallbackSyncDbId = firstAvailableSyncDbId();
    }
    if (fallbackSyncDbId != 0) {
        const auto fallbackContext = _cache.syncContext(fallbackSyncDbId);
        setCurrentSelection(fallbackContext->drive.dbId(), fallbackSyncDbId);
        return;
    }

    setCurrentSelection(firstAvailableDriveDbId(), 0);
}

void MainSelectionStore::handleCacheGraphChanged() {
    const auto previousDriveContext = currentDriveContext();
    const auto previousSyncContext = currentSyncContext();
    const auto previousDriveDbId = _currentDriveDbId;
    const auto previousSyncDbId = _currentSyncDbId;
    ensureValidSelection();
    if (_currentDriveDbId == previousDriveDbId && _currentSyncDbId == previousSyncDbId &&
        (previousDriveContext != currentDriveContext() || previousSyncContext != currentSyncContext())) {
        emit currentContextChanged();
    }
}

void MainSelectionStore::setCurrentSelection(const DriveDbId driveDbId, const SyncDbId syncDbId) {
    if (_currentDriveDbId == driveDbId && _currentSyncDbId == syncDbId) {
        return;
    }

    qCDebug(lcMainSelectionStore) << "Current main context changed | driveDbId:" << _currentDriveDbId << "=>" << driveDbId
                                  << "| syncDbId:" << _currentSyncDbId << "=>" << syncDbId;
    const bool driveChanged = _currentDriveDbId != driveDbId;
    const bool syncChanged = _currentSyncDbId != syncDbId;
    _currentDriveDbId = driveDbId;
    _currentSyncDbId = syncDbId;
    if (driveChanged) {
        emit currentDriveDbIdChanged();
    }
    if (syncChanged) {
        emit currentSyncDbIdChanged();
        emit currentSyncRuntimeInfoChanged();
    }
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

DriveDbId MainSelectionStore::firstAvailableDriveDbId() const {
    const auto contexts = _cache.driveContexts();
    if (contexts.empty()) {
        return 0;
    }
    return contexts.front().drive.dbId();
}

} // namespace KDC
