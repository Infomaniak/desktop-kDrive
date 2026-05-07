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

#include <QObject>

namespace KDC {

/**
 * Sequential initial snapshot loader for Linux v4 cache population.
 *
 * Loads users, then accounts, then drives, then syncs, then errors so the
 * graph-backed AppCache is populated in parent-first order.
 */
class CachePopulator : public QObject {
        Q_OBJECT

    public:
        explicit CachePopulator(CommService &commService, AppCache &appCache, QObject *parent = nullptr);
        void bootstrap();

    signals:
        void bootstrapCompleted();

    private:
        void loadUsers();
        void loadAccounts();
        void loadDrives();
        void loadSyncs();
        void loadSyncErrors();

        CommService &_commService;
        AppCache &_appCache;
};

} // namespace KDC
