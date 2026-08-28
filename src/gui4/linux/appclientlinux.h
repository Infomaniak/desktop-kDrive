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
#include "app/cache/activitystore.h"
#include "app/cache/cachepipeline.h"
#include "app/cache/mainselectionstore.h"
#include "app/cache/parametersstore.h"
#include "app/dialogs/manydeletescontroller.h"
#include "app/mainwindow/activitiescontroller.h"
#include "app/mainwindow/homecontroller.h"
#include "app/mainwindow/mainsidebarcontroller.h"
#include "app/mainwindow/networkstatusobserver.h"
#include "app/navigation/approuter.h"
#include "app/onboarding/onboardingsessionmanager.h"
#include "app/services/cachepopulator.h"
#include "app/services/commservice.h"
#include "app/services/activityservice.h"
#include "app/services/driveservice.h"
#include "app/services/parametersservice.h"
#include "app/services/sentryservice.h"
#include "app/services/serviceactiontracker.h"
#include "app/services/serviceeventbus.h"
#include "app/services/syncservice.h"
#include "app/services/userservice.h"
#include "app/systraycontroller.h"
#include "communicationlayer/ipcclient.h"
#include "communicationlayer/signaldispatcher.h"
#include "ui/chrome/windowdecorationcontroller.h"

#include <QApplication>
#include <QIcon>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QTranslator>

namespace KDC {

Q_DECLARE_LOGGING_CATEGORY(lcAppClientLinux)

/**
 * @brief Composition root for the Linux GUI client.
 *
 * Owns and wires the process-long application layers:
 * - IPC transport, server-signal dispatch, and the typed communication facade.
 * - AppCache, ActivityStore, ParametersStore, their live push pipeline, and the two-branch bootstrap population.
 * - Application services, action tracking, transient service events, and Sentry coordination.
 * - Main selection, navigation, sidebar, Home, Activities, global dialogs, and the ephemeral onboarding-session manager.
 * - Linux system tray, network observation, frameless-window integration, translations, and the QML runtime.
 *
 * Construction configures logging, translations, application identity, the system tray, signal connections, and the QML
 * engine before initiating the IPC connection to the server.
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
        ParametersStore &parametersStore() { return _parametersStore; }
        ParametersService &parametersService() { return _parametersService; }
        AppRouter &appRouter() { return _appRouter; }
        ServiceActionTracker &serviceActionTracker() { return _serviceActionTracker; }
        ServiceEventBus &serviceEventBus() { return _serviceEventBus; }

    signals:
        /** Emitted once the first IPC connection to the server has been successfully established. */
        void ipcConnected();
        /** Emitted when the IPC connection is lost after having been established. Considered fatal. */
        void ipcDisconnected();

    private:
        static void setupLogging();
        static void configureLogger();
        static void logApplicationInformation();
        static void logSystemInformation();
        static void logDisplayInformation();
        static void logQtInformation();
        static void logScreenInformation();
        void setupTranslations();
        void setupSystemTray();
        void setupSignalConnections();
        void setupQmlEngine(const QIcon &appIcon);
        void setupIpcConnection();
        void handleIpcDisconnection();
        void handleBootstrapCompletion();
        void refreshUpdaterState();
        void updateLoggerMinLevel() const;
        void requestQuit();
        void quitOnServerDisconnection();
        void handleManyDeletesPresentationRequested();
        void openMainWindow();
        void openOnboardingFromHome();
        void handleConfiguredSyncsChanged();
        [[nodiscard]] bool hasConfiguredSyncs() const { return !_appCache.syncContexts().empty(); }

        IpcClient _ipcClient{this};
        SignalDispatcher _signalDispatcher{this};
        CommService _serverCommService{_ipcClient, _signalDispatcher, this};
        AppCache _appCache{this};
        ActivityStore _activityStore{this};
        ParametersStore _parametersStore{this};
        CachePipeline _cachePipeline{_serverCommService, _appCache, _activityStore, this};
        MainSelectionStore _mainSelectionStore{_appCache, this};
        MainSidebarController _mainSidebarController{_appCache, _mainSelectionStore, this};
        ParametersService _parametersService{_serverCommService, _parametersStore, this};
        ManyDeletesController _manyDeletesController{_serverCommService, _appCache, _parametersService, this};
        AppRouter _appRouter{this};
        ServiceActionTracker _serviceActionTracker{this};
        ServiceEventBus _serviceEventBus{this};
        ActivityService _activityService{_serverCommService, _serviceActionTracker, _serviceEventBus, this};
        SentryService _sentryService{_parametersService, _appCache, _parametersStore, this};
        CachePopulator _cachePopulator{_serverCommService, _appCache, _parametersStore, this};
        UserService _userService{_serverCommService, _appCache, _serviceActionTracker, _serviceEventBus, this};
        OnboardingSessionManager _onboardingSessionManager{_cachePopulator, _appCache,        _serverCommService,
                                                           _userService,    _serviceEventBus, this};
        DriveService _driveService{_serverCommService, _serviceActionTracker, _serviceEventBus, this};
        SyncService _syncService{_serverCommService, _serviceActionTracker, _serviceEventBus, this};
        WindowDecorationController _windowDecorationController{this};
        SystemTrayController _systemTrayController{this};
        NetworkStatusObserver _networkStatusObserver{this};
        HomeController _homeController{
                _appCache, _mainSelectionStore, _syncService, _appRouter, _systemTrayController, _networkStatusObserver, this};
        ActivitiesController _activitiesController{_activityStore,         _appCache,        _mainSelectionStore,
                                                   _networkStatusObserver, _activityService, this};
        QTranslator _baseTranslator{this};
        QTranslator _localizedTranslator{this};
        QQmlApplicationEngine _qmlEngine;
        bool _bootstrapCompleted{false};
        bool _mainWindowActivationPending{false};
        bool _mainWindowDismissedDuringBootstrap{false};
        bool _preferSetupHomeWhenUnconfigured{false};
        bool _hadConfiguredSync{false};
        bool _quitPending{false};
};

} // namespace KDC
