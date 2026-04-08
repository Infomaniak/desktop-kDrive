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

#include <QDir>
#include <QGuiApplication>
#include <QLocale>
#include <QScreen>
#include <QSysInfo>

#include <cstdlib>
#include <thread>
#include <unistd.h>

namespace KDC {

Q_LOGGING_CATEGORY(lcAppClientLinux, "gui.v4.app", QtInfoMsg)

AppClientLinux::AppClientLinux(int &argc, char **argv) :
    QGuiApplication(argc, argv) {
    setupLogging();

    qCInfo(lcAppClientLinux) << "Linux v4 GUI bootstrap started";

    (void) connect(&_ipcClient, &IpcClient::connected, this, &AppClientLinux::ipcConnected);
    (void) connect(&_ipcClient, &IpcClient::disconnected, this, &AppClientLinux::ipcDisconnected);
    (void) connect(&_ipcClient, &IpcClient::serverSignalReceived, &_signalDispatcher, &SignalDispatcher::dispatch);
    (void) connect(this, &QCoreApplication::aboutToQuit, this, [] { qCInfo(lcAppClientLinux) << "Qt aboutToQuit emitted"; });

    qCDebug(lcAppClientLinux) << "IPC signal wiring initialized";


#ifdef QT_DEBUG
    qCInfo(lcAppClientLinux) << "Starting initial IPC connection";
    _ipcClient.connectToServer();
#else
    qCCritical(lcAppClientLinux) << "Release mode not already supported.";
    std::exit(EXIT_FAILURE);
#endif
}

void AppClientLinux::setupLogging() const {
    auto *const logger = Logger::instance();
    logger->setIsClientLog(true);
    logger->setLogDebug(true);
    logger->setupTemporaryFolderLogDir();
    logger->enterNextLogFile();
    // TODO: Set the minimum log level from parameters once the parameters cache is available (Logger::minLogLevel)

    qCInfo(lcAppClientLinux) << "***** Application & System Informations *****";
    qCInfo(lcAppClientLinux) << "app version:" << CommonUtility::currentVersion();
    qCInfo(lcAppClientLinux) << "version tag:" << CommonUtility::versionTag();
    qCInfo(lcAppClientLinux) << "version build:" << CommonUtility::versionBuild();
    qCInfo(lcAppClientLinux) << "log directory:" << logger->temporaryFolderLogDirPath();
    qCInfo(lcAppClientLinux) << "executable path:" << QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    qCInfo(lcAppClientLinux) << "application dir:" << QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
    qCInfo(lcAppClientLinux) << "working directory:" << QDir::toNativeSeparators(QDir::currentPath());
    qCInfo(lcAppClientLinux) << "client pid:" << QCoreApplication::applicationPid();
    qCInfo(lcAppClientLinux) << "user:" << qEnvironmentVariable("USER") << "| logname:" << qEnvironmentVariable("LOGNAME");
    qCInfo(lcAppClientLinux) << "uid:" << getuid() << "| euid:" << geteuid();
    qCInfo(lcAppClientLinux) << "home directory:" << QDir::toNativeSeparators(QDir::homePath());
    qCInfo(lcAppClientLinux) << "os:" << CommonUtility::platformName();
    qCInfo(lcAppClientLinux) << "os version:" << CommonUtility::osVersion();
    qCInfo(lcAppClientLinux) << "kernel type/version:" << QSysInfo::kernelType() << QSysInfo::kernelVersion();
    qCInfo(lcAppClientLinux) << "cpu architecture:" << QSysInfo::currentCpuArchitecture();
    qCInfo(lcAppClientLinux) << "# of logical CPU cores:" << std::thread::hardware_concurrency();
    qCInfo(lcAppClientLinux) << "locale:" << QLocale::system().name();

    qCInfo(lcAppClientLinux) << "display server:" << qEnvironmentVariable("XDG_SESSION_TYPE");
    qCInfo(lcAppClientLinux) << "display (X11):" << qEnvironmentVariable("DISPLAY");
    qCInfo(lcAppClientLinux) << "display (Wayland):" << qEnvironmentVariable("WAYLAND_DISPLAY");
    qCInfo(lcAppClientLinux) << "desktop environment:" << qEnvironmentVariable("XDG_CURRENT_DESKTOP");

    qCInfo(lcAppClientLinux) << "Qt version:" << qVersion();
    qCInfo(lcAppClientLinux) << "Qt platform:" << this->platformName();
    if (qEnvironmentVariableIsSet("QT_SCALE_FACTOR"))
        qCInfo(lcAppClientLinux) << "Qt scale factor:" << qEnvironmentVariable("QT_SCALE_FACTOR");
    if (qEnvironmentVariableIsSet("QT_AUTO_SCREEN_SCALE_FACTOR"))
        qCInfo(lcAppClientLinux) << "Qt auto screen scale:" << qEnvironmentVariable("QT_AUTO_SCREEN_SCALE_FACTOR");
    if (qEnvironmentVariableIsSet("QT_FONT_DPI"))
        qCInfo(lcAppClientLinux) << "Qt font DPI:" << qEnvironmentVariable("QT_FONT_DPI");

    const auto screens = QGuiApplication::screens();
    qCInfo(lcAppClientLinux) << "# of screens:" << screens.size();
    for (const auto *screen: screens) {
        qCInfo(lcAppClientLinux) << "  screen:" << screen->name();
        qCInfo(lcAppClientLinux) << "  - resolution:" << screen->size();
        qCInfo(lcAppClientLinux) << "  - physical DPI:" << screen->physicalDotsPerInch();
        qCInfo(lcAppClientLinux) << "  - logical DPI:" << screen->logicalDotsPerInch();
        qCInfo(lcAppClientLinux) << "  - scale factor:" << screen->devicePixelRatio();
    }
    qCInfo(lcAppClientLinux) << "********************";
}

} // namespace KDC
