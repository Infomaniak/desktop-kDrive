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

#include "app/applicationidentity.h"
#include "libcommon/utility/utility.h"
#include "libcommongui/logger.h"

#include <Poco/Dynamic/Struct.h>

#include <QCoreApplication>
#include <QDir>
#include <QLocale>
#include <QQmlContext>
#include <QQmlError>
#include <QScreen>
#include <QStringList>
#include <QSysInfo>
#include <QTimer>
#include <QTranslator>
#include <QVariant>
#include <QWindow>
#include <QQmlEngine>

#include <chrono>
#include <thread>
#include <unistd.h>

namespace KDC {

Q_LOGGING_CATEGORY(lcAppClientLinux, "gui.v4.app", QtInfoMsg)

AppClientLinux::AppClientLinux(int &argc, char **argv) :
    QApplication(argc, argv) {
    setupLogging();
    setupTranslations();
    setQuitOnLastWindowClosed(false);
    QIcon appIcon;
    ApplicationIdentity::configureApplication(appIcon);

    qCInfo(lcAppClientLinux) << "Linux v4 GUI bootstrap started";
    _systemTrayController.initialize();
    _systemTrayController.observe(_appCache);

    (void) connect(&_ipcClient, &IpcClient::connected, this, &AppClientLinux::ipcConnected);
    (void) connect(&_ipcClient, &IpcClient::disconnected, this, &AppClientLinux::ipcDisconnected);
    (void) connect(&_ipcClient, &IpcClient::serverSignalReceived, &_signalDispatcher, &SignalDispatcher::dispatch);
    (void) connect(this, &AppClientLinux::ipcDisconnected, &_appCache, [this] {
        // Normal UTILITY_QUIT paths stop the application before the server closes the socket. This cleanup therefore runs only
        // when an established IPC connection is lost unexpectedly.
        _systemTrayController.setProductStateInitialized(false);
        _appRouter.hideMainWindow();
        _appCache.clearAll();
        _parametersStore.clear();
    });
    (void) connect(&_cachePopulator, &CachePopulator::bootstrapCompleted, &_cachePipeline, &CachePipeline::markPopulated);
    (void) connect(&_cachePopulator, &CachePopulator::bootstrapCompleted, &_systemTrayController,
                   [this] { _systemTrayController.setProductStateInitialized(true); });
    (void) connect(&_cachePopulator, &CachePopulator::bootstrapCompleted, &_sentryService,
                   &SentryService::updateAuthenticatedUser);
    (void) connect(&_cachePopulator, &CachePopulator::bootstrapCompleted, this, [this] {
        if (_systemTrayController.trayModeActive() || _appCache.syncContexts().empty()) {
            return;
        }

        qCInfo(lcAppClientLinux) << "Opening main window after bootstrap because tray fallback mode is active";
        openMainWindow();
    });
    (void) connect(this, &AppClientLinux::ipcConnected, this, [this] { _cachePopulator.bootstrap(); });
    (void) connect(this, &QCoreApplication::aboutToQuit, this, [] { qCInfo(lcAppClientLinux) << "Qt aboutToQuit emitted"; });
    (void) connect(&_serverCommService, &CommService::showSettings, this, &AppClientLinux::openMainWindow);
    (void) connect(&_serverCommService, &CommService::showSynthesis, this, &AppClientLinux::openMainWindow);
    (void) connect(&_serverCommService, &CommService::quit, this, [] { QCoreApplication::quit(); });
    (void) connect(&_onboardingSessionManager, &OnboardingSessionManager::openOnboardingWindowRequested, &_systemTrayController,
                   &SystemTrayController::showMainWindow);
    (void) connect(&_onboardingSessionManager, &OnboardingSessionManager::closeOnboardingWindowRequested, &_systemTrayController,
                   &SystemTrayController::hideMainWindow);
    (void) connect(&_onboardingSessionManager, &OnboardingSessionManager::onboardingCompleted, this,
                   [this] { QTimer::singleShot(0, this, &AppClientLinux::openMainWindow); });
    (void) connect(&_systemTrayController, &SystemTrayController::openMainWindowRequested, this, &AppClientLinux::openMainWindow);
    (void) connect(&_appCache, &AppCache::usersChanged, &_sentryService, &SentryService::updateAuthenticatedUser);
    (void) connect(&_parametersStore, &ParametersStore::parametersInfoChanged, this, [this] {
        const auto parametersInfo = _parametersStore.parametersInfo();
        if (!parametersInfo.has_value()) {
            return;
        }

        Logger::instance()->setMinLogLevel(toInt(parametersInfo->logLevel()));
        qCInfo(lcAppClientLinux) << "Logger minimum level updated from parameters | level:"
                                 << QString::fromStdString(toString(parametersInfo->logLevel()));
    });
    (void) connect(&_systemTrayController, &SystemTrayController::quitRequested, this, [this] {
        qCInfo(lcAppClientLinux) << "Quit requested from system tray";
        if (!_ipcClient.isConnected()) {
            qCWarning(lcAppClientLinux) << "IPC is not connected, quitting application directly";
            quit();
            return;
        }
        _serverCommService.requestQuit([](const auto &exitInfo) {
            if (!exitInfo) {
                qCWarning(lcAppClientLinux) << "Server quit request failed";
            }
            quit();
        });
    });

    _qmlEngine.rootContext()->setContextProperty(QStringLiteral("appCache"), &_appCache);
    _qmlEngine.rootContext()->setContextProperty(QStringLiteral("parametersStore"), &_parametersStore);
    _qmlEngine.rootContext()->setContextProperty(QStringLiteral("userService"), &_userService);
    _qmlEngine.rootContext()->setContextProperty(QStringLiteral("driveService"), &_driveService);
    _qmlEngine.rootContext()->setContextProperty(QStringLiteral("syncService"), &_syncService);
    _qmlEngine.rootContext()->setContextProperty(QStringLiteral("serviceEventBus"), &_serviceEventBus);
    _qmlEngine.rootContext()->setContextProperty(QStringLiteral("windowDecorationController"), &_windowDecorationController);
    (void) qmlRegisterUncreatableType<AppRouter>("kDrive.UI", 1, 0, "AppRouter",
                                                 "AppRouter is owned by AppClientLinux and exposed as appRouter.");
    (void) qmlRegisterUncreatableType<SyncSelectorModel>("kDrive.UI", 1, 0, "SyncSelectorModel",
                                                        "SyncSelectorModel is owned by MainSidebarController.");
    _qmlEngine.setOutputWarningsToStandardError(false);
    (void) connect(&_qmlEngine, &QQmlApplicationEngine::warnings, this, [](const QList<QQmlError> &warnings) {
        for (const auto &warning: warnings) {
            qCWarning(lcAppClientLinux) << "QML warning:" << warning.toString();
        }
    });
    _qmlEngine.setInitialProperties({
            {QStringLiteral("appRouter"), QVariant::fromValue<QObject *>(&_appRouter)},
            {QStringLiteral("mainSidebarController"), QVariant::fromValue<QObject *>(&_mainSidebarController)},
            {QStringLiteral("onboardingSessionManager"), QVariant::fromValue<QObject *>(&_onboardingSessionManager)},
            {QStringLiteral("systemTrayController"), QVariant::fromValue<QObject *>(&_systemTrayController)},
    });
    _qmlEngine.loadFromModule(QStringLiteral("kDrive.UI"), QStringLiteral("Main"));
    if (_qmlEngine.rootObjects().isEmpty()) {
        qCCritical(lcAppClientLinux) << "QML root object creation failed";
        SentryService::reportFatalAndExit("QML root object creation failed", "QQmlApplicationEngine returned no root object.");
    }
    auto *const mainWindow = qobject_cast<QWindow *>(_qmlEngine.rootObjects().constFirst());
    if (mainWindow == nullptr) {
        qCCritical(lcAppClientLinux) << "QML root object is not a window";
        SentryService::reportFatalAndExit("QML root object is not a window",
                                          "The first QML root object is not a QWindow instance.");
    }
    mainWindow->setIcon(appIcon);
    _systemTrayController.setMainWindow(mainWindow);

    qCDebug(lcAppClientLinux) << "IPC/cache/QML wiring initialized";

    if (const QStringList arguments = QCoreApplication::arguments(); arguments.size() > 1) {
        bool isValidPort = false;
        const quint16 port = arguments[1].toUShort(&isValidPort);
        if (!isValidPort || port == 0) {
            SentryService::reportFatalAndExit(QStringLiteral("Invalid server communication port argument"),
                                              QStringLiteral("Received command-line value: %1").arg(arguments[1]));
        }

        qCInfo(lcAppClientLinux) << "Starting initial IPC connection on configured port=" << port;
        _ipcClient.connectToServer(port);
    } else {
#ifdef QT_DEBUG
        qCInfo(lcAppClientLinux) << "Starting initial IPC connection using the debug .comm file";
        _ipcClient.connectToServer();
#else
        SentryService::reportFatalAndExit("Missing server communication port argument",
                                          "Release and RelWithDebInfo clients must be launched by the server with its TCP port.");
#endif
    }
}

void AppClientLinux::setupTranslations() {
    // Catalogs are id-based: qsTrId(id) returns the raw id when no translation is loaded. Install
    // client_en as a base so every id always resolves (keys not yet translated degrade to English),
    // then overlay the system locale on top. Qt queries translators last-installed-first, so the
    // locale wins where it has a translation and falls back to the English base otherwise.
    if (_baseTranslator.load(QStringLiteral("client_en"), QStringLiteral(":/i18n"))) {
        (void) installTranslator(&_baseTranslator);
    } else {
        qCWarning(lcAppClientLinux) << "base English translation catalog missing; UI may show source ids";
    }

    if (const QLocale locale = QLocale::system();
        _localizedTranslator.load(locale, QStringLiteral("client"), QStringLiteral("_"), QStringLiteral(":/i18n"))) {
        (void) installTranslator(&_localizedTranslator);
        qCInfo(lcAppClientLinux) << "translations loaded for locale" << locale.name();
    } else {
        qCInfo(lcAppClientLinux) << "no catalog for locale" << locale.name() << "- using English base";
    }
}

void AppClientLinux::openMainWindow() {
    if (_appCache.syncContexts().empty()) {
        _onboardingSessionManager.openOnboardingWindow();
        return;
    }

    _mainSelectionStore.ensureValidSelection();
    _appRouter.showMainWindow();
    _systemTrayController.showMainWindow();
    _serverCommService.requestActivateLoadInfo([](const ExitInfo &exitInfo) {
        if (!exitInfo) {
            qCWarning(lcAppClientLinux) << "Main window live info refresh failed | code:" << exitInfo.code()
                                        << "/ cause:" << exitInfo.cause();
        }
    });
    qCInfo(lcAppClientLinux) << "Main window opened"
                             << "| configuredDrives:" << _appCache.driveContexts().size()
                             << "| configuredSyncs:" << _appCache.syncContexts().size()
                             << "| currentSyncDbId:" << _mainSelectionStore.currentSyncDbId();
}

void AppClientLinux::setupLogging() {
    auto *const logger = Logger::instance();
    logger->setIsClientLog(true);
    logger->setLogDebug(true);
    logger->setupLogDir();
    logger->setLogExpire(std::chrono::days(CommonUtility::logsPurgeRate));
    logger->enterNextLogFile();

    qCInfo(lcAppClientLinux) << "***** Application & System Informations *****";
    qCInfo(lcAppClientLinux) << "app version:" << CommonUtility::currentVersion();
    qCInfo(lcAppClientLinux) << "version tag:" << CommonUtility::versionTag();
    qCInfo(lcAppClientLinux) << "version build:" << CommonUtility::versionBuild();
    qCInfo(lcAppClientLinux) << "log directory:" << logger->logDirectoryPath();
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
    qCInfo(lcAppClientLinux) << "Qt platform:" << platformName();
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
