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
#include <QLoggingCategory>
#include <QObject>
#include <QString>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>

Q_DECLARE_LOGGING_CATEGORY(lcStorageController)

namespace KDC {

/**
 * QML-facing state and lifecycle owner for the Linux Storage page.
 *
 * Storage data is computed locally and cached per synchronization for the process lifetime. Scans run outside the GUI
 * thread, keep the last resolved presentation visible, are canceled when the page is hidden or the selection changes,
 * and are refreshed once an active synchronization leaves an active or transitional state.
 */
class StorageController final : public QObject {
        Q_OBJECT
        Q_PROPERTY(State state READ state NOTIFY storageChanged)
        Q_PROPERTY(QString volumeName READ volumeName NOTIFY storageChanged)
        Q_PROPERTY(QString usageText READ usageText NOTIFY storageChanged)
        Q_PROPERTY(QString syncSizeText READ syncSizeText NOTIFY storageChanged)
        Q_PROPERTY(QString otherSizeText READ otherSizeText NOTIFY storageChanged)
        Q_PROPERTY(QString availableSizeText READ availableSizeText NOTIFY storageChanged)
        Q_PROPERTY(double syncRatio READ syncRatio NOTIFY storageChanged)
        Q_PROPERTY(double otherRatio READ otherRatio NOTIFY storageChanged)
        Q_PROPERTY(double availableRatio READ availableRatio NOTIFY storageChanged)

    public:
        enum class State : uint8_t {
            Loading = 0,
            Ready,
            Unavailable,
        };
        Q_ENUM(State)

        explicit StorageController(MainSelectionStore &selectionStore, QObject *parent = nullptr);
        ~StorageController() override;

        [[nodiscard]] State state() const { return _state; }
        [[nodiscard]] QString volumeName() const;
        [[nodiscard]] QString usageText() const;
        [[nodiscard]] QString syncSizeText() const;
        [[nodiscard]] QString otherSizeText() const;
        [[nodiscard]] QString availableSizeText() const;
        [[nodiscard]] double syncRatio() const;
        [[nodiscard]] double otherRatio() const;
        [[nodiscard]] double availableRatio() const;
        Q_INVOKABLE void setViewActive(bool active);
        Q_INVOKABLE void retry();

    signals:
        void storageChanged();

    private:
        struct CachedSnapshot {
                SyncPath syncRoot;
                StorageSnapshot snapshot;
        };

        void refreshSelectedContext();
        void handleSyncStatusChanged();
        void presentSelectedCache();
        void presentSnapshot(const StorageSnapshot &snapshot);
        void startScan();
        void cancelScan();
        void handleScanFinished();
        void setState(State state);
        [[nodiscard]] std::optional<SyncStatus> currentStatus() const;
        [[nodiscard]] uint64_t usedBytes() const;
        [[nodiscard]] uint64_t otherBytes() const;
        [[nodiscard]] static bool isActiveStatus(SyncStatus status);
        [[nodiscard]] static QString formatBytes(uint64_t bytes);
        [[nodiscard]] double ratio(uint64_t bytes) const;

        MainSelectionStore &_selectionStore;
        QFutureWatcher<StorageScanResult> _scanWatcher;
        std::shared_ptr<std::atomic_bool> _scanCancellation;
        std::unordered_map<SyncDbId, CachedSnapshot> _cache;
        std::unordered_set<SyncDbId> _dirtySyncs;
        std::optional<StorageSnapshot> _currentSnapshot;
        std::optional<SyncStatus> _observedStatus;
        SyncDbId _selectedSyncDbId{0};
        SyncPath _selectedSyncRoot;
        State _state{State::Loading};
        bool _viewActive{false};
};

} // namespace KDC
