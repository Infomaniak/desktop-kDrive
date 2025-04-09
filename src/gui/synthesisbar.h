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

#pragma once

#include "customtoolbutton.h"
#include "errorspopup.h"
#include "libcommon/utility/types.h"

#include <QList>
#include <QWidget>

#include <memory>

namespace KDC {

class ClientGui;

class SynthesisBar : public QWidget {
        Q_OBJECT

    public:
        explicit SynthesisBar(std::shared_ptr<ClientGui> gui, bool debugCrash, QWidget *parent = nullptr);

        void refreshErrorsButton();
        void displayErrors(int driveDbId);
        bool systemMove() const { return _systemMove; }
        void reset();

    private:
        std::shared_ptr<ClientGui> _gui;
        CustomToolButton *_errorsButton{nullptr};
        CustomToolButton *_infosButton{nullptr};
        CustomToolButton *_menuButton{nullptr};
        NotificationsDisabled _notificationsDisabled{NotificationsDisabled::Never};
        QDateTime _notificationsDisabledUntilDateTime;
        bool _debugCrash{false};
        bool _systemMove{false};

        static const std::map<NotificationsDisabled, QString> _notificationsDisabledMap;
        static const std::map<NotificationsDisabled, QString> _notificationsDisabledForPeriodMap;

        void getDriveErrorList(QList<ErrorsPopup::DriveError> &list);
        QUrl syncUrl(int syncDbId, const QString &filePath);
        void openUrl(int syncDbId, const QString &filePath = QString());

        /**
         * This method determines if the user is allowed to move the top level dialog.
         * @return `true` if he is allowed
         * @note Wayland does not allow programmatically to set a top level window position. In this case, the position of the
         * synthesis popup is therefore not aligned with the system tray icon and the user is allowed to move it.
         */
        bool allowMove() {
            static const bool value = qApp->platformName() == "wayland";
            return value;
        }

        bool event(QEvent *event) override;
        void showEvent(QShowEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;
        bool eventFilter(QObject *obj, QEvent *event);

    signals:
        void showParametersDialog(int driveDbId = 0, bool errorPage = false);
        void disableNotifications(NotificationsDisabled type, QDateTime dateTime);
        void exit();
        void crash();
        void crashServer();
        void crashEnforce();
        void crashFatal();

    private slots:
        void onDisplayErrors(int driveDbId);
        void onOpenErrorsMenu();
        void onOpenMiscellaneousMenu();
        void onOpenFolder();
        void onOpenWebview();
        void onOpenDriveParameters();
        void onNotificationActionTriggered();
        void onOpenPreferences();
        void onDisplayHelp();
        void onSendFeedback();
        void onExit();
        void onCrash();
        void onCrashServer();
        void onCrashEnforce();
        void onCrashFatal();
        void retranslateUi();
};

} // namespace KDC
