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

#include "storagecontroller.h"

#include <QtConcurrentRun>

#include <QLocale>

#include <algorithm>
#include <limits>
Q_LOGGING_CATEGORY(lcStorageController, "gui.v4.storagecontroller", QtInfoMsg)

namespace KDC {

StorageController::StorageController(MainSelectionStore &selectionStore, QObject *const parent) :
    QObject(parent),
    _selectionStore(selectionStore) {
    (void) connect(&_selectionStore, &MainSelectionStore::currentSyncDbIdChanged, this,
                   &StorageController::refreshSelectedContext);
    (void) connect(&_selectionStore, &MainSelectionStore::currentContextChanged, this,
                   &StorageController::refreshSelectedContext);
    (void) connect(&_selectionStore, &MainSelectionStore::currentSyncStatusChanged, this,
                   &StorageController::handleSyncStatusChanged);
    (void) connect(&_scanWatcher, &QFutureWatcher<StorageScanResult>::finished, this, &StorageController::handleScanFinished);
    refreshSelectedContext();
}

StorageController::~StorageController() {
    cancelScan();
}

QString StorageController::volumeName() const {
    return _currentSnapshot ? _currentSnapshot->volumeName : QString{};
}

QString StorageController::usageText() const {
    if (!_currentSnapshot) {
        return {};
    }
    return qtTrId("storageUsageLabel").arg(formatBytes(usedBytes()), formatBytes(_currentSnapshot->totalBytes));
}

QString StorageController::syncSizeText() const {
    return _currentSnapshot ? formatBytes(_currentSnapshot->syncBytes) : QString{};
}

QString StorageController::otherSizeText() const {
    return _currentSnapshot ? formatBytes(otherBytes()) : QString{};
}

QString StorageController::availableSizeText() const {
    return _currentSnapshot ? formatBytes(_currentSnapshot->availableBytes) : QString{};
}

double StorageController::syncRatio() const {
    return _currentSnapshot ? ratio(_currentSnapshot->syncBytes) : 0.0;
}

double StorageController::otherRatio() const {
    return _currentSnapshot ? ratio(otherBytes()) : 0.0;
}

double StorageController::availableRatio() const {
    return _currentSnapshot ? ratio(_currentSnapshot->availableBytes) : 0.0;
}

void StorageController::setViewActive(const bool active) {
    if (_viewActive == active) {
        return;
    }

    _viewActive = active;
    if (!_viewActive) {
        cancelScan();
        return;
    }

    refreshSelectedContext();
    if (const auto status = currentStatus(); status && isActiveStatus(*status)) {
        (void) _dirtySyncs.insert(_selectedSyncDbId);
    }
    startScan();
}

void StorageController::retry() {
    if (!_viewActive || _selectedSyncDbId == 0) {
        return;
    }
    startScan();
}

void StorageController::refreshSelectedContext() {
    const auto context = _selectionStore.currentSyncContext();
    const SyncDbId nextSyncDbId = context ? context->syncInfo.dbId() : 0;
    const SyncPath nextSyncRoot = context ? context->syncInfo.localPath().lexically_normal() : SyncPath{};
    if (nextSyncDbId == _selectedSyncDbId && nextSyncRoot == _selectedSyncRoot) {
        return;
    }

    cancelScan();
    if (_selectedSyncDbId != 0 && nextSyncDbId == _selectedSyncDbId && nextSyncRoot != _selectedSyncRoot) {
        (void) _cache.erase(_selectedSyncDbId);
    }

    _selectedSyncDbId = nextSyncDbId;
    _selectedSyncRoot = nextSyncRoot;
    _observedStatus = currentStatus();
    presentSelectedCache();
    if (_viewActive && _selectedSyncDbId != 0) {
        if (_observedStatus && isActiveStatus(*_observedStatus)) {
            (void) _dirtySyncs.insert(_selectedSyncDbId);
        }
        startScan();
    }
}

void StorageController::handleSyncStatusChanged() {
    const auto nextStatus = currentStatus();
    if (!nextStatus) {
        _observedStatus.reset();
        return;
    }

    const bool nextStatusIsActive = isActiveStatus(*nextStatus);
    if (nextStatusIsActive) {
        (void) _dirtySyncs.insert(_selectedSyncDbId);
    }

    const bool leftActiveStatus = _observedStatus && isActiveStatus(*_observedStatus) && !nextStatusIsActive;
    _observedStatus = nextStatus;
    if (leftActiveStatus && _viewActive && _dirtySyncs.erase(_selectedSyncDbId) > 0) {
        qCInfo(lcStorageController) << "Refreshing Storage after synchronization left active state | syncDbId:"
                                    << _selectedSyncDbId << "| status:" << QString::fromStdString(toString(*nextStatus));
        startScan();
    }
}

void StorageController::presentSelectedCache() {
    _currentSnapshot.reset();

    if (_selectedSyncDbId != 0) {
        if (const auto cached = _cache.find(_selectedSyncDbId);
            cached != _cache.end() && cached->second.syncRoot == _selectedSyncRoot) {
            presentSnapshot(cached->second.snapshot);
            return;
        }
    }

    setState(State::Loading);
    emit storageChanged();
}

void StorageController::presentSnapshot(const StorageSnapshot &snapshot) {
    _currentSnapshot = snapshot;
    setState(State::Ready);
    emit storageChanged();
}

void StorageController::startScan() {
    if (!_viewActive || _selectedSyncDbId == 0 || _selectedSyncRoot.empty()) {
        return;
    }

    cancelScan();

    const SyncDbId requestedSyncDbId = _selectedSyncDbId;
    const SyncPath requestedSyncRoot = _selectedSyncRoot;
    _scanCancellation = std::make_shared<std::atomic_bool>(false);
    const auto cancellation = _scanCancellation;
    qCInfo(lcStorageController) << "Starting local Storage scan | syncDbId:" << requestedSyncDbId
                                << "| root:" << Path2QStr(requestedSyncRoot);
    (void) _scanWatcher.setProperty("syncDbId", QVariant::fromValue<qint64>(requestedSyncDbId));
    (void) _scanWatcher.setProperty("syncRoot", Path2QStr(requestedSyncRoot));
    _scanWatcher.setFuture(QtConcurrent::run([requestedSyncRoot, cancellation] {
        return StorageScanner::scan(requestedSyncRoot, [cancellation] { return cancellation->load(std::memory_order_relaxed); });
    }));
}

void StorageController::cancelScan() {
    if (_scanCancellation) {
        _scanCancellation->store(true, std::memory_order_relaxed);
        _scanCancellation.reset();
    }
}

void StorageController::handleScanFinished() {
    const auto result = _scanWatcher.result();
    const auto requestedSyncDbId = static_cast<SyncDbId>(_scanWatcher.property("syncDbId").toLongLong());
    const auto requestedSyncRoot = QStr2Path(_scanWatcher.property("syncRoot").toString());
    if (!_viewActive || requestedSyncDbId != _selectedSyncDbId || requestedSyncRoot != _selectedSyncRoot ||
        result.error == StorageScanError::Cancelled) {
        return;
    }

    if (result.succeeded()) {
        _cache[requestedSyncDbId] = CachedSnapshot{.syncRoot = requestedSyncRoot, .snapshot = *result.snapshot};
        if (const auto status = currentStatus(); status && !isActiveStatus(*status)) {
            (void) _dirtySyncs.erase(requestedSyncDbId);
        }
        _scanCancellation.reset();
        presentSnapshot(*result.snapshot);
        qCInfo(lcStorageController) << "Local Storage scan completed | syncDbId:" << requestedSyncDbId;
        return;
    }

    (void) _cache.erase(requestedSyncDbId);
    _currentSnapshot.reset();
    _scanCancellation.reset();
    setState(State::Unavailable);
    emit storageChanged();
    qCWarning(lcStorageController) << "Local Storage scan failed | syncDbId:" << requestedSyncDbId
                                   << "| error:" << toString(result.error);
}

void StorageController::setState(const State state) {
    _state = state;
}

std::optional<SyncStatus> StorageController::currentStatus() const {
    const auto runtimeInfo = _selectionStore.currentSyncRuntimeInfo();
    return runtimeInfo ? std::optional{runtimeInfo->status} : std::nullopt;
}

uint64_t StorageController::usedBytes() const {
    return _currentSnapshot ? _currentSnapshot->totalBytes - _currentSnapshot->availableBytes : 0;
}

uint64_t StorageController::otherBytes() const {
    return _currentSnapshot ? usedBytes() - std::min(_currentSnapshot->syncBytes, usedBytes()) : 0;
}

bool StorageController::isActiveStatus(const SyncStatus status) {
    return status == SyncStatus::Starting || status == SyncStatus::Running || status == SyncStatus::PauseAsked ||
           status == SyncStatus::StopAsked;
}

QString StorageController::formatBytes(const uint64_t bytes) {
    return QLocale{}.formattedDataSize(static_cast<qint64>(std::min<uint64_t>(bytes, std::numeric_limits<qint64>::max())), 1,
                                       QLocale::DataSizeSIFormat);
}

double StorageController::ratio(const uint64_t bytes) const {
    if (!_currentSnapshot || _currentSnapshot->totalBytes == 0) {
        return 0.0;
    }
    return std::clamp(static_cast<double>(bytes) / static_cast<double>(_currentSnapshot->totalBytes), 0.0, 1.0);
}

} // namespace KDC
