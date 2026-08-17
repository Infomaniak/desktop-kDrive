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

#include "app/cache/activitystore.h"
#include "app/cache/appcache.h"
#include "app/services/commservice.h"

#include <QMetaObject>
#include <QObject>

#include <vector>

namespace KDC {

/**
 * Owns all server-push signal connections from CommService to Linux v4 cache stores.
 *
 * This is the single bridge for push-driven cache mutation in the Linux v4 services layer.
 *
 * It starts in pre-population mode: supported CommService push signals are connected to a drop logger only, so live server
 * mutations cannot race with CachePopulator's initial full-snapshot replacements. Once markPopulated() is called after
 * CachePopulator::bootstrapCompleted(), those temporary drop connections are removed and the live pipeline is installed.
 *
 * Entity push signals are connected directly to matching AppCache mutation slots in live mode. File activity signals are
 * also dropped during population: receiving one in that phase violates the server/client startup contract. The class owns
 * only the signal wiring; AppCache and ActivityStore remain their respective cache authorities, and CachePopulator remains
 * responsible for initial snapshot loading.
 */
class CachePipeline : public QObject {
        Q_OBJECT

    public:
        explicit CachePipeline(CommService &commService, AppCache &appCache, ActivityStore &activityStore,
                               QObject *parent = nullptr);

    public slots:
        void markPopulated();

    private:
        void connectDropPipeline();
        void connectLivePipeline();
        void routeActivity(SyncDbId syncDbId, const SyncFileItemInfo &item) const;
        void reconcileInProgressActivities(SyncDbId syncDbId) const;
        void reconcileActivities() const;
        static void logDroppedPush(const char *signalName);

        CommService &_commService;
        AppCache &_appCache;
        ActivityStore &_activityStore;
        std::vector<QMetaObject::Connection> _prePopulationConnections;
        bool _populated{false};
};

} // namespace KDC
