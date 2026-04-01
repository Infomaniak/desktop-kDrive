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

#include "app/services/servercommservice.h"
#include "communicationlayer/ipcclient.h"
#include "communicationlayer/signaldispatcher.h"

#include <QGuiApplication>
#include <QLoggingCategory>

namespace KDC {

Q_DECLARE_LOGGING_CATEGORY(lcAppClientLinux)

/**
 * Top-level application object for the Linux GUI client.
 *
 * Owns the IPC client and the signal dispatcher. On construction, sets up logging,
 * wires IPC signals to the dispatcher, and initiates the connection to the server.
 */
class AppClientLinux : public QGuiApplication {
        Q_OBJECT

    public:
        explicit AppClientLinux(int &argc, char **argv);

        /**
         * Provides access to the signal dispatcher for registering server signal handlers.
         * Call registerHandler() on the returned dispatcher before the IPC connection is established.
         */
        SignalDispatcher &signalDispatcher() { return _signalDispatcher; }
        ServerCommService &serverCommService() { return _serverCommService; }

    signals:
        /** Emitted once the first IPC connection to the server has been successfully established. */
        void ipcConnected();
        /** Emitted when the IPC connection is lost after having been established. Considered fatal. */
        void ipcDisconnected();

    private:
        static void setupLogging();

        IpcClient _ipcClient{this};
        SignalDispatcher _signalDispatcher{this};
        ServerCommService _serverCommService{_signalDispatcher, this};
};

} // namespace KDC