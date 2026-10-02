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
#include "app/services/commservice.h"
#include "app/services/serviceactiontracker.h"
#include "app/services/serviceeventbus.h"

#include <QObject>
#include <QString>

#include <cstdint>
#include <unordered_map>
#include <unordered_set>

namespace KDC {

class AppCache;
class CachePopulator;

/**
 * High-level sync-oriented facade.
 *
 * Role:
 * - orchestrates sync lifecycle requests through CommService;
 * - keeps durable cache mutations signal-driven through CachePipeline;
 * - reports transient failures through ServiceEventBus;
 * - registers durable pending state in ServiceActionTracker;
 * - is the only sender of SYNC_ADD, so that AppCache can reserve each drive while its creation is in flight.
 */
class SyncService : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)

    public:
        explicit SyncService(CommService &commService, AppCache &appCache, CachePopulator &cachePopulator,
                             ServiceActionTracker &serviceActionTracker, ServiceEventBus &serviceEventBus,
                             QObject *parent = nullptr);

        [[nodiscard]] bool loading() const;

        // Sends SYNC_ADD. A classic sync (drive root) reserves the drive until AppCache reflects the outcome, and is refused
        // without sending anything when the drive already has a classic sync or one in flight. Advanced syncs are not
        // restricted.
        [[nodiscard]] bool addDriveSync(const SyncAddRequest &request, const CommService::SyncInfoCallback &callback);

        Q_INVOKABLE void startSync(qint64 syncDbId);
        Q_INVOKABLE void stopSync(qint64 syncDbId);
        Q_INVOKABLE void deleteSync(qint64 syncDbId);
        // Sends SYNC_DELETE and reports its outcome. The cache still learns the removal from the SYNC_REMOVED push.
        void deleteSync(SyncDbId syncDbId, const CommService::VoidCallback &callback);
        Q_INVOKABLE void querySyncStatus(qint64 syncDbId);
        Q_INVOKABLE void findGoodPathForNewSync(const QString &basePath);
        Q_INVOKABLE void isPathValidForNewSync(const QString &path, int32_t syncConfiguration);

        Q_INVOKABLE [[nodiscard]] bool isStartSyncPending(qint64 syncDbId) const;
        Q_INVOKABLE [[nodiscard]] bool isStopSyncPending(qint64 syncDbId) const;
        Q_INVOKABLE [[nodiscard]] bool isDeleteSyncPending(qint64 syncDbId) const;
        Q_INVOKABLE [[nodiscard]] bool isQuerySyncStatusPending(qint64 syncDbId) const;
        Q_INVOKABLE [[nodiscard]] bool isFindGoodPathForNewSyncPending() const;
        Q_INVOKABLE [[nodiscard]] bool isPathValidForNewSyncPending() const;

    signals:
        void loadingChanged();
        void syncActionPendingChanged(qint64 syncDbId);
        void syncStatusReceived(qint64 syncDbId, int32_t status);
        void suggestedPathReceived(const QString &goodPath, const QString &warningMessage);
        void pathValidationReceived(bool isValid);

    private:
        void beginAction(const ServiceActionTracker::ActionKey &actionKey, ServiceActionTracker::ScopeId scopeId = 0);
        void endAction(const ServiceActionTracker::ActionKey &actionKey, ServiceActionTracker::ScopeId scopeId = 0);
        [[nodiscard]] bool isActionPending(const ServiceActionTracker::ActionKey &actionKey,
                                           ServiceActionTracker::ScopeId scopeId = 0) const;
        void notifyRequestFailure(const ExitInfo &exitInfo, RequestNum requestNum);
        [[nodiscard]] bool isValidSyncConfigurationValue(int32_t syncConfiguration) const;
        void releaseSyncedReservations();
        void releaseReconciledReservations();

        CommService &_commService;
        AppCache &_appCache;
        CachePopulator &_cachePopulator;
        ServiceActionTracker &_serviceActionTracker;
        ServiceEventBus &_serviceEventBus;
        uint64_t _findGoodPathGeneration{0};
        uint64_t _pathValidationGeneration{0};
        // Successful creations whose SYNC_ADDED push has not reached AppCache yet.
        std::unordered_map<AvailableDriveKey, SyncDbId> _awaitingSyncPush;
        // Failed creations: SYNC_ADD may still have persisted the sync, so the reservation lasts until the cache is reconciled.
        std::unordered_set<AvailableDriveKey> _awaitingReconciliation;
};

} // namespace KDC
