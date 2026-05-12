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

#include "libcommon/utility/types.h"

#include <QMenu>
#include <QObject>
#include <QPointer>
#include <QSystemTrayIcon>

#include <unordered_map>

class QAction;
class QWindow;

namespace KDC {

class AppCache;
class CommService;

enum class TrayIconState {
    Neutral,
    Error,
    Notification,
    Pause,
    Sync,
};

/**
 * Owns the Linux v4 system tray entry and window visibility actions.
 *
 * The controller keeps the application alive in the background while the main QML window is hidden, and provides both direct
 * tray-click activation and a tray menu action for desktop environments such as GNOME.
 */
class SystemTrayController final : public QObject {
        Q_OBJECT

    public:
        explicit SystemTrayController(QObject *parent = nullptr);

        void initialize();
        void observe(AppCache &appCache, CommService &commService);
        void setMainWindow(QWindow *window);
        void setProductStateInitialized(bool initialized);
        void setNotificationActive(bool active);
        void setIconState(TrayIconState state);

        Q_INVOKABLE void showMainWindow() const;
        Q_INVOKABLE void hideMainWindow() const;

    signals:
        void quitRequested();

    private:
        void refreshIconState();
        void reconcileKnownSyncStatuses();
        void onSyncProgressInfo(SyncDbId syncDbId, SyncStatus status);
        void onTrayActivated(QSystemTrayIcon::ActivationReason reason);

        AppCache *_appCache = nullptr;
        QPointer<QWindow> _mainWindow;
        QSystemTrayIcon _trayIcon;
        QMenu _trayMenu;
        QAction *_openAction = nullptr;
        QAction *_quitAction = nullptr;
        std::unordered_map<SyncDbId, SyncStatus> _syncStatuses;
        TrayIconState _iconState = TrayIconState::Neutral;
        bool _isProductStateInitialized = false;
        bool _isNotificationActive = false;
        bool _isTrayAvailable = false;
        bool _isInitialized = false;
};

} // namespace KDC
