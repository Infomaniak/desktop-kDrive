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

#include <QMetaObject>
#include <QObject>

#include <vector>

namespace KDC {

/**
 * Owns all server-push signal connections from CommService to AppCache.
 *
 * This is the single bridge for push-driven cache mutation in the Linux v4 services layer.
 *
 * It starts in pre-hydration mode: supported CommService push signals are connected to a drop logger only, so live server
 * mutations cannot race with CachePopulator's initial full-snapshot replacements. Once markHydrated() is called after
 * CachePopulator::bootstrapCompleted(), those temporary drop connections are removed and the live pipeline is installed.
 *
 * Push signals are connected directly to matching AppCache mutation slots in live mode. The class owns only the signal
 * wiring; AppCache remains the cache authority, and CachePopulator remains responsible for initial snapshot loading.
 */
class CachePipeline : public QObject {
        Q_OBJECT

    public:
        explicit CachePipeline(CommService &commService, AppCache &appCache, QObject *parent = nullptr);

    public slots:
        void markHydrated();

    private:
        void connectDropPipeline();
        void connectLivePipeline();
        static void logDroppedPush(const char *signalName);

        CommService &_commService;
        AppCache &_appCache;
        std::vector<QMetaObject::Connection> _preHydrationConnections;
        bool _hydrated{false};
};

} // namespace KDC
