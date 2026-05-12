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
#include <QIcon>
#include <QLoggingCategory>
#include <QWindow>

#include <algorithm>

namespace KDC {

Q_LOGGING_CATEGORY(lcSystemTrayController, "gui.v4.systray", QtInfoMsg)

namespace {

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
    QObject(parent) {}

void SystemTrayController::initialize() {
    if (_isInitialized) {
        qCWarning(lcSystemTrayController) << "System tray controller already initialized";
        return;
    }

    _isTrayAvailable = QSystemTrayIcon::isSystemTrayAvailable();
    qCInfo(lcSystemTrayController) << "Initializing system tray | available:" << _isTrayAvailable
                                   << "| state:" << trayIconStateLogName(_iconState) << "| icon:" << trayIconPath(_iconState);

    _trayIcon.setIcon(QIcon(QString::fromUtf8(trayIconPath(_iconState))));
    _trayIcon.setToolTip(tr("kDrive"));

    _openAction = _trayMenu.addAction(tr("Open kDrive"));
    _trayMenu.addSeparator();
    _quitAction = _trayMenu.addAction(tr("Quit kDrive"));
    _trayIcon.setContextMenu(&_trayMenu);

    (void) connect(_openAction, &QAction::triggered, this, &SystemTrayController::showMainWindow);
    (void) connect(_quitAction, &QAction::triggered, this, &SystemTrayController::quitRequested);
    (void) connect(&_trayIcon, &QSystemTrayIcon::activated, this, &SystemTrayController::onTrayActivated);

    if (!_isTrayAvailable) {
        qCWarning(lcSystemTrayController) << "System tray is not available";
    }

    _trayIcon.show();
    _isInitialized = true;
    qCInfo(lcSystemTrayController) << "System tray icon shown | visible:" << _trayIcon.isVisible();
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

    if (!_isTrayAvailable && _mainWindow) {
        qCWarning(lcSystemTrayController) << "Showing main window because the system tray is unavailable";
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

void SystemTrayController::hideMainWindow() const {
    if (!_mainWindow) {
        qCWarning(lcSystemTrayController) << "Cannot hide main window: no window registered";
        return;
    }

    qCInfo(lcSystemTrayController) << "Hiding main window instead of quitting";
    _mainWindow->hide();
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

void SystemTrayController::onTrayActivated(QSystemTrayIcon::ActivationReason reason) {
    qCInfo(lcSystemTrayController) << "System tray activated | reason:" << reason;

    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::MiddleClick) {
        showMainWindow();
    }
}

} // namespace KDC
