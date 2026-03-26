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

#include "appclientlinux.h"

#include "libcommongui/logger.h"
#include "libcommon/theme/theme.h"
#include "libcommon/utility/utility.h"

#include <Poco/Dynamic/Struct.h>

#include <QGuiApplication>
#include <QLocale>
#include <QScreen>
#include <QSysInfo>

#include <cstdlib>
#include <thread>

namespace KDC {

Q_LOGGING_CATEGORY(lcAppClientLinux, "gui.v4.app", QtInfoMsg)

AppClientLinux::AppClientLinux(int &argc, char **argv) :
    QGuiApplication(argc, argv) {
    setupLogging();

    (void) connect(&_ipcClient, &IpcClient::connected, this, &AppClientLinux::ipcConnected);
    (void) connect(&_ipcClient, &IpcClient::disconnected, this, &AppClientLinux::ipcDisconnected);
    (void) connect(&_ipcClient, &IpcClient::serverSignalReceived, &_signalDispatcher, &SignalDispatcher::dispatch);

    (void) connect(this, &AppClientLinux::ipcConnected, this, [this] {
        qCDebug(lcAppClientLinux) << "IPC connected - sending " << RequestNum::USER_INFOLIST<< " request";
        (void) _ipcClient.sendRequest(RequestNum::USER_INFOLIST, {}, [](const ExitInfo &exitInfo, const Poco::DynamicStruct &) {
            qCDebug(lcAppClientLinux) << "USER_INFOLIST response | exitCode:" << exitInfo.code()
                                      << "exitCause:" << exitInfo.cause(); // callbacks test
        });
    });

#ifdef QT_DEBUG
    _ipcClient.connectToServer();
#else
    qCCritical(lcAppClientLinux) << "Release mode not already supported.";
    std::exit(EXIT_FAILURE);
#endif
}

void AppClientLinux::setupLogging() {
    auto *const logger = Logger::instance();
    logger->setIsClientLog(true);
    logger->setLogDebug(true);
    logger->setupTemporaryFolderLogDir();
    logger->enterNextLogFile();

    qInfo(lcAppClientLinux) << "***** Application & System Informations *****";
    qInfo(lcAppClientLinux) << "os:" << CommonUtility::platformName();
    qInfo(lcAppClientLinux) << "os version:" << CommonUtility::osVersion().c_str();
    qInfo(lcAppClientLinux) << "kernel version:" << QSysInfo::kernelVersion();
    qInfo(lcAppClientLinux) << "kernel type:" << QSysInfo::kernelType();
    qInfo(lcAppClientLinux) << "cpu architecture:" << QSysInfo::currentCpuArchitecture();
    qInfo(lcAppClientLinux) << "# of logical CPU cores:" << std::thread::hardware_concurrency();
    qInfo(lcAppClientLinux) << "locale:" << QLocale::system().name();

    qInfo(lcAppClientLinux) << "display server:" << qEnvironmentVariable("XDG_SESSION_TYPE");
    qInfo(lcAppClientLinux) << "display (X11):" << qEnvironmentVariable("DISPLAY");
    qInfo(lcAppClientLinux) << "display (Wayland):" << qEnvironmentVariable("WAYLAND_DISPLAY");
    qInfo(lcAppClientLinux) << "desktop environment:" << qEnvironmentVariable("XDG_CURRENT_DESKTOP");

    qInfo(lcAppClientLinux) << "Qt version:" << qVersion();
    qInfo(lcAppClientLinux) << "Qt platform:" << qEnvironmentVariable("QT_QPA_PLATFORM");
    if (qEnvironmentVariableIsSet("QT_SCALE_FACTOR"))
        qInfo(lcAppClientLinux) << "Qt scale factor:" << qEnvironmentVariable("QT_SCALE_FACTOR");
    if (qEnvironmentVariableIsSet("QT_AUTO_SCREEN_SCALE_FACTOR"))
        qInfo(lcAppClientLinux) << "Qt auto screen scale:" << qEnvironmentVariable("QT_AUTO_SCREEN_SCALE_FACTOR");
    if (qEnvironmentVariableIsSet("QT_FONT_DPI"))
        qInfo(lcAppClientLinux) << "Qt font DPI:" << qEnvironmentVariable("QT_FONT_DPI");

    const auto screens = QGuiApplication::screens();
    qInfo(lcAppClientLinux) << "# of screens:" << screens.size();
    for (const auto *screen: screens) {
        qInfo(lcAppClientLinux) << "  screen:" << screen->name();
        qInfo(lcAppClientLinux) << "  - resolution:" << screen->size();
        qInfo(lcAppClientLinux) << "  - physical DPI:" << screen->physicalDotsPerInch();
        qInfo(lcAppClientLinux) << "  - logical DPI:" << screen->logicalDotsPerInch();
        qInfo(lcAppClientLinux) << "  - scale factor:" << screen->devicePixelRatio();
    }
    qInfo(lcAppClientLinux) << "********************";

}

} // namespace KDC
