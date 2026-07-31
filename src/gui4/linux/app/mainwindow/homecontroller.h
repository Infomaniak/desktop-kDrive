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

#include "libcommon/utility/cstypes.h"

#include <QObject>
#include <QString>

#include <cstdint>
#include <optional>

namespace KDC {

class AppCache;
class AppRouter;
class MainSelectionStore;
class NetworkStatusObserver;
class SyncService;
class SystemTrayController;

/**
 * Cache-backed QML adapter for the Linux v4 Home and its toolbar controls.
 *
 * The controller centralizes presentation-state resolution and synchronization actions. QML remains a modular rendering layer
 * and does not call backend services directly.
 */
class HomeController final : public QObject {
        Q_OBJECT
        Q_PROPERTY(HomeStatus status READ status NOTIFY homeChanged)
        Q_PROPERTY(PrimaryAction primaryAction READ primaryAction NOTIFY homeChanged)
        Q_PROPERTY(SyncControlState syncControlState READ syncControlState NOTIFY homeChanged)
        Q_PROPERTY(QString firstName READ firstName NOTIFY homeChanged)
        Q_PROPERTY(QString driveName READ driveName NOTIFY homeChanged)
        Q_PROPERTY(QString avatarSource READ avatarSource NOTIFY homeChanged)
        Q_PROPERTY(int32_t errorCount READ errorCount NOTIFY homeChanged)
        Q_PROPERTY(bool hasCurrentDrive READ hasCurrentDrive NOTIFY homeChanged)
        Q_PROPERTY(bool hasConnectedUser READ hasConnectedUser NOTIFY homeChanged)

    public:
        enum class HomeStatus : uint8_t {
            Loading = 0,
            UpToDate,
            Syncing,
            Paused,
            Offline,
            SetupRequired,
        };
        Q_ENUM(HomeStatus)

        enum class PrimaryAction : uint8_t {
            None = 0,
            HideWindow,
            ShowActivities,
            ResumeSync,
            SignIn,
            ConfigureSync,
        };
        Q_ENUM(PrimaryAction)

        enum class SyncControlState : uint8_t {
            Disabled = 0,
            Pending,
            Pause,
            Resume,
        };
        Q_ENUM(SyncControlState)

        explicit HomeController(AppCache &appCache, MainSelectionStore &mainSelectionStore, SyncService &syncService,
                                AppRouter &appRouter, SystemTrayController &systemTrayController,
                                NetworkStatusObserver &networkStatusObserver, QObject *parent = nullptr);

        [[nodiscard]] HomeStatus status() const;
        [[nodiscard]] PrimaryAction primaryAction() const;
        [[nodiscard]] SyncControlState syncControlState() const;
        [[nodiscard]] QString firstName() const;
        [[nodiscard]] QString driveName() const;
        [[nodiscard]] QString avatarSource() const;
        [[nodiscard]] int32_t errorCount() const;
        [[nodiscard]] bool hasCurrentDrive() const;
        [[nodiscard]] bool hasConnectedUser() const;

        Q_INVOKABLE void triggerPrimaryAction();
        Q_INVOKABLE void toggleSync();

    signals:
        void homeChanged();
        void setupRequested();

    private:
        [[nodiscard]] std::optional<SyncStatus> currentRuntimeStatus() const;
        [[nodiscard]] qint64 currentSyncDbId() const;
        [[nodiscard]] bool syncActionPending() const;

        AppCache &_appCache;
        MainSelectionStore &_mainSelectionStore;
        SyncService &_syncService;
        AppRouter &_appRouter;
        SystemTrayController &_systemTrayController;
        NetworkStatusObserver &_networkStatusObserver;
};

} // namespace KDC
