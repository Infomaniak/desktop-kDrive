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

#include "app/services/commservice.h"
#include "app/services/serviceactiontracker.h"
#include "app/services/serviceeventbus.h"

#include <QObject>
#include <QString>

#include <cstdint>

namespace KDC {

/**
 * High-level sync-oriented facade.
 *
 * Role:
 * - orchestrates sync lifecycle requests through CommService;
 * - keeps durable cache mutations signal-driven through CachePipeline;
 * - reports transient failures through ServiceEventBus;
 * - registers durable pending state in ServiceActionTracker.
 */
class SyncService : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)

    public:
        explicit SyncService(CommService &commService, ServiceActionTracker &serviceActionTracker,
                             ServiceEventBus &serviceEventBus, QObject *parent = nullptr);

        [[nodiscard]] bool loading() const;

        Q_INVOKABLE void addSync(qint64 userDbId, qint64 accountId, qint64 driveId, const QString &localFolderPath,
                                 const QString &serverFolderPath, const QString &serverFolderNodeId, bool liteSync);
        Q_INVOKABLE void startSync(qint64 syncDbId);
        Q_INVOKABLE void stopSync(qint64 syncDbId);
        Q_INVOKABLE void deleteSync(qint64 syncDbId);
        Q_INVOKABLE void querySyncStatus(qint64 syncDbId);
        Q_INVOKABLE void findGoodPathForNewSync(const QString &basePath);
        Q_INVOKABLE void isPathValidForNewSync(const QString &path, int32_t syncConfiguration);

        Q_INVOKABLE [[nodiscard]] bool isAddSyncPending() const;
        Q_INVOKABLE [[nodiscard]] bool isStartSyncPending(qint64 syncDbId) const;
        Q_INVOKABLE [[nodiscard]] bool isStopSyncPending(qint64 syncDbId) const;
        Q_INVOKABLE [[nodiscard]] bool isDeleteSyncPending(qint64 syncDbId) const;
        Q_INVOKABLE [[nodiscard]] bool isQuerySyncStatusPending(qint64 syncDbId) const;
        Q_INVOKABLE [[nodiscard]] bool isFindGoodPathForNewSyncPending() const;
        Q_INVOKABLE [[nodiscard]] bool isPathValidForNewSyncPending() const;

    signals:
        void loadingChanged();
        void syncStatusReceived(qint64 syncDbId, int32_t status);
        void suggestedPathReceived(const QString &goodPath, const QString &warningMessage);
        void pathValidationReceived(bool isValid);
        void syncAddCompleted(qint64 syncDbId);

    private:
        void beginAction(const ServiceActionTracker::ActionKey &actionKey, ServiceActionTracker::ScopeId scopeId = 0);
        void endAction(const ServiceActionTracker::ActionKey &actionKey, ServiceActionTracker::ScopeId scopeId = 0);
        [[nodiscard]] bool isActionPending(const ServiceActionTracker::ActionKey &actionKey,
                                           ServiceActionTracker::ScopeId scopeId = 0) const;
        void notifyRequestFailure(const ExitInfo &exitInfo, RequestNum requestNum);
        [[nodiscard]] bool isValidSyncConfigurationValue(int32_t syncConfiguration) const;

        CommService &_commService;
        ServiceActionTracker &_serviceActionTracker;
        ServiceEventBus &_serviceEventBus;
        uint64_t _findGoodPathGeneration{0};
        uint64_t _pathValidationGeneration{0};
};

} // namespace KDC
