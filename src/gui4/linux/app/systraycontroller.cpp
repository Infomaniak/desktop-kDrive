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

#include "systraycontroller.h"

#include "app/cache/appcache.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QLoggingCategory>
#include <QWindow>
#include <QTimer>

#include <algorithm>

namespace KDC {

Q_LOGGING_CATEGORY(lcSystemTrayController, "gui.v4.systray", QtInfoMsg)

namespace {
constexpr uint8_t trayAvailabilityRetryLimit = 60;
constexpr int32_t trayAvailabilityRetryIntervalMs = 1000;

#ifdef QT_DEBUG
bool forceNoTrayRequested() {
    return qEnvironmentVariableIsSet("KDRIVE_FORCE_NO_TRAY");
}
#endif

QString toQString(const TrayIconState state) {
    switch (state) {
        using enum TrayIconState;
        case Neutral:
            return "Neutral";
        case Error:
            return "Error";
        case Notification:
            return "Notification";
        case Pause:
            return "Pause";
        case Sync:
            return "Sync";
    }

    return "Unknown";
}

QString trayIconPath(const TrayIconState state) {
    switch (state) {
        using enum TrayIconState;
        case Neutral:
            return ":/assets/tray/neutral.svg";
        case Error:
            return ":/assets/tray/error.svg";
        case Notification:
            return ":/assets/tray/notif.svg";
        case Pause:
            return ":/assets/tray/pause.svg";
        case Sync:
            return ":/assets/tray/sync.svg";
    }

    return ":/assets/tray/error.svg";
}

bool isPauseStatus(const SyncStatus status) {
    return status == SyncStatus::Paused || status == SyncStatus::Stopped;
}

bool isSyncStatus(const SyncStatus status) {
    return status == SyncStatus::Running;
}

} // namespace

SystemTrayController::SystemTrayController(QObject *const parent) :
    QObject(parent) {
    _trayAvailabilityRetryTimer.setSingleShot(true);
    _trayAvailabilityRetryTimer.setInterval(trayAvailabilityRetryIntervalMs);
    (void) connect(&_trayAvailabilityRetryTimer, &QTimer::timeout, this, &SystemTrayController::attemptTrayActivation);
}

void SystemTrayController::initialize() {
    if (_isInitialized) {
        qCWarning(lcSystemTrayController) << "System tray controller already initialized";
        return;
    }

#ifdef QT_DEBUG
    if (forceNoTrayRequested()) {
        qCWarning(lcSystemTrayController) << "Debug override active: forcing system tray to be unavailable";
        _isTrayAvailable = false;
    } else {
        _isTrayAvailable = QSystemTrayIcon::isSystemTrayAvailable();
    }
#else
    _isTrayAvailable = QSystemTrayIcon::isSystemTrayAvailable();
#endif
    qCInfo(lcSystemTrayController) << "Initializing system tray | available:" << _isTrayAvailable
                                   << "| state:" << toQString(_iconState) << "| icon:" << trayIconPath(_iconState);

    _trayIcon.setIcon(QIcon(trayIconPath(_iconState)));
    _trayIcon.setToolTip(tr("kDrive"));

    _openAction = _trayMenu.addAction(qtTrId("statusBarOpenApp"));
    _settingsAction = _trayMenu.addAction(qtTrId("statusBarSettings"));
    (void) _trayMenu.addSeparator();
    _quitAction = _trayMenu.addAction(qtTrId("statusBarQuitApp"));
    _trayIcon.setContextMenu(&_trayMenu);

    (void) connect(_openAction, &QAction::triggered, this, &SystemTrayController::openMainWindowRequested);
    (void) connect(_settingsAction, &QAction::triggered, this, &SystemTrayController::showSettingsWindow);
    (void) connect(_quitAction, &QAction::triggered, this, &SystemTrayController::quitRequested);
    (void) connect(&_trayIcon, &QSystemTrayIcon::activated, this, &SystemTrayController::onTrayActivated);

    if (_isTrayAvailable) {
        activateTrayMode();
    } else {
        qCWarning(lcSystemTrayController) << "System tray is not available at startup, using fallback window mode";
        startTrayAvailabilityRetry();
    }

    _isInitialized = true;
}

void SystemTrayController::observe(AppCache &appCache) {
    _appCache = &appCache;
    _hasSyncErrors = !_appCache->syncErrors().empty();

    (void) connect(&appCache, &AppCache::syncsChanged, this, [this] {
        qCDebug(lcSystemTrayController) << "Sync cache changed, refreshing tray icon state";
        refreshIconState();
    });
    (void) connect(&appCache, &AppCache::syncStatusChanged, this, [this](const SyncDbId) { refreshIconState(); });
    (void) connect(&appCache, &AppCache::syncErrorsChanged, this, [this] {
        qCDebug(lcSystemTrayController) << "Sync errors changed, refreshing tray icon state";
        _hasSyncErrors = !_appCache->syncErrors().empty();
        refreshIconState();
    });

    refreshIconState();
}

void SystemTrayController::setMainWindow(QWindow *const window) {
    _mainWindow = window;
    qCInfo(lcSystemTrayController) << "Main window registered | valid:" << !_mainWindow.isNull()
                                   << "| visible:" << (_mainWindow ? _mainWindow->isVisible() : false);

    if (!_isTrayModeActive && _mainWindow) {
        qCWarning(lcSystemTrayController) << "Showing main window because tray mode is not active";
        showMainWindow();
    }
}

void SystemTrayController::setProductStateInitialized(const bool initialized) {
    if (_isProductStateInitialized == initialized) {
        return;
    }

    qCInfo(lcSystemTrayController) << "System tray product state initialization changed | initialized:" << initialized;
    _isProductStateInitialized = initialized;
    refreshIconState();
}

void SystemTrayController::setNotificationActive(const bool active) {
    if (_isNotificationActive == active) {
        return;
    }

    qCInfo(lcSystemTrayController) << "Manual system tray notification state changed | active:" << active;
    _isNotificationActive = active;
    refreshIconState();
}

void SystemTrayController::setIconState(const TrayIconState state) {
    if (_iconState == state) {
        return;
    }

    qCInfo(lcSystemTrayController) << "System tray icon state changed | from:" << toQString(_iconState)
                                   << "| to:" << toQString(state) << "| icon:" << trayIconPath(state);
    _iconState = state;

    if (!_isInitialized) {
        return;
    }

    _trayIcon.setIcon(QIcon(trayIconPath(_iconState)));
}

void SystemTrayController::showMainWindow() const {
    if (_mainWindow == nullptr) {
        qCWarning(lcSystemTrayController) << "Cannot show main window: no window registered";
        return;
    }

    qCInfo(lcSystemTrayController) << "Showing main window from system tray";
    _mainWindow->show();
    _mainWindow->raise();
    _mainWindow->requestActivate();
}

void SystemTrayController::showSettingsWindow() {
    qCWarning(lcSystemTrayController) << "Settings window action triggered from system tray, but not implemented yet";
}

void SystemTrayController::hideMainWindow() const {
    if (_mainWindow == nullptr) {
        qCWarning(lcSystemTrayController) << "Cannot hide main window: no window registered";
        return;
    }

    if (!_isTrayModeActive) {
        qCWarning(lcSystemTrayController) << "Cannot hide main window in fallback mode, quitting application instead";
        QCoreApplication::quit();
        return;
    }

    qCInfo(lcSystemTrayController) << "Hiding main window instead of quitting";
    _mainWindow->hide();
}

void SystemTrayController::startTrayAvailabilityRetry() {
    if (_trayAvailabilityRetryTimer.isActive()) {
        return;
    }

#ifdef QT_DEBUG
    if (forceNoTrayRequested()) {
        qCInfo(lcSystemTrayController) << "Debug override keeps system tray unavailable";
    }
#endif
    _trayAvailabilityRetryCount = 0;
    qCInfo(lcSystemTrayController) << "Starting system tray availability retry loop | intervalMs:"
                                   << trayAvailabilityRetryIntervalMs << "| limit:" << trayAvailabilityRetryLimit;
    _trayAvailabilityRetryTimer.start();
}

void SystemTrayController::stopTrayAvailabilityRetry() {
    if (_trayAvailabilityRetryTimer.isActive()) {
        qCInfo(lcSystemTrayController) << "Stopping system tray availability retry loop";
        _trayAvailabilityRetryTimer.stop();
    }
    _trayAvailabilityRetryCount = 0;
}

void SystemTrayController::attemptTrayActivation() {
    if (_isTrayModeActive) {
        stopTrayAvailabilityRetry();
        return;
    }

#ifdef QT_DEBUG
    _isTrayAvailable = !forceNoTrayRequested() && QSystemTrayIcon::isSystemTrayAvailable();
#else
    _isTrayAvailable = QSystemTrayIcon::isSystemTrayAvailable();
#endif
    if (_isTrayAvailable) {
        qCInfo(lcSystemTrayController) << "System tray became available, activating tray mode";
        activateTrayMode();
        return;
    }

    ++_trayAvailabilityRetryCount;
    qCInfo(lcSystemTrayController) << "System tray still unavailable | retry:" << _trayAvailabilityRetryCount << "/"
                                   << trayAvailabilityRetryLimit;

    if (_trayAvailabilityRetryCount >= trayAvailabilityRetryLimit) {
        qCWarning(lcSystemTrayController) << "System tray unavailable after retry limit, staying in fallback window mode";
        stopTrayAvailabilityRetry();
        return;
    }

    _trayAvailabilityRetryTimer.start();
}

void SystemTrayController::activateTrayMode() {
    if (_isTrayModeActive) {
        return;
    }

    _isTrayAvailable = true;
    _isTrayModeActive = true;
    stopTrayAvailabilityRetry();
    _trayIcon.show();
    qCInfo(lcSystemTrayController) << "System tray mode activated | visible:" << _trayIcon.isVisible();
    emit trayModeActiveChanged(true);
}

void SystemTrayController::refreshIconState() {
    if (!_isProductStateInitialized || (_appCache == nullptr)) {
        setIconState(TrayIconState::Neutral);
        return;
    }

    const auto syncs = _appCache->syncs();
    if (std::ranges::any_of(syncs, [this](const BaseSync &sync) {
            const auto runtimeInfo = _appCache->syncRuntimeInfo(sync.dbId());
            return runtimeInfo.has_value() && isSyncStatus(runtimeInfo->status);
        })) {
        setIconState(TrayIconState::Sync);
        return;
    }

    if (_hasSyncErrors) {
        setIconState(TrayIconState::Error);
        return;
    }

    if (_isNotificationActive) {
        setIconState(TrayIconState::Notification);
        return;
    }

    if (!syncs.empty() && std::ranges::all_of(syncs, [this](const BaseSync &sync) {
            const auto runtimeInfo = _appCache->syncRuntimeInfo(sync.dbId());
            return runtimeInfo.has_value() && isPauseStatus(runtimeInfo->status);
        })) {
        setIconState(TrayIconState::Pause);
        return;
    }

    setIconState(TrayIconState::Neutral);
}

void SystemTrayController::onTrayActivated(const QSystemTrayIcon::ActivationReason reason) {
    qCInfo(lcSystemTrayController) << "System tray activated | reason:" << reason;

    if (!_isTrayModeActive) {
        qCWarning(lcSystemTrayController) << "Tray activation ignored because tray mode is not active";
        return;
    }

    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::MiddleClick) {
        emit openMainWindowRequested();
    }
}

} // namespace KDC
