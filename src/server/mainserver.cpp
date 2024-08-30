/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

#include "appserver.h"
#include "config.h"
#include "version.h"
#include "common/utility.h"
#include "libcommon/asserts.h"
#include "updater/updaterserver.h"
#include "libcommon/utility/utility.h"
#include "libcommon/log/sentry/sentryhandler.h"
#include "libcommonserver/log/log.h"

#include <QtGlobal>
#include <QTimer>
#include <QFileInfo>
#include <QDir>
#include <QFile>

#include <signal.h>
#include <iostream>

#ifdef Q_OS_UNIX
#include <sys/time.h>
#include <sys/resource.h>
#endif

#include <log4cplus/loggingmacros.h>

#define APP_RLIMIT_NOFILE 0x100000

#if defined(Q_OS_WIN)
// For Windows, stack size is defined at link time (see CMakeLists)
#elif defined(Q_OS_MAC)
#define APP_RLIMIT_STACK 0x3FF0000
#else
#define APP_RLIMIT_STACK 0x4000000
#endif

void signalHandler(int signum) {
    std::cerr << "Server stoped with signal " << signum << std::endl;

    // Make sure everything flushes
#ifdef NDEBUG
    auto sentryClose = qScopeGuard([] { sentry_close(); });
#endif

    exit(signum);
}

int main(int argc, char **argv) {
#ifndef Q_OS_WIN
    signal(SIGABRT, signalHandler);
    signal(SIGKILL, signalHandler);
    signal(SIGBUS, signalHandler);
    signal(SIGSEGV, signalHandler);

    signal(SIGPIPE, SIG_IGN);
#endif

    std::cout << "kDrive server starting" << std::endl;

    // Working dir;
    KDC::CommonUtility::_workingDirPath = KDC::SyncPath(argv[0]).parent_path();
    KDC::SentryHandler::init(KDC::SentryHandler::SentryProject::Server);
    KDC::SentryHandler::instance()->setGlobalConfidentialityLevel(KDC::SentryConfidentialityLevel::Authenticated); 

    Q_INIT_RESOURCE(client);

    std::unique_ptr<KDC::AppServer> appPtr = nullptr;
    try {
        appPtr = std::unique_ptr<KDC::AppServer>(new KDC::AppServer(argc, argv));
    } catch (std::exception const &e) {
        std::cerr << "kDrive server initialization error: " << e.what() << std::endl;
        return -1;
    }

    if (appPtr->helpAsked()) {
        appPtr->showHelp();
        return 0;
    }

    if (appPtr->versionAsked()) {
        appPtr->showVersion();
        return 0;
    }

    if (appPtr->clearSyncNodesAsked()) {
        appPtr->clearSyncNodes();
        return 0;
    }

    if (appPtr->clearKeychainKeysAsked()) {
        appPtr->clearKeychainKeys();
        return 0;
    }

#if defined(Q_OS_MAC) && defined(NDEBUG)
    // Copy Uninstaller inside application folder
    try {
        KDC::SyncPath uninstallerPath = "/Applications/kDrive/kDrive.app/Contents/Frameworks/kDrive Uninstaller.app";
        KDC::SyncPath destPath = "/Applications/kDrive/kDrive Uninstaller.app";
        std::filesystem::copy(uninstallerPath, destPath,
                              std::filesystem::copy_options::overwrite_existing | std::filesystem::copy_options::recursive);
        // LOGW_INFO(KDC::Log::instance()->getLogger(), L"Uninstaller copied to: " << Path2WStr(destPath).c_str());
    } catch (std::filesystem::filesystem_error &fsError) {
        LOG_ERROR(KDC::Log::instance()->getLogger(), "Failed to copy uninstaller: " << fsError.what());
    } catch (...) {
        LOG_ERROR(KDC::Log::instance()->getLogger(), "Failed to copy uninstaller: nnknown error");
    }
#endif

#ifdef Q_OS_UNIX
    // Increase the size limit of core dumps
    struct rlimit coreLimit;
    coreLimit.rlim_cur = RLIM_INFINITY;
    coreLimit.rlim_max = RLIM_INFINITY;
    if (setrlimit(RLIMIT_CORE, &coreLimit) < 0) {
        LOG_WARN(KDC::Log::instance()->getLogger(), "Unable to set RLIMIT_CORE");
    }

    // Increase the limit of simultaneously open files
    struct rlimit numFiles;
    if (getrlimit(RLIMIT_NOFILE, &numFiles) == 0 && numFiles.rlim_cur < APP_RLIMIT_NOFILE) {
        numFiles.rlim_cur = qMin(rlim_t(APP_RLIMIT_NOFILE), numFiles.rlim_max);
        if (setrlimit(RLIMIT_NOFILE, &numFiles) < 0) {
            LOG_WARN(KDC::Log::instance()->getLogger(), "Unable to set RLIMIT_NOFILE - err =" << errno);
        }
    } else {
        LOG_WARN(KDC::Log::instance()->getLogger(), "Unable to get RLIMIT_NOFILE - err=" << errno);
    }

    // Increase the limit of stack size
    struct rlimit stackSize;
    if (getrlimit(RLIMIT_STACK, &stackSize) == 0 && numFiles.rlim_cur < APP_RLIMIT_STACK) {
        stackSize.rlim_cur = qMin(rlim_t(APP_RLIMIT_STACK), numFiles.rlim_max);
        if (setrlimit(RLIMIT_STACK, &stackSize) < 0) {
            LOG_WARN(KDC::Log::instance()->getLogger(), "Unable to set RLIMIT_STACK - err =" << errno);
        }
    } else {
        LOG_WARN(KDC::Log::instance()->getLogger(), "Unable to get RLIMIT_STACK - err=" << errno);
    }
#endif

    // If the application is already running, notify it.
    if (appPtr->isRunning()) {
        LOG_INFO(KDC::Log::instance()->getLogger(), "Server already running");
        if (appPtr->isSessionRestored()) {
            // This call is mirrored with the one in Application::slotParseMessage
            LOG_DEBUG(KDC::Log::instance()->getLogger(), "Session was restored, don't notify app!");
            return -1;
        }

        if (appPtr->settingsAsked()) {
            appPtr->sendShowSettingsMsg();
            return 0;
        }

        if (appPtr->synthesisAsked()) {
            appPtr->sendShowSynthesisMsg();
            return 0;
        }

        appPtr->showAlreadyRunning();
        return 0;
    }

    // If handleStartup returns true, main() needs to terminate here, e.g. because the updater is triggered
    KDC::UpdaterServer *updater = KDC::UpdaterServer::instance();
    if (updater && updater->handleStartup()) {
        LOG_INFO(KDC::Log::instance()->getLogger(), "Update in progress, exiting...");
        return 1;
    }

    return appPtr->exec();
}
