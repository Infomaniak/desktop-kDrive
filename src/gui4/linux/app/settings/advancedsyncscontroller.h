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

#include "app/settings/advancedsynclistmodel.h"
#include "libcommon/utility/types.h"

#include <QColor>
#include <QObject>

#include <cstdint>

namespace KDC {

class AppCache;
class CommService;
class SyncService;

/**
 * Settings "Advanced sync" page state for one user drive.
 *
 * Role: project the drive's advanced synchronizations from AppCache into `AdvancedSyncListModel`, load each blacklist to
 * present a custom selection, open their local and remote folders, and delete them. Like the drive management page, it
 * targets a DriveDbId, so another user's synchronizations of the same backend drive are never presented. Leaving the
 * drive once it has no synchronization left is up to the drive management page, which stays below this one.
 */
class AdvancedSyncsController final : public QObject {
        Q_OBJECT
        Q_PROPERTY(AdvancedSyncListModel *model READ model CONSTANT)
        Q_PROPERTY(QColor driveColor READ driveColor NOTIFY presentationChanged)
        Q_PROPERTY(bool empty READ empty NOTIFY presentationChanged)
        Q_PROPERTY(bool deletePending READ deletePending NOTIFY presentationChanged)

    public:
        AdvancedSyncsController(AppCache &appCache, CommService &commService, SyncService &syncService,
                                QObject *parent = nullptr);

        [[nodiscard]] AdvancedSyncListModel *model() { return &_model; }
        [[nodiscard]] QColor driveColor() const { return _driveColor; }
        [[nodiscard]] bool empty() const { return _model.rows().empty(); }
        // A deletion requested from this page is waiting for its response.
        [[nodiscard]] bool deletePending() const { return _deletingSyncDbId != 0; }

        Q_INVOKABLE void open(qint64 driveDbId);
        /// Releases the target only when it is still the given drive, so a closing page cannot reset its successor.
        Q_INVOKABLE void close(qint64 driveDbId);
        /// Reloads a confirmed blacklist, after a failure or once the excluded folders page saved a new one.
        Q_INVOKABLE void reloadBlackList(qint64 syncDbId);
        Q_INVOKABLE void openLocalFolder(qint64 syncDbId) const;
        Q_INVOKABLE void openRemoteFolder(qint64 syncDbId);
        Q_INVOKABLE void deleteSync(qint64 syncDbId);

    signals:
        void presentationChanged();
        void deleteSucceeded(qint64 syncDbId);
        void deleteFailed(qint64 syncDbId);

    private:
        [[nodiscard]] bool hasTarget() const { return _driveDbId != 0; }
        [[nodiscard]] bool targets(const SyncDbId syncDbId) const { return hasTarget() && _model.row(syncDbId) != nullptr; }
        void refresh();
        void loadBlackList(SyncDbId syncDbId);
        void resetTarget();

        AppCache &_appCache;
        CommService &_commService;
        SyncService &_syncService;
        AdvancedSyncListModel _model;
        DriveDbId _driveDbId{0};
        DriveId _driveId{0};
        QColor _driveColor;
        // Deleted synchronization awaiting its response; it may already have left the cache through SYNC_REMOVED.
        SyncDbId _deletingSyncDbId{0};
        uint64_t _targetGeneration{0};
};

} // namespace KDC
