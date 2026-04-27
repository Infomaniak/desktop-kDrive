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
#include "app/cache/cachetypes.h"

#include <QObject>

#include <optional>
#include <vector>

namespace KDC {

/**
 * Sync-first main-shell selection owner for Linux v4.
 *
 * Role: own currentSyncDbId and heal it when cache graph changes.
 * Non-role: own cached entities or onboarding selections.
 * All mutations must run on the Qt main thread.
 */
class MainSelectionStore : public QObject {
        Q_OBJECT
        Q_PROPERTY(qint64 currentSyncDbId READ currentSyncDbId NOTIFY currentSyncDbIdChanged)

    public:
        explicit MainSelectionStore(AppCache &cache, QObject *parent = nullptr);

        [[nodiscard]] qint64 currentSyncDbId() const;
        [[nodiscard]] std::optional<SyncContext> currentSyncContext() const;
        [[nodiscard]] std::vector<SyncContext> syncContexts() const;

        Q_INVOKABLE void selectSync(qint64 syncDbId);
        Q_INVOKABLE void clearSelection();
        Q_INVOKABLE void ensureValidSelection();

    signals:
        void currentSyncDbIdChanged();
        void currentSyncContextChanged();

    private:
        void handleCacheChanged();
        void setCurrentSyncDbId(SyncDbId syncDbId);
        [[nodiscard]] SyncDbId firstAvailableSyncDbId() const;

        AppCache &_cache;
        SyncDbId _currentSyncDbId{0};
        SyncDbId _lastRequestedSyncDbId{0};
};

} // namespace KDC
