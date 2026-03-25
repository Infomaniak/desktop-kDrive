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

#include "AppClientLinux.h"

#include "libcommongui/logger.h"

#include <cstdlib>

namespace KDC {

Q_LOGGING_CATEGORY(lcAppClientLinux, "gui.v4.app", QtInfoMsg)

AppClientLinux::AppClientLinux(int &argc, char **argv) :
    QGuiApplication(argc, argv) {
    setupLogging();

    connect(&_ipcClient, &IpcClient::connected, this, &AppClientLinux::ipcConnected);
    connect(&_ipcClient, &IpcClient::disconnected, this, &AppClientLinux::ipcDisconnected);
    connect(&_ipcClient, &IpcClient::messageReceived, this, &AppClientLinux::ipcMessageReceived);

#ifdef QT_DEBUG
    _ipcClient.connectToServer();
#else
    qCCritical(lcAppClientLinux) << "Release mode not already supported.";
    std::exit(EXIT_FAILURE);
#endif
}

void AppClientLinux::setupLogging() {
    auto *const logger = Logger::instance();
    logger->setIsCLientLog(true);
    logger->setLogDebug(true);
    logger->setupTemporaryFolderLogDir();
    logger->enterNextLogFile();
}

} // namespace KDC
