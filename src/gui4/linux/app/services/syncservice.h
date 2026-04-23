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

#include <cstdint>

namespace KDC {

/**
 * High-level sync-oriented facade exposed to QML.
 *
 * Role:
 * - orchestrates sync requests through CommService;
 * - updates AppCache snapshots and incremental entities;
 * - reports transient cross-service failures through ServiceEventBus;
 * - registers per-action pending state in ServiceActionTracker.
 */
class SyncService : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)

    public:
        explicit SyncService(CommService &commService, AppCache &appCache, ServiceActionTracker &serviceActionTracker,
                             ServiceEventBus &serviceEventBus, QObject *parent = nullptr);

        [[nodiscard]] bool loading() const { return _loading; }

        Q_INVOKABLE void loadSyncs();
        Q_INVOKABLE void addSync(qint64 userDbId, qint64 accountId, qint64 driveId, const QString &localFolderPath,
                                 const QString &serverFolderPath, const QString &serverFolderNodeId, bool liteSync);
        Q_INVOKABLE void startSync(qint64 syncDbId);
        Q_INVOKABLE void stopSync(qint64 syncDbId);
        Q_INVOKABLE void deleteSync(qint64 syncDbId);
        Q_INVOKABLE void querySyncStatus(qint64 syncDbId);
        Q_INVOKABLE void findGoodPathForNewSync(const QString &basePath);
        Q_INVOKABLE void isPathValidForNewSync(const QString &path, int32_t syncConfiguration);
        Q_INVOKABLE [[nodiscard]] bool isLoadSyncsPending() const;
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
        // The second argument can carry a non-blocking warning message from the backend.
        void suggestedPathReceived(const QString &goodPath, const QString &warningMessage);
        void pathValidationReceived(bool isValid);
        void syncAddCompleted(qint64 syncDbId);

    private:
        void beginAction(const QString &actionKey, qint64 scopeId = 0);
        void endAction(const QString &actionKey, qint64 scopeId = 0);
        void setLoading(bool loading);
        [[nodiscard]] bool isActionPending(const QString &actionKey, qint64 scopeId = 0) const;
        void notifyRequestFailure(const ExitInfo &exitInfo, RequestNum requestNum);
        [[nodiscard]] bool isValidSyncConfigurationValue(int32_t syncConfiguration) const;

        CommService &_commService;
        AppCache &_appCache;
        ServiceActionTracker &_serviceActionTracker;
        ServiceEventBus &_serviceEventBus;
        bool _loading{false};
};

} // namespace KDC
