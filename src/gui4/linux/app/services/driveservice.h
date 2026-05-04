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
 * High-level drive-oriented facade.
 *
 * Role:
 * - orchestrates drive requests through CommService;
 * - refreshes AppCache snapshots on successful loads;
 * - reports transient failures through ServiceEventBus;
 * - registers durable pending state in ServiceActionTracker.
 */
class DriveService : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)

    public:
        explicit DriveService(CommService &commService, AppCache &appCache, ServiceActionTracker &serviceActionTracker,
                              ServiceEventBus &serviceEventBus, QObject *parent = nullptr);

        [[nodiscard]] bool loading() const;

        Q_INVOKABLE void loadDrives();
        Q_INVOKABLE void deleteDrive(qint64 driveDbId);
        Q_INVOKABLE [[nodiscard]] bool isLoadDrivesPending() const;
        Q_INVOKABLE [[nodiscard]] bool isDeleteDrivePending(qint64 driveDbId) const;
        Q_INVOKABLE [[nodiscard]] bool isUpdateDrivePending(qint64 driveDbId) const;

        void updateDrive(const DriveInfo &driveInfo);

    signals:
        void loadingChanged();

    private:
        void beginAction(const ServiceActionTracker::ActionKey &actionKey, ServiceActionTracker::ScopeId scopeId = 0);
        void endAction(const ServiceActionTracker::ActionKey &actionKey, ServiceActionTracker::ScopeId scopeId = 0);
        [[nodiscard]] bool isActionPending(const ServiceActionTracker::ActionKey &actionKey,
                                           ServiceActionTracker::ScopeId scopeId = 0) const;
        void notifyRequestFailure(const ExitInfo &exitInfo, RequestNum requestNum);

        CommService &_commService;
        AppCache &_appCache;
        ServiceActionTracker &_serviceActionTracker;
        ServiceEventBus &_serviceEventBus;
};

} // namespace KDC
