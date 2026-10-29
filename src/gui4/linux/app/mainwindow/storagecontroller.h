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

#pragma once

#include "app/cache/mainselectionstore.h"
#include "storagescanner.h"

#include <QFutureWatcher>
#include <QObject>
#include <QPromise>
#include <QString>
#include <QTimer>

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace KDC {

class AppCache;

/** One step of a Storage scan, published as soon as it is known so the page fills progressively. */
struct StorageScanUpdate {
        enum class Kind : uint8_t {
            Volume = 0,
            SelectedSyncMeasured,
            OtherSyncMeasured,
            OtherSyncsCompleted,
            Failed,
        };

        Kind kind{Kind::Volume};
        std::optional<StorageVolume> volume{};
        SyncDbId syncDbId{0};
        SyncPath syncRoot{};
        // Empty for a cached synchronization or for one that is not on the scanned volume.
        std::optional<uint64_t> bytes{};
        bool onVolume{false};
        // Device of another synchronization root; empty when it could not be inspected or read.
        std::optional<dev_t> device{};
        StorageScanError error{StorageScanError::None};
};

/**
 * QML-facing state and lifecycle owner for the Linux Storage page.
 *
 * The page shows the local volume of the selected synchronization, split between this synchronization, the other
 * synchronizations stored on the same volume, other files, and free space, plus the online quota of its drive.
 *
 * Each synchronization root is measured outside the GUI thread and cached for the process lifetime; a cached measure is
 * reused until its synchronization has been active again. A scan publishes the volume first, then the selected
 * synchronization, then the other synchronizations, so the gauge fills progressively. Scans run only while the page is
 * visible and are canceled when it is hidden or the selection changes. The free space of the volume is also polled
 * without any scan while a synchronization is selected, so the sidebar warning stays current outside the page.
 */
class StorageController final : public QObject {
        Q_OBJECT
        Q_PROPERTY(State state READ state NOTIFY storageChanged)
        Q_PROPERTY(bool retrying READ retrying NOTIFY retryingChanged)
        Q_PROPERTY(bool animatedUpdate READ animatedUpdate NOTIFY animatedUpdateChanged)
        Q_PROPERTY(QString volumeName READ volumeName NOTIFY storageChanged)
        Q_PROPERTY(bool usesDefaultVolumeName READ usesDefaultVolumeName NOTIFY storageChanged)
        Q_PROPERTY(QString usedSizeText READ usedSizeText NOTIFY storageChanged)
        Q_PROPERTY(QString totalSizeText READ totalSizeText NOTIFY storageChanged)
        Q_PROPERTY(QString availableSizeText READ availableSizeText NOTIFY storageChanged)
        Q_PROPERTY(bool syncMeasured READ syncMeasured NOTIFY storageChanged)
        Q_PROPERTY(QString syncSizeText READ syncSizeText NOTIFY storageChanged)
        Q_PROPERTY(bool otherSyncsMeasured READ otherSyncsMeasured NOTIFY storageChanged)
        Q_PROPERTY(QString otherSyncsSizeText READ otherSyncsSizeText NOTIFY storageChanged)
        Q_PROPERTY(bool otherSyncsEmpty READ otherSyncsEmpty NOTIFY storageChanged)
        Q_PROPERTY(QString otherSizeText READ otherSizeText NOTIFY storageChanged)
        Q_PROPERTY(double syncRatio READ syncRatio NOTIFY storageChanged)
        Q_PROPERTY(double otherSyncsRatio READ otherSyncsRatio NOTIFY storageChanged)
        Q_PROPERTY(double otherRatio READ otherRatio NOTIFY storageChanged)
        Q_PROPERTY(double availableRatio READ availableRatio NOTIFY storageChanged)
        Q_PROPERTY(bool diskAlmostFull READ diskAlmostFull NOTIFY storageChanged)
        Q_PROPERTY(bool quotaAvailable READ quotaAvailable NOTIFY quotaChanged)
        Q_PROPERTY(bool quotaFull READ quotaFull NOTIFY quotaChanged)
        Q_PROPERTY(QString quotaUsedSizeText READ quotaUsedSizeText NOTIFY quotaChanged)
        Q_PROPERTY(QString quotaTotalSizeText READ quotaTotalSizeText NOTIFY quotaChanged)
        Q_PROPERTY(QString quotaFreeSizeText READ quotaFreeSizeText NOTIFY quotaChanged)
        Q_PROPERTY(double quotaUsedRatio READ quotaUsedRatio NOTIFY quotaChanged)
        Q_PROPERTY(bool canUpgradeSubscription READ canUpgradeSubscription NOTIFY quotaChanged)
        Q_PROPERTY(QString driveName READ driveName NOTIFY quotaChanged)
        Q_PROPERTY(QString subscriptionName READ subscriptionName NOTIFY quotaChanged)
        Q_PROPERTY(bool storageWarning READ storageWarning NOTIFY storageWarningChanged)

    public:
        enum class State : uint8_t {
            Loading = 0,
            Ready,
            Unavailable,
        };
        Q_ENUM(State)

        StorageController(AppCache &appCache, MainSelectionStore &selectionStore, QObject *parent = nullptr);
        ~StorageController() override;

        [[nodiscard]] State state() const { return _state; }
        [[nodiscard]] bool retrying() const { return _retrying; }
        [[nodiscard]] bool animatedUpdate() const { return _animatedUpdate; }
        [[nodiscard]] QString volumeName() const;
        [[nodiscard]] bool usesDefaultVolumeName() const;
        [[nodiscard]] QString usedSizeText() const;
        [[nodiscard]] QString totalSizeText() const;
        [[nodiscard]] QString availableSizeText() const;
        [[nodiscard]] bool syncMeasured() const { return _syncBytes.has_value(); }
        [[nodiscard]] QString syncSizeText() const;
        [[nodiscard]] bool otherSyncsMeasured() const { return _otherSyncsBytes.has_value(); }
        [[nodiscard]] QString otherSyncsSizeText() const;
        /** True once measured, when no other synchronization occupies space on the volume. */
        [[nodiscard]] bool otherSyncsEmpty() const { return _otherSyncsBytes.has_value() && *_otherSyncsBytes == 0; }
        [[nodiscard]] QString otherSizeText() const;
        [[nodiscard]] double syncRatio() const;
        [[nodiscard]] double otherSyncsRatio() const;
        [[nodiscard]] double otherRatio() const;
        [[nodiscard]] double availableRatio() const;
        [[nodiscard]] bool diskAlmostFull() const;
        [[nodiscard]] bool quotaAvailable() const;
        [[nodiscard]] bool quotaFull() const;
        [[nodiscard]] QString quotaUsedSizeText() const;
        [[nodiscard]] QString quotaTotalSizeText() const;
        [[nodiscard]] QString quotaFreeSizeText() const;
        [[nodiscard]] double quotaUsedRatio() const;
        [[nodiscard]] bool canUpgradeSubscription() const;
        [[nodiscard]] QString driveName() const;
        /** Offer name of the drive, e.g. "kDrive Team" or "kSuite Business"; empty while the pack is unknown. */
        [[nodiscard]] QString subscriptionName() const;
        [[nodiscard]] bool storageWarning() const;

        Q_INVOKABLE void setViewActive(bool active);
        Q_INVOKABLE void retry();
        Q_INVOKABLE void openDiskManagement() const;
        Q_INVOKABLE void openSubscriptionUpgrade() const;
        /** Asks Settings to open the "Manage synchronization" page of the selected synchronization. */
        Q_INVOKABLE void manageSynchronization();
        /** Notifies views that locale-formatted sizes must be read again. */
        void retranslate();

    signals:
        void storageChanged();
        void quotaChanged();
        void storageWarningChanged();
        void retryingChanged();
        void animatedUpdateChanged();
        void manageSynchronizationRequested(qint64 driveDbId, qint64 syncDbId, bool advancedSync);

    private:
        enum class ScanTrigger : uint8_t {
            Automatic = 0,
            Retry,
        };

        struct CachedMeasure {
                SyncPath syncRoot{};
                dev_t device{0};
                uint64_t bytes{0};
                // False for a synchronization stored on another volume, whose bytes have never been needed.
                bool measured{false};
                // Volume read with the measure of a selected synchronization, shown at once when it is selected again.
                std::optional<StorageVolume> volume{};
        };

        struct ScanTarget {
                SyncDbId syncDbId{0};
                SyncPath syncRoot{};
                bool needsMeasure{true};
        };

        void refreshSelectedContext();
        void handleSyncStatusChanged(SyncDbId syncDbId);
        void handleSyncsChanged();
        void presentSelectedCache();
        void startScan(ScanTrigger trigger = ScanTrigger::Automatic);
        void cancelScan();
        void processScanUpdates();
        void applyScanUpdate(const StorageScanUpdate &update);
        void handleScanFinished();
        void applyVolume(const StorageVolume &volume);
        void failScan(StorageScanError error);
        void publish(bool animated);
        void pollVolume();
        void handleVolumePollFinished();
        void setState(State state);
        void setRetrying(bool retrying);
        void setAnimatedUpdate(bool animated);
        void refreshStorageWarning();
        [[nodiscard]] std::vector<ScanTarget> otherSyncTargets() const;
        [[nodiscard]] bool needsMeasure(SyncDbId syncDbId, const SyncPath &syncRoot) const;
        [[nodiscard]] bool isKnown(SyncDbId syncDbId, const SyncPath &syncRoot) const;
        [[nodiscard]] std::optional<uint64_t> cachedOtherSyncsBytes(dev_t device) const;
        [[nodiscard]] bool isSyncActive(SyncDbId syncDbId) const;
        [[nodiscard]] std::optional<Drive> currentDrive() const;
        [[nodiscard]] uint64_t otherBytes() const;
        static void scan(QPromise<StorageScanUpdate> &promise, const ScanTarget &selected, const std::vector<ScanTarget> &others);
        [[nodiscard]] static std::optional<StorageScanUpdate> scanOtherSync(const ScanTarget &other, dev_t volumeDevice,
                                                                            const StorageScanner::CancellationCheck &isCanceled);
        [[nodiscard]] static bool isActiveStatus(SyncStatus status);
        [[nodiscard]] static QString formatBytes(uint64_t bytes);
        [[nodiscard]] double ratio(uint64_t bytes) const;

        AppCache &_appCache;
        MainSelectionStore &_selectionStore;
        QFutureWatcher<StorageScanUpdate> _scanWatcher;
        QFutureWatcher<StorageVolumeResult> _volumeWatcher;
        QTimer _minimumRetryTimer;
        QTimer _volumePollTimer;
        std::unordered_map<SyncDbId, CachedMeasure> _measures;
        std::unordered_set<SyncDbId> _dirtySyncs;
        std::unordered_map<SyncDbId, SyncStatus> _observedStatuses;
        std::optional<StorageVolume> _volume;
        std::optional<uint64_t> _syncBytes;
        std::optional<uint64_t> _otherSyncsBytes;
        // Other synchronizations of the running scan found on the volume, with their measured or cached bytes.
        uint64_t _pendingOtherSyncsBytes{0};
        SyncDbId _selectedSyncDbId{0};
        SyncPath _selectedSyncRoot;
        // Last free space read by a scan or a poll; empty when the volume could not be read.
        std::optional<uint64_t> _freeBytes;
        SyncPath _volumePollRoot;
        int32_t _processedScanResults{0};
        State _state{State::Loading};
        bool _viewActive{false};
        bool _retrying{false};
        bool _retryCompletionPending{false};
        bool _animatedUpdate{false};
        bool _storageWarning{false};
        bool _diskAlmostFullLogged{false};
};

} // namespace KDC
