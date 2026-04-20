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
#include "app/models/drivelistmodel.h"
#include "app/models/synclistmodel.h"
#include "app/services/commservice.h"
#include "app/services/driveservice.h"
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
 * Owns the transport stack, the central app cache, and the cache pipeline.
 * On construction, sets up logging, wires IPC signals to dispatcher/cache,
 * and initiates the connection to the server.
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
        UserService _userService{_serverCommService, _appCache, this};
        DriveService _driveService{_serverCommService, _appCache, this};
        SyncService _syncService{_serverCommService, _appCache, this};
        DriveListModel _driveListModel{_appCache, this};
        SyncListModel _syncListModel{_appCache, this};
        QQmlApplicationEngine _qmlEngine;
};

} // namespace KDC
