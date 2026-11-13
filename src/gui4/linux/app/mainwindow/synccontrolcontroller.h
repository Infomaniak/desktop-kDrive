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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "app/mainwindow/synccontrolstate.h"
#include "libcommon/utility/types.h"

#include <QObject>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace KDC {

class AppCache;
class MainSelectionStore;
class NetworkStatusObserver;
class SyncErrorsProjection;
class SyncService;

/**
 * QML-facing state of the toolbar Pause/Resume control.
 *
 * With one synchronization, the control pauses or resumes it. With several, it opens a menu that acts on the selected
 * synchronization or on every synchronization of every user. Only the synchronizations whose control is Pause or Resume are
 * counted: a blocked synchronization, or one with a status or request in transition, is counted nowhere. The whole control
 * is disabled offline. Every action recomputes its targets before reaching SyncService.
 */
class SyncControlController final : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool multipleSyncs READ multipleSyncs NOTIFY changed)
        Q_PROPERTY(KDC::SyncControl::SyncControlState selectedState READ selectedState NOTIFY changed)
        Q_PROPERTY(int32_t pauseAllCount READ pauseAllCount NOTIFY changed)
        Q_PROPERTY(int32_t resumeAllCount READ resumeAllCount NOTIFY changed)
        Q_PROPERTY(bool available READ available NOTIFY changed)

    public:
        SyncControlController(const AppCache &appCache, const MainSelectionStore &selectionStore,
                              const SyncErrorsProjection &projection, SyncService &syncService,
                              const NetworkStatusObserver &networkStatusObserver, QObject *parent = nullptr);

        // At least two synchronizations are configured: the control opens the menu.
        [[nodiscard]] bool multipleSyncs() const { return _snapshot.syncCount > 1; }
        [[nodiscard]] SyncControlState selectedState() const { return _snapshot.selectedState; }
        [[nodiscard]] int32_t pauseAllCount() const { return static_cast<int32_t>(_snapshot.pauseTargets.size()); }
        [[nodiscard]] int32_t resumeAllCount() const { return static_cast<int32_t>(_snapshot.resumeTargets.size()); }
        // At least one action is available on the selected synchronization or on every synchronization.
        [[nodiscard]] bool available() const;

        Q_INVOKABLE void toggleSelected();
        Q_INVOKABLE void pauseAll();
        Q_INVOKABLE void resumeAll();

    signals:
        void changed();

    private:
        struct Snapshot {
                std::size_t syncCount{0};
                SyncControlState selectedState{SyncControlState::Disabled};
                std::vector<SyncDbId> pauseTargets;
                std::vector<SyncDbId> resumeTargets;

                friend bool operator==(const Snapshot &lhs, const Snapshot &rhs) = default;
        };

        [[nodiscard]] Snapshot computeSnapshot() const;
        [[nodiscard]] SyncControlState syncControlStateOf(SyncDbId syncDbId, bool blocked) const;
        void refresh();

        const AppCache &_appCache;
        const MainSelectionStore &_selectionStore;
        const SyncErrorsProjection &_projection;
        SyncService &_syncService;
        const NetworkStatusObserver &_networkStatusObserver;
        Snapshot _snapshot;
};

} // namespace KDC
