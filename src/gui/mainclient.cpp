/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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

#include "appclient.h"
#include "config.h"
#include "version.h"
#include "cocoainitializer.h"
#include "libcommon/theme/theme.h"
#include "libcommon/utility/utility.h"
#include "libcommon/log/sentry/handler.h"
#include "libcommongui/utility/utility.h"

#include <QtGlobal>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QFileInfo>
#include <QDir>

#include <signal.h>
#include <iostream>
#include <fstream>

#ifdef Q_OS_UNIX
#include <sys/time.h>
#include <sys/resource.h>
#endif

Q_LOGGING_CATEGORY(lcMain, "gui.mainclient", QtInfoMsg)

void warnSystray() {
    QMessageBox::critical(0, qApp->translate("main.cpp", "System Tray not available"),
                          qApp->translate("main.cpp",
                                          "%1 requires on a working system tray. "
                                          "If you are running XFCE, please follow "
                                          "<a href=\"http://docs.xfce.org/xfce/xfce4-panel/systray\">these instructions</a>. "
                                          "Otherwise, please install a system tray application such as 'trayer' and try again.")
                                  .arg(QString::fromStdString(KDC::Theme::instance()->appName())));
}

void signalHandler(int signum) {
    KDC::SignalType signalType = KDC::fromInt<KDC::SignalType>(signum);
    std::cerr << "Client stopped with signal " << signalType << std::endl;

    KDC::CommonUtility::writeSignalFile(KDC::AppType::Client, signalType);

    exit(signum);
}

int main(int argc, char **argv) {
    // KDC::CommonUtility::handleSignals(signalHandler); // !!! The signal handler interferes with Sentry !!!

    std::cout << "kDrive client starting" << std::endl;

    // Working dir;
    KDC::CommonUtility::_workingDirPath = KDC::SyncPath(argv[0]).parent_path();
    KDC::sentry::Handler::init(KDC::AppType::Client);
    KDC::sentry::Handler::instance()->setGlobalConfidentialityLevel(KDC::sentry::ConfidentialityLevel::Authenticated);

#ifdef Q_OS_LINUX
    // Bug with multi-screen
    // cf. https://codereview.qt-project.org/c/qt/qtbase/+/200327
    qputenv("QT_RELY_ON_NET_WORKAREA_ATOM", "1");
#endif

    // qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "0");
    // qputenv("QT_SCALE_FACTOR", "1.2");

#ifdef Q_OS_WIN
    // If the font size ratio is set on Windows, we need to
    // enable the auto pixelRatio in Qt since we don't
    // want to use sizes relative to the font size everywhere.
    // This is automatic on OS X, but opt-in on Windows and Linux
    // https://doc-snapshots.qt.io/qt5-5.6/highdpi.html#qt-support
    // We do not define it on linux so the behaviour is kept the same
    // as other Qt apps in the desktop environment. (which may or may
    // not set this envoronment variable)
    if (!qEnvironmentVariableIsSet("QT_AUTO_SCREEN_SCALE_FACTOR")) {
        qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "1");
    }
#endif

#ifdef Q_OS_MAC
    Mac::CocoaInitializer cocoaInit; // RIIA
#endif

    Q_INIT_RESOURCE(client);

    std::unique_ptr<KDC::AppClient> appPtr = nullptr;
    try {
        appPtr = std::make_unique<KDC::AppClient>(argc, argv);
    } catch (const std::exception &e) {
        std::cerr << "kDrive client initialization error: " << e.what() << std::endl;
        return -1;
    }

#ifdef Q_OS_WIN
    // The Windows style still has pixelated elements with Qt 5.6,
    // it's recommended to use the Fusion style in this case, even
    // though it looks slightly less native. Check here after the
    // QApplication was constructed, but before any QWidget is
    // constructed.
    if (appPtr->devicePixelRatio() > 1) QApplication::setStyle(QStringLiteral("fusion"));
#endif

#ifdef Q_OS_UNIX
    // Increase the size limit of core dumps
    struct rlimit core_limit;
    core_limit.rlim_cur = RLIM_INFINITY;
    core_limit.rlim_max = RLIM_INFINITY;
    if (setrlimit(RLIMIT_CORE, &core_limit) < 0) {
        qCWarning(lcMain) << "Unable to set RLIMIT_CORE";
    }
#endif

    // if the application is already running, notify it.
    if (appPtr->isRunning()) {
        qCInfo(lcMain) << "Already running, exiting...";
        if (appPtr->isSessionRestored()) {
            // This call is mirrored with the one in Application::slotParseMessage
            qCInfo(lcMain) << "Session was restored, don't notify app!";
            return -1;
        }
        return 0;
    }

    // We can't call isSystemTrayAvailable with appmenu-qt5 begause it hides the systemtray (issue #4693)
    if (qgetenv("QT_QPA_PLATFORMTHEME") != "appmenu-qt5") {
        if (!QSystemTrayIcon::isSystemTrayAvailable()) {
            // If the systemtray is not there, we will wait one second for it to maybe start
            // (eg boot time) then we show the settings dialog if there is still no systemtray.
            // On XFCE however, we show a message box with explainaition how to install a systemtray.
            qCInfo(lcMain) << "System tray is not available, waiting...";
            KDC::CommonGuiUtility::sleep(1);

            auto desktopSession = qgetenv("XDG_CURRENT_DESKTOP").toLower();
            if (desktopSession.isEmpty()) {
                desktopSession = qgetenv("DESKTOP_SESSION").toLower();
            }
            if (desktopSession == "xfce") {
                int attempts = 0;
                while (!QSystemTrayIcon::isSystemTrayAvailable()) {
                    attempts++;
                    if (attempts >= 30) {
                        qCWarning(lcMain) << "System tray unavailable (xfce)";
                        warnSystray();
                        break;
                    }
                    KDC::CommonGuiUtility::sleep(1);
                }
            }

            if (QSystemTrayIcon::isSystemTrayAvailable()) {
                appPtr->onTryTrayAgain();
            } else if (desktopSession != "ubuntu") {
                qCInfo(lcMain) << "System tray still not available, showing window and trying again later";
                appPtr->showParametersDialog();
                QTimer::singleShot(10000, appPtr.get(), &KDC::AppClient::onTryTrayAgain);
            } else {
                qCInfo(lcMain) << "System tray still not available, but assuming it's fine on 'ubuntu' desktop";
            }
        }
    }

    return appPtr->exec();
}
