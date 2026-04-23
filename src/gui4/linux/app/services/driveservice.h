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

#include "app/cache/appcache.h"
#include "app/services/commservice.h"
#include "app/services/serviceactiontracker.h"
#include "app/services/serviceeventbus.h"

#include <QObject>
#include <QString>

namespace KDC {

/**
 * High-level drive-oriented facade exposed to QML.
 *
 * Role:
 * - orchestrates drive requests through CommService;
 * - updates AppCache snapshots and incremental entities;
 * - reports transient cross-service failures through ServiceEventBus;
 * - registers per-action pending state in ServiceActionTracker.
 *
 * QML should use the Q_INVOKABLE methods.
 * updateDrive() remains C++-facing until a stable QML edit contract for DriveInfo is introduced.
 */
class DriveService : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
        Q_PROPERTY(qint64 activeDriveDbId READ activeDriveDbId NOTIFY activeDriveDbIdChanged)

    public:
        explicit DriveService(CommService &commService, AppCache &appCache, ServiceActionTracker &serviceActionTracker,
                              ServiceEventBus &serviceEventBus, QObject *parent = nullptr);

        [[nodiscard]] bool loading() const { return _loading; }
        [[nodiscard]] qint64 activeDriveDbId() const { return _appCache.selectedDriveDbId(); }

        Q_INVOKABLE void loadDrives();
        Q_INVOKABLE void deleteDrive(qint64 driveDbId);
        Q_INVOKABLE void setActiveDrive(qint64 driveDbId);
        Q_INVOKABLE [[nodiscard]] bool isLoadDrivesPending() const;
        Q_INVOKABLE [[nodiscard]] bool isDeleteDrivePending(qint64 driveDbId) const;
        Q_INVOKABLE [[nodiscard]] bool isUpdateDrivePending(qint64 driveDbId) const;

        void updateDrive(const DriveInfo &driveInfo);

    signals:
        void loadingChanged();
        void activeDriveDbIdChanged();

    private:
        void beginAction(const QString &actionKey, qint64 scopeId = 0);
        void endAction(const QString &actionKey, qint64 scopeId = 0);
        void setLoading(bool loading);
        [[nodiscard]] bool isActionPending(const QString &actionKey, qint64 scopeId = 0) const;
        void notifyRequestFailure(const ExitInfo &exitInfo, RequestNum requestNum);

        CommService &_commService;
        AppCache &_appCache;
        ServiceActionTracker &_serviceActionTracker;
        ServiceEventBus &_serviceEventBus;
        bool _loading{false};
};

} // namespace KDC
