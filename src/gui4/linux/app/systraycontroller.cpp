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
#include "app/services/commservice.h"

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
#else
bool forceNoTrayRequested() {
    return false;
}
#endif

QString syncStatusLogName(const SyncStatus status) {
    return QString::fromStdString(toString(status));
}

const char *trayIconStateLogName(const TrayIconState state) {
    switch (state) {
        case TrayIconState::Neutral:
            return "Neutral";
        case TrayIconState::Error:
            return "Error";
        case TrayIconState::Notification:
            return "Notification";
        case TrayIconState::Pause:
            return "Pause";
        case TrayIconState::Sync:
            return "Sync";
    }

    return "Unknown";
}

const char *trayIconPath(const TrayIconState state) {
    switch (state) {
        case TrayIconState::Neutral:
            return ":/assets/tray/neutral.svg";
        case TrayIconState::Error:
            return ":/assets/tray/error.svg";
        case TrayIconState::Notification:
            return ":/assets/tray/notif.svg";
        case TrayIconState::Pause:
            return ":/assets/tray/pause.svg";
        case TrayIconState::Sync:
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

SystemTrayController::SystemTrayController(QObject *parent) :
    QObject(parent) {
    _trayAvailabilityRetryTimer.setParent(this);
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
                                   << "| state:" << trayIconStateLogName(_iconState) << "| icon:" << trayIconPath(_iconState);

    _trayIcon.setIcon(QIcon(QString::fromUtf8(trayIconPath(_iconState))));
    _trayIcon.setToolTip(tr("kDrive"));

    _openAction = _trayMenu.addAction(tr("Open kDrive"));
    _settingsAction = _trayMenu.addAction(tr("Settings"));
    (void) _trayMenu.addSeparator();
    _quitAction = _trayMenu.addAction(tr("Quit kDrive"));
    _trayIcon.setContextMenu(&_trayMenu);

    (void) connect(_openAction, &QAction::triggered, this, &SystemTrayController::showMainWindow);
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

void SystemTrayController::observe(AppCache &appCache, CommService &commService) {
    _appCache = &appCache;

    (void) connect(&appCache, &AppCache::syncsChanged, this, [this] {
        qCDebug(lcSystemTrayController) << "Sync cache changed, refreshing tray icon state";
        reconcileKnownSyncStatuses();
        refreshIconState();
    });
    (void) connect(&appCache, &AppCache::syncErrorsChanged, this, [this] {
        qCDebug(lcSystemTrayController) << "Sync errors changed, refreshing tray icon state";
        refreshIconState();
    });
    (void) connect(&commService, &CommService::syncProgressInfo, this,
                   [this](const SyncDbId syncDbId, const SyncStatus status, SyncStep, int64_t, int64_t, int64_t, int64_t,
                          int64_t) { onSyncProgressInfo(syncDbId, status); });

    reconcileKnownSyncStatuses();
    refreshIconState();
}

void SystemTrayController::setMainWindow(QWindow *window) {
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

    qCInfo(lcSystemTrayController) << "System tray icon state changed | from:" << trayIconStateLogName(_iconState)
                                   << "| to:" << trayIconStateLogName(state) << "| icon:" << trayIconPath(state);
    _iconState = state;

    if (!_isInitialized) {
        return;
    }

    _trayIcon.setIcon(QIcon(QString::fromUtf8(trayIconPath(_iconState))));
}

void SystemTrayController::showMainWindow() const {
    if (!_mainWindow) {
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
    if (!_mainWindow) {
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
    if (!_isProductStateInitialized || !_appCache) {
        setIconState(TrayIconState::Neutral);
        return;
    }

    if (std::ranges::any_of(_syncStatuses, [](const auto &entry) { return isSyncStatus(entry.second); })) {
        setIconState(TrayIconState::Sync);
        return;
    }

    if (!_appCache->syncErrors().empty()) {
        setIconState(TrayIconState::Error);
        return;
    }

    if (_isNotificationActive) {
        setIconState(TrayIconState::Notification);
        return;
    }

    if (std::ranges::all_of(_syncStatuses, [](const auto &entry) { return isPauseStatus(entry.second); })) {
        setIconState(TrayIconState::Pause);
        return;
    }

    setIconState(TrayIconState::Neutral);
}

void SystemTrayController::reconcileKnownSyncStatuses() {
    if (!_appCache) {
        _syncStatuses.clear();
        return;
    }

    const auto syncs = _appCache->syncs();
    std::erase_if(_syncStatuses, [&syncs](const auto &entry) {
        return std::ranges::none_of(syncs, [&entry](const SyncInfo &sync) { return sync.dbId() == entry.first; });
    });

    for (const auto &sync: syncs) {
        (void) _syncStatuses.try_emplace(sync.dbId(), SyncStatus::Undefined);
    }
}

void SystemTrayController::onSyncProgressInfo(const SyncDbId syncDbId, const SyncStatus status) {
    const auto previousStatusIt = _syncStatuses.find(syncDbId);
    if (previousStatusIt != _syncStatuses.end() && previousStatusIt->second == status) {
        return;
    }

    qCInfo(lcSystemTrayController) << "Sync status changed for tray icon | syncDbId:" << syncDbId << "| from:"
                                   << (previousStatusIt == _syncStatuses.end() ? QStringLiteral("Unknown")
                                                                               : syncStatusLogName(previousStatusIt->second))
                                   << "| to:" << syncStatusLogName(status);
    _syncStatuses[syncDbId] = status;
    refreshIconState();
}

void SystemTrayController::onTrayActivated(const QSystemTrayIcon::ActivationReason reason) const {
    qCInfo(lcSystemTrayController) << "System tray activated | reason:" << reason;

    if (!_isTrayModeActive) {
        qCWarning(lcSystemTrayController) << "Tray activation ignored because tray mode is not active";
        return;
    }

    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::MiddleClick) {
        showMainWindow();
    }
}

} // namespace KDC
