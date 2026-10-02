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

#pragma once

#include "app/cache/cachetypes.h"

#include <QColor>
#include <QObject>
#include <QString>

#include <cstdint>

namespace KDC {

class AppCache;
class CommService;
class SyncService;

/**
 * Settings "kDrive management" page state for one user drive.
 *
 * Role: resolve the main synchronization of one configured drive from AppCache, load its confirmed blacklist to present a
 * custom selection, open its local folder, and delete it. The page targets a DriveDbId rather than a backend DriveId: a
 * drive row belongs to one account of one user, so another user's synchronizations of the same drive are never presented.
 */
class DriveManagementController final : public QObject {
        Q_OBJECT
        Q_PROPERTY(qint64 userDbId READ userDbId NOTIFY presentationChanged)
        Q_PROPERTY(qint64 accountId READ accountId NOTIFY presentationChanged)
        Q_PROPERTY(qint64 driveId READ driveId NOTIFY presentationChanged)
        Q_PROPERTY(QString driveName READ driveName NOTIFY presentationChanged)
        Q_PROPERTY(QColor driveColor READ driveColor NOTIFY presentationChanged)
        Q_PROPERTY(bool hasMainSync READ hasMainSync NOTIFY presentationChanged)
        Q_PROPERTY(QString localPath READ localPath NOTIFY presentationChanged)
        Q_PROPERTY(bool syncCreationPending READ syncCreationPending NOTIFY presentationChanged)
        Q_PROPERTY(bool selectionLoading READ selectionLoading NOTIFY presentationChanged)
        Q_PROPERTY(bool selectionLoadFailed READ selectionLoadFailed NOTIFY presentationChanged)
        Q_PROPERTY(bool customSelection READ customSelection NOTIFY presentationChanged)
        Q_PROPERTY(bool deletePending READ deletePending NOTIFY presentationChanged)

    public:
        DriveManagementController(AppCache &appCache, CommService &commService, SyncService &syncService,
                                  QObject *parent = nullptr);

        // Identity of the drive for the activation editor, which works on available drives.
        [[nodiscard]] qint64 userDbId() const { return _availableDriveKey.userDbId; }
        [[nodiscard]] qint64 accountId() const { return _availableDriveKey.accountId; }
        [[nodiscard]] qint64 driveId() const { return _availableDriveKey.driveId; }
        [[nodiscard]] QString driveName() const { return _driveName; }
        [[nodiscard]] QColor driveColor() const { return _driveColor; }
        [[nodiscard]] bool hasMainSync() const { return _mainSyncDbId != 0; }
        [[nodiscard]] QString localPath() const { return _localPath; }
        [[nodiscard]] bool syncCreationPending() const { return _syncCreationPending; }
        [[nodiscard]] bool selectionLoading() const { return _selectionState == SelectionState::Loading; }
        [[nodiscard]] bool selectionLoadFailed() const { return _selectionState == SelectionState::Failed; }
        // Reports a confirmed non-empty blacklist only; it stays unchanged while a reload is in flight.
        [[nodiscard]] bool customSelection() const { return _customSelection; }
        // Tracked by SyncService, so reopening the page during a deletion cannot send a second SYNC_DELETE.
        [[nodiscard]] bool deletePending() const;

        Q_INVOKABLE void open(qint64 driveDbId);
        /// Releases the target only when it is still the given drive, so a closing page cannot reset its successor.
        Q_INVOKABLE void close(qint64 driveDbId);
        Q_INVOKABLE void retrySelection();
        Q_INVOKABLE void openLocalFolder() const;
        Q_INVOKABLE void deleteMainSync();

    signals:
        void presentationChanged();
        // The targeted drive has no synchronization left.
        void driveRemoved();
        void deleteSucceeded();
        void deleteFailed();

    private:
        enum class SelectionState : uint8_t {
            Idle,
            Loading,
            Loaded,
            Failed,
        };

        [[nodiscard]] bool hasTarget() const { return _driveDbId != 0; }
        void refresh();
        void loadSelection();
        void resetTarget();

        AppCache &_appCache;
        CommService &_commService;
        SyncService &_syncService;
        DriveDbId _driveDbId{0};
        AvailableDriveKey _availableDriveKey;
        SyncDbId _mainSyncDbId{0};
        // Deleted synchronization awaiting its response; it may already have left the cache through SYNC_REMOVED.
        SyncDbId _deletingSyncDbId{0};
        QString _driveName;
        QColor _driveColor;
        QString _localPath;
        SelectionState _selectionState{SelectionState::Idle};
        uint64_t _targetGeneration{0};
        uint64_t _selectionGeneration{0};
        bool _syncCreationPending{false};
        bool _customSelection{false};
};

} // namespace KDC
