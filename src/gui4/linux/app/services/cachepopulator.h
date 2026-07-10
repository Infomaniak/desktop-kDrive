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
#include "app/cache/parametersstore.h"
#include "app/services/commservice.h"

#include <QObject>

#include <cstdint>

namespace KDC {

/**
 * Sequential parent-first snapshot loader for Linux v4 cache population and explicit reconciliation.
 *
 * Loads parameters first, then users, accounts, drives, syncs, and errors so GUI state is populated in parent-first
 * order. Product entities go to AppCache; application parameters go to ParametersStore. Initial bootstrap remains fatal
 * on failure because the app has no coherent cache to run from. Reconciliation reuses the same sequence after
 * recoverable mutations and reports failures to the caller without emitting the bootstrap signal. Once a snapshot is
 * complete, asks the server to refresh live user/account/drive metadata so quota-only drive updates are pushed through
 * CachePipeline.
 */
class CachePopulator : public QObject {
        Q_OBJECT

    public:
        explicit CachePopulator(CommService &commService, AppCache &appCache, ParametersStore &parametersStore,
                                QObject *parent = nullptr);
        /**
         * Populates the initial cache snapshot.
         *
         * Failure is fatal because Linux v4 cannot safely start without a coherent user/account/drive/sync graph.
         */
        void bootstrap();

        /**
         * Repairs the cache snapshot while the app is already running.
         *
         * Failure is reported through reconciliationFailed() so the caller can keep the UI in a recoverable state.
         */
        void reconcile();

    signals:
        /// Emitted only after the initial startup snapshot completes successfully.
        void bootstrapCompleted();
        /// Emitted after an explicit recoverable cache repair completes successfully.
        void reconciliationCompleted();
        /// Emitted when an explicit recoverable cache repair fails.
        void reconciliationFailed();

    private:
        enum class PopulationMode : uint8_t {
            Bootstrap,
            Reconciliation,
        };

        void loadParameters(PopulationMode mode);
        void loadUsers(PopulationMode mode);
        void loadAccounts(PopulationMode mode);
        void loadDrives(PopulationMode mode);
        void loadSyncs(PopulationMode mode);
        void loadSyncErrors(PopulationMode mode);
        void activateLiveInfoRefresh() const;
        [[nodiscard]] bool handlePopulationFailure(const char *stage, const ExitInfo &exitInfo, PopulationMode mode);

        CommService &_commService;
        AppCache &_appCache;
        ParametersStore &_parametersStore;
};

} // namespace KDC
