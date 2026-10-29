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

#include "app/appconstants.h"
#include "app/cache/appcache.h"

#include <QDesktopServices>
#include <QLocale>
#include <QLoggingCategory>
#include <QPromise>
#include <QUrl>
#include <QtConcurrentRun>

#include <algorithm>
#include <limits>
#include <unordered_set>
#include <utility>

using namespace Qt::StringLiterals;

namespace KDC {

namespace {

Q_LOGGING_CATEGORY(lcStorageController, "gui.v4.storagecontroller", QtInfoMsg)

constexpr int32_t minimumRetryIndicatorDurationMs = 300;

} // namespace

StorageController::StorageController(AppCache &appCache, MainSelectionStore &selectionStore, QObject *const parent) :
    QObject(parent),
    _appCache(appCache),
    _selectionStore(selectionStore) {
    (void) connect(&_selectionStore, &MainSelectionStore::currentSyncDbIdChanged, this,
                   &StorageController::refreshSelectedContext);
    (void) connect(&_selectionStore, &MainSelectionStore::currentContextChanged, this, [this] {
        refreshSelectedContext();
        emit quotaChanged();
        refreshStorageWarning();
    });
    (void) connect(&_appCache, &AppCache::syncStatusChanged, this, &StorageController::handleSyncStatusChanged);
    (void) connect(&_appCache, &AppCache::syncsChanged, this, &StorageController::handleSyncsChanged);
    (void) connect(&_scanWatcher, &QFutureWatcher<StorageScanUpdate>::resultReadyAt, this,
                   &StorageController::processScanUpdates);
    (void) connect(&_scanWatcher, &QFutureWatcher<StorageScanUpdate>::finished, this, &StorageController::handleScanFinished);
    (void) connect(&_volumeWatcher, &QFutureWatcher<StorageVolumeResult>::finished, this,
                   &StorageController::handleVolumePollFinished);

    _minimumRetryTimer.setInterval(minimumRetryIndicatorDurationMs);
    _minimumRetryTimer.setSingleShot(true);
    (void) connect(&_minimumRetryTimer, &QTimer::timeout, this, [this] {
        if (_retryCompletionPending) {
            _retryCompletionPending = false;
            processScanUpdates();
            handleScanFinished();
        }
    });

    _volumePollTimer.setInterval(AppConstants::Storage::freeSpacePollIntervalMs);
    (void) connect(&_volumePollTimer, &QTimer::timeout, this, &StorageController::pollVolume);

    refreshSelectedContext();
}

StorageController::~StorageController() {
    cancelScan();
}

QString StorageController::volumeName() const {
    return _volume ? _volume->volumeName : QString{};
}

bool StorageController::usesDefaultVolumeName() const {
    return _volume && _volume->usesDefaultRootName;
}

QString StorageController::usedSizeText() const {
    return _volume ? formatBytes(_volume->usedBytes()) : QString{};
}

QString StorageController::totalSizeText() const {
    return _volume ? formatBytes(_volume->totalBytes) : QString{};
}

QString StorageController::availableSizeText() const {
    return _volume ? formatBytes(_volume->availableBytes) : QString{};
}

QString StorageController::syncSizeText() const {
    return _syncBytes.has_value() ? formatBytes(*_syncBytes) : QString{};
}

QString StorageController::otherSyncsSizeText() const {
    return _otherSyncsBytes.has_value() ? formatBytes(*_otherSyncsBytes) : QString{};
}

QString StorageController::otherSizeText() const {
    return _volume ? formatBytes(otherBytes()) : QString{};
}

double StorageController::syncRatio() const {
    return _syncBytes.has_value() ? ratio(*_syncBytes) : 0.0;
}

double StorageController::otherSyncsRatio() const {
    return _otherSyncsBytes.has_value() ? ratio(*_otherSyncsBytes) : 0.0;
}

double StorageController::otherRatio() const {
    return ratio(otherBytes());
}

double StorageController::availableRatio() const {
    return _volume ? ratio(_volume->availableBytes) : 0.0;
}

bool StorageController::diskAlmostFull() const {
    return _freeBytes.has_value() && *_freeBytes < AppConstants::Storage::diskAlmostFullThresholdBytes;
}

bool StorageController::quotaAvailable() const {
    const auto drive = currentDrive();
    return drive && drive->size() > 0;
}

bool StorageController::quotaFull() const {
    const auto drive = currentDrive();
    return drive && drive->size() > 0 && drive->usedSize() >= drive->size();
}

QString StorageController::quotaUsedSizeText() const {
    const auto drive = currentDrive();
    if (!drive || drive->size() <= 0) {
        return {};
    }

    return formatBytes(static_cast<uint64_t>(std::max<int64_t>(drive->usedSize(), 0)));
}

QString StorageController::quotaTotalSizeText() const {
    const auto drive = currentDrive();
    if (!drive || drive->size() <= 0) {
        return {};
    }

    return formatBytes(static_cast<uint64_t>(drive->size()));
}

QString StorageController::quotaFreeSizeText() const {
    const auto drive = currentDrive();
    if (!drive || drive->size() <= 0) {
        return {};
    }

    return formatBytes(static_cast<uint64_t>(std::max<int64_t>(drive->size() - drive->usedSize(), 0)));
}

double StorageController::quotaUsedRatio() const {
    const auto drive = currentDrive();
    if (!drive || drive->size() <= 0) {
        return 0.0;
    }

    return std::clamp(static_cast<double>(drive->usedSize()) / static_cast<double>(drive->size()), 0.0, 1.0);
}

bool StorageController::canUpgradeSubscription() const {
    const auto drive = currentDrive();
    return drive && drive->admin();
}

QString StorageController::driveName() const {
    const auto drive = currentDrive();
    return drive ? QString::fromStdString(drive->name()) : QString{};
}

QString StorageController::subscriptionName() const {
    const auto drive = currentDrive();
    if (!drive || drive->packInfo().displayName().empty()) {
        return {};
    }

    const QString displayName = QString::fromStdString(drive->packInfo().displayName());
    return AppConstants::Drive::isStandaloneDrivePack(drive->packInfo().name()) ? u"kDrive %1"_s.arg(displayName) : displayName;
}

bool StorageController::storageWarning() const {
    return _storageWarning;
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
    for (const auto &sync: _appCache.syncs()) {
        if (isSyncActive(sync.dbId())) {
            _measureCache.markDirty(sync.dbId());
        }
    }
    startScan();
}

void StorageController::retry() {
    if (!_viewActive || _selectedSyncDbId == 0 || _retrying || _scanWatcher.isRunning()) {
        return;
    }

    startScan(ScanTrigger::Retry);
}

void StorageController::openDiskManagement() const {
    const SyncPath root = _volume ? _volume->volumeRoot : _selectedSyncRoot;
    if (root.empty()) {
        return;
    }

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(Path2QStr(root)))) {
        qCWarning(lcStorageController) << "Unable to open the Storage volume root | path:" << Path2QStr(root);
    }
}

void StorageController::openSubscriptionUpgrade() const {
    const auto drive = currentDrive();
    if (!drive || !drive->admin()) {
        return;
    }

    if (!QDesktopServices::openUrl(AppConstants::Drive::renewalUri(*drive))) {
        qCWarning(lcStorageController) << "Unable to open the subscription upgrade page | driveId:" << drive->driveId();
    }
}

void StorageController::manageSynchronization() {
    const auto context = _selectionStore.currentSyncContext();
    if (!context) {
        return;
    }

    const DriveDbId driveDbId = context->drive.dbId();
    const SyncDbId syncDbId = context->syncInfo.dbId();
    const auto mainSync = _appCache.mainSync(driveDbId);
    const bool advancedSync = !mainSync || mainSync->dbId() != syncDbId;
    emit manageSynchronizationRequested(driveDbId, syncDbId, advancedSync);
}

void StorageController::retranslate() {
    emit storageChanged();
    emit quotaChanged();
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
        _measureCache.erase(_selectedSyncDbId);
    }

    _selectedSyncDbId = nextSyncDbId;
    _selectedSyncRoot = nextSyncRoot;
    _freeBytes.reset();
    presentSelectedCache();

    if (_selectedSyncDbId == 0) {
        _volumePollTimer.stop();
    } else {
        _volumePollTimer.start();
        pollVolume();
    }

    if (_viewActive && _selectedSyncDbId != 0) {
        if (isSyncActive(_selectedSyncDbId)) {
            _measureCache.markDirty(_selectedSyncDbId);
        }
        startScan();
    }
}

/**
 * Marks a synchronization as needing a new measure while it is active, and refreshes the page once it leaves an active
 * or transitional state.
 */
void StorageController::handleSyncStatusChanged(const SyncDbId syncDbId) {
    const auto runtimeInfo = _appCache.syncRuntimeInfo(syncDbId);
    if (!runtimeInfo) {
        _measureCache.forgetStatus(syncDbId);
        return;
    }

    const bool leftActiveStatus = _measureCache.observeStatus(syncDbId, runtimeInfo->status);

    if (syncDbId == _selectedSyncDbId) {
        pollVolume();
    }

    if (leftActiveStatus && _viewActive && _measureCache.isDirty(syncDbId)) {
        qCInfo(lcStorageController) << "Refreshing Storage after synchronization left active state | syncDbId:" << syncDbId
                                    << "| status:" << QString::fromStdString(toString(runtimeInfo->status));
        startScan();
    }
}

/** Drops the measures of removed synchronizations and measures the new ones while the page is visible. */
void StorageController::handleSyncsChanged() {
    std::unordered_set<SyncDbId> existingSyncDbIds;
    for (const auto &sync: _appCache.syncs()) {
        (void) existingSyncDbIds.insert(sync.dbId());
    }

    _measureCache.retain(existingSyncDbIds);

    if (!_viewActive || _scanWatcher.isRunning() || _selectedSyncDbId == 0) {
        return;
    }

    const auto targets = otherSyncTargets();
    if (std::ranges::any_of(targets, [this](const StorageScanTarget &target) {
            return !_measureCache.isKnown(target.syncDbId, target.syncRoot);
        })) {
        startScan();
    }
}

void StorageController::presentSelectedCache() {
    _volume.reset();
    _syncBytes.reset();
    _otherSyncsBytes.reset();

    if (_selectedSyncDbId != 0) {
        if (const auto *const cached = _measureCache.find(_selectedSyncDbId, _selectedSyncRoot);
            cached != nullptr && cached->volume.has_value()) {
            _volume = cached->volume;
            _syncBytes = cached->bytes;
            _otherSyncsBytes = _measureCache.bytesOnDevice(otherSyncTargets(), _volume->device);
            _freeBytes = _volume->availableBytes;
        }
    }

    setState(_volume ? State::Ready : State::Loading);
    publish(false);
}

void StorageController::startScan(const ScanTrigger trigger) {
    if (!_viewActive || _selectedSyncDbId == 0 || _selectedSyncRoot.empty()) {
        return;
    }

    cancelScan();
    if (trigger == ScanTrigger::Retry) {
        setRetrying(true);
        _minimumRetryTimer.start();
    }

    const StorageScanTarget selected{
            .syncDbId = _selectedSyncDbId,
            .syncRoot = _selectedSyncRoot,
            .needsMeasure = trigger == ScanTrigger::Retry || _measureCache.needsMeasure(_selectedSyncDbId, _selectedSyncRoot)};
    const auto others = otherSyncTargets();
    const auto measuredOtherCount =
            std::ranges::count_if(others, [](const StorageScanTarget &target) { return target.needsMeasure; });

    _pendingOtherSyncsBytes = 0;
    _processedScanResults = 0;
    qCInfo(lcStorageController) << "Starting local Storage scan | syncDbId:" << selected.syncDbId
                                << "| root:" << Path2QStr(selected.syncRoot)
                                << "| trigger:" << (trigger == ScanTrigger::Retry ? "retry" : "automatic")
                                << "| measure selected:" << selected.needsMeasure
                                << "| other syncs to measure:" << measuredOtherCount << "/" << others.size();

    _scanWatcher.setFuture(QtConcurrent::run(
            [selected, others](QPromise<StorageScanUpdate> &promise) { StorageScanner::scanSyncs(promise, selected, others); }));
}

void StorageController::cancelScan() {
    _minimumRetryTimer.stop();
    _retryCompletionPending = false;
    setRetrying(false);
    if (_scanWatcher.isRunning()) {
        _scanWatcher.cancel();
    }
}

/**
 * Applies the scan steps published so far, in order. While a retry indicator has not been visible for its minimum
 * duration, the steps are held back so a fast retry does not flicker.
 */
void StorageController::processScanUpdates() {
    if (_scanWatcher.isCanceled()) {
        return;
    }
    if (_retrying && _minimumRetryTimer.isActive()) {
        _retryCompletionPending = true;
        return;
    }

    const auto future = _scanWatcher.future();
    while (_processedScanResults < future.resultCount()) {
        const auto update = future.resultAt(_processedScanResults);
        ++_processedScanResults;
        applyScanUpdate(update);
    }
}

void StorageController::applyScanUpdate(const StorageScanUpdate &update) {
    using Kind = StorageScanUpdate::Kind;

    switch (update.kind) {
        case Kind::Volume:
            setRetrying(false);
            applyVolume(*update.volume);
            break;
        case Kind::SelectedSyncMeasured:
            _measureCache.recordSelectedMeasure(update.syncDbId, update.syncRoot, _volume, *update.bytes,
                                                isSyncActive(update.syncDbId));
            _syncBytes = update.bytes;
            publish(true);
            break;
        case Kind::OtherSyncMeasured:
            if (const auto bytes = _measureCache.recordOtherSync(update, isSyncActive(update.syncDbId)); bytes.has_value()) {
                _pendingOtherSyncsBytes += *bytes;
            }
            break;
        case Kind::OtherSyncsCompleted:
            _otherSyncsBytes = _pendingOtherSyncsBytes;
            // The volume of the selected synchronization is cached with it once the whole page is known.
            _measureCache.setVolume(_selectedSyncDbId, _volume);
            publish(true);
            qCInfo(lcStorageController) << "Local Storage scan completed | syncDbId:" << _selectedSyncDbId;
            break;
        case Kind::Failed:
            setRetrying(false);
            failScan(update.error);
            break;
    }
}

void StorageController::handleScanFinished() {
    if (_scanWatcher.isCanceled() || _retryCompletionPending) {
        return;
    }

    processScanUpdates();
    setRetrying(false);
}

/**
 * Presents the volume read by the scan. A selected synchronization the scan does not measure again keeps its cached
 * bytes, including bytes measured while another synchronization was selected, which were cached without a volume.
 */
void StorageController::applyVolume(const StorageVolume &volume) {
    _volume = volume;
    _freeBytes = volume.availableBytes;
    if (!_syncBytes.has_value()) {
        if (const auto *const cached = _measureCache.find(_selectedSyncDbId, _selectedSyncRoot);
            cached != nullptr && cached->measured) {
            _syncBytes = cached->bytes;
        }
    }

    setState(State::Ready);
    publish(true);
}

void StorageController::failScan(const StorageScanError error) {
    _measureCache.erase(_selectedSyncDbId);
    _volume.reset();
    _syncBytes.reset();
    _otherSyncsBytes.reset();
    _freeBytes.reset();
    setState(State::Unavailable);
    publish(false);
    qCWarning(lcStorageController) << "Local Storage scan failed | syncDbId:" << _selectedSyncDbId
                                   << "| error:" << toString(error);
}

/**
 * Emits the storage change. `animatedUpdateChanged` is emitted first so the gauge knows whether to animate before its
 * widths are read again: values restored from the cache must appear at once.
 */
void StorageController::publish(const bool animated) {
    setAnimatedUpdate(animated);
    emit storageChanged();
    refreshStorageWarning();
}

/** Reads the free space of the selected volume outside the GUI thread, without logging failures. */
void StorageController::pollVolume() {
    if (_selectedSyncRoot.empty() || _volumeWatcher.isRunning()) {
        return;
    }

    _volumePollRoot = _selectedSyncRoot;
    const SyncPath root = _volumePollRoot;
    _volumeWatcher.setFuture(QtConcurrent::run([root] { return StorageScanner::readVolume(root, false); }));
}

void StorageController::handleVolumePollFinished() {
    if (_volumePollRoot != _selectedSyncRoot) {
        // The selection changed during this read: read the newly selected volume instead.
        pollVolume();
        return;
    }

    const auto result = _volumeWatcher.result();
    if (!result.volume) {
        _freeBytes.reset();
        refreshStorageWarning();
        return;
    }

    _freeBytes = result.volume->availableBytes;
    if (_volume && _volume->device == result.volume->device &&
        (_volume->totalBytes != result.volume->totalBytes || _volume->availableBytes != result.volume->availableBytes)) {
        _volume->totalBytes = result.volume->totalBytes;
        _volume->availableBytes = result.volume->availableBytes;
        publish(true);
        return;
    }

    refreshStorageWarning();
}

void StorageController::setState(const State state) {
    _state = state;
}

void StorageController::setRetrying(const bool retrying) {
    if (_retrying == retrying) {
        return;
    }

    _retrying = retrying;
    emit retryingChanged();
}

void StorageController::setAnimatedUpdate(const bool animated) {
    if (_animatedUpdate == animated) {
        return;
    }

    _animatedUpdate = animated;
    emit animatedUpdateChanged();
}

void StorageController::refreshStorageWarning() {
    const bool almostFull = diskAlmostFull();
    if (almostFull != _diskAlmostFullLogged) {
        _diskAlmostFullLogged = almostFull;
        qCInfo(lcStorageController) << "Storage disk almost full state changed | syncDbId:" << _selectedSyncDbId
                                    << "| almost full:" << almostFull;
    }

    const bool warning = almostFull || quotaFull();
    if (warning == _storageWarning) {
        return;
    }

    _storageWarning = warning;
    emit storageWarningChanged();
}

std::vector<StorageScanTarget> StorageController::otherSyncTargets() const {
    std::vector<StorageScanTarget> targets;
    for (const auto &sync: _appCache.syncs()) {
        if (sync.dbId() == _selectedSyncDbId) {
            continue;
        }

        const SyncPath root = sync.localPath().lexically_normal();
        if (root.empty()) {
            continue;
        }

        targets.push_back(
                {.syncDbId = sync.dbId(), .syncRoot = root, .needsMeasure = _measureCache.needsMeasure(sync.dbId(), root)});
    }
    return targets;
}

bool StorageController::isSyncActive(const SyncDbId syncDbId) const {
    const auto runtimeInfo = _appCache.syncRuntimeInfo(syncDbId);
    return runtimeInfo && StorageMeasureCache::isActiveStatus(runtimeInfo->status);
}

std::optional<Drive> StorageController::currentDrive() const {
    const auto context = _selectionStore.currentSyncContext();
    return context ? std::optional{context->drive} : std::nullopt;
}

/** Used space that is not attributed to a measured kDrive synchronization, so it shrinks as the measures arrive. */
uint64_t StorageController::otherBytes() const {
    if (!_volume) {
        return 0;
    }

    const uint64_t usedBytes = _volume->usedBytes();
    const uint64_t kDriveBytes = std::min(_syncBytes.value_or(0) + _otherSyncsBytes.value_or(0), usedBytes);
    return usedBytes - kDriveBytes;
}

QString StorageController::formatBytes(const uint64_t bytes) {
    return QLocale{}.formattedDataSize(static_cast<qint64>(std::min<uint64_t>(bytes, std::numeric_limits<qint64>::max())), 1,
                                       QLocale::DataSizeSIFormat);
}

double StorageController::ratio(const uint64_t bytes) const {
    if (!_volume || _volume->totalBytes == 0) {
        return 0.0;
    }

    return std::clamp(static_cast<double>(bytes) / static_cast<double>(_volume->totalBytes), 0.0, 1.0);
}

} // namespace KDC
