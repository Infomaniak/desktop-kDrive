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
#include "app/cache/cachepipeline.h"
#include "app/cache/mainselectionstore.h"
#include "app/cache/onboardingstate.h"
#include "app/models/availabledrivelistmodel.h"
#include "app/models/synclistmodel.h"
#include "app/services/cachepopulator.h"
#include "app/services/commservice.h"
#include "app/services/driveservice.h"
#include "app/services/serviceactiontracker.h"
#include "app/services/serviceeventbus.h"
#include "app/services/syncservice.h"
#include "app/services/userservice.h"
#include "communicationlayer/ipcclient.h"
#include "communicationlayer/signaldispatcher.h"

#include <QApplication>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>

namespace KDC {

Q_DECLARE_LOGGING_CATEGORY(lcAppClientLinux)

/**
 * Top-level application object for the Linux GUI client.
 *
 * Owns the IPC client and cross-service infrastructure objects:
 * - SignalDispatcher: routes server push messages to typed handlers.
 * - CommService: typed request/signal facade over IPC.
 * - CachePipeline: unique bridge from CommService push signals to AppCache.
 * - CachePopulator: sequential initial snapshot loader for the graph-backed cache.
 * - ServiceActionTracker: durable UI-facing pending-action state.
 * - ServiceEventBus: transient cross-service events (errors, notifications, ...).
 *
 * On construction, sets up logging,
 * wires IPC signals to the dispatcher, and initiates the connection to the server.
 */
class AppClientLinux : public QApplication {
        Q_OBJECT

    public:
        explicit AppClientLinux(int &argc, char **argv);

        /**
         * Provides access to the signal dispatcher for registering server signal handlers.
         * Call registerHandler() on the returned dispatcher before the IPC connection is established.
         */
        SignalDispatcher &signalDispatcher() { return _signalDispatcher; }
        CommService &serverCommService() { return _serverCommService; }
        AppCache &appCache() { return _appCache; }
        MainSelectionStore &mainSelectionStore() { return _mainSelectionStore; }
        OnboardingState &onboardingState() { return _onboardingState; }
        ServiceActionTracker &serviceActionTracker() { return _serviceActionTracker; }
        ServiceEventBus &serviceEventBus() { return _serviceEventBus; }

    signals:
        /** Emitted once the first IPC connection to the server has been successfully established. */
        void ipcConnected();
        /** Emitted when the IPC connection is lost after having been established. Considered fatal. */
        void ipcDisconnected();

    private:
        static void setupLogging();

        IpcClient _ipcClient{this};
        SignalDispatcher _signalDispatcher{this};
        CommService _serverCommService{_ipcClient, _signalDispatcher, this};
        AppCache _appCache{this};
        CachePipeline _cachePipeline{_serverCommService, _appCache, this};
        MainSelectionStore _mainSelectionStore{_appCache, this};
        OnboardingState _onboardingState{_appCache, this};
        AvailableDriveListModel _availableDriveListModel{_appCache, _onboardingState, this};
        CurrentSyncModel _currentSyncModel{_mainSelectionStore, this};
        DriveListModel _driveListModel{_appCache, _mainSelectionStore, this};
        SyncListModel _syncListModel{_appCache, _mainSelectionStore, this};
        ServiceActionTracker _serviceActionTracker{this};
        ServiceEventBus _serviceEventBus{this};
        CachePopulator _cachePopulator{_serverCommService, _appCache, this};
        UserService _userService{_serverCommService, _appCache, _serviceActionTracker, _serviceEventBus, this};
        DriveService _driveService{_serverCommService, _serviceActionTracker, _serviceEventBus, this};
        SyncService _syncService{_serverCommService, _serviceActionTracker, _serviceEventBus, this};
        QQmlApplicationEngine _qmlEngine;
};

} // namespace KDC
