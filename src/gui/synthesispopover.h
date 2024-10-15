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

#pragma once

#include "customtoolbutton.h"
#include "driveselectionwidget.h"
#include "progressbarwidget.h"
#include "statusbarwidget.h"
#include "buttonsbarwidget.h"
#include "synchronizeditemwidget.h"
#include "errorspopup.h"
#include "menuitemwidget.h"
#include "info/driveinfoclient.h"
#include "info/syncfileiteminfo.h"

#include <QColor>
#include <QDateTime>
#include <QDialog>
#include <QEvent>
#include <QListWidget>
#include <QRect>
#include <QStackedWidget>
#include <QWidgetAction>

namespace KDC {

class ClientGui;

class SynthesisPopover : public QDialog {
        Q_OBJECT

        Q_PROPERTY(QColor background_main_color READ backgroundMainColor WRITE setBackgroundMainColor)

    public:
        enum defaultTextLabelType {
            defaultTextLabelTypeNoSyncFolder = 0,
            defaultTextLabelTypeNoDrive,
            defaultTextLabelTypeCanSync
        };

        static const std::map<NotificationsDisabled, QString> _notificationsDisabledMap;
        static const std::map<NotificationsDisabled, QString> _notificationsDisabledForPeriodMap;

        explicit SynthesisPopover(std::shared_ptr<ClientGui> gui, bool debugCrash, QWidget *parent = nullptr);
        ~SynthesisPopover();

        void setPosition(const QRect &sysTrayIconRect);
        void forceRedraw();

        void refreshLockedStatus();

    signals:
        void showParametersDialog(int syncDbId = 0, bool errorPage = false);
        void exit();
        void addDrive();
        void disableNotifications(NotificationsDisabled type, QDateTime dateTime);
        void applyStyle();
        void crash();
        void crashServer();
        void crashEnforce();
        void crashFatal();
        void cannotSelect(bool cannotSelect);
        void updateItemList();
        void copyLinkItem(int syncDbId, const QString &filePath);
        void openWebviewItem(int syncDbId, const QString &filePath);
        void executeSyncAction(ActionType type, ActionTarget target, int dbId);

    public slots:
        void onConfigRefreshed();
        void onUpdateProgress(int syncDbId);
        void onItemCompleted(int syncDbId, const SyncFileItemInfo &itemInfo);
        void onDriveQuotaUpdated(int driveDbId);
        void onRefreshErrorList(int driveDbId);
        void onAppVersionLocked(bool currentVersionLocked);
        void onRefreshStatusNeeded();
        void onUpdateAvailabalityChange(UpdateState updateState);

    private:
        std::shared_ptr<ClientGui> _gui;
        bool _debugCrash;
        QRect _sysTrayIconRect;

        QColor _backgroundMainColor;
        CustomToolButton *_errorsButton{nullptr};
        CustomToolButton *_infosButton{nullptr};
        CustomToolButton *_menuButton{nullptr};
        DriveSelectionWidget *_driveSelectionWidget{nullptr};
        ProgressBarWidget *_progressBarWidget{nullptr};
        StatusBarWidget *_statusBarWidget{nullptr};
        ButtonsBarWidget *_buttonsBarWidget{nullptr};
        QStackedWidget *_stackedWidget{nullptr};
        QWidget *_defaultSynchronizedPageWidget{nullptr};
        QWidget *_mainWidget{nullptr};
        QWidget *_lockedAppVersionWidget{nullptr};
        NotificationsDisabled _notificationsDisabled{NotificationsDisabled::Never};
        QDateTime _notificationsDisabledUntilDateTime;
        QLabel *_notImplementedLabel{nullptr};
        QLabel *_notImplementedLabel2{nullptr};
        QLabel *_defaultTitleLabel{nullptr};
        QLabel *_defaultTextLabel{nullptr};
        int _defaultTextLabelType{defaultTextLabelTypeNoSyncFolder};
        QUrl _localFolderUrl;
        QUrl _remoteFolderUrl;
        qint64 _lastRefresh{0};
        QLabel *_lockedAppupdateAppLabel;
        QPushButton *_lockedAppUpdateButton{nullptr};
        QLabel *_lockedAppLabel{nullptr};
#ifdef Q_OS_LINUX
        QLabel *_lockedAppUpdateManualLabel{nullptr};
#endif
        void changeEvent(QEvent *event) override;
        void paintEvent(QPaintEvent *event) override;
        bool event(QEvent *event) override;

        [[nodiscard]] QColor backgroundMainColor() const { return _backgroundMainColor; }
        void setBackgroundMainColor(const QColor &value) { _backgroundMainColor = value; }

        void initUI();
        QUrl syncUrl(int syncDbId, const QString &filePath);
        void openUrl(int syncDbId, const QString &filePath = QString());
        void getFirstSyncWithStatus(SyncStatus status, int driveDbId, int &syncDbId, bool &found);
        void getFirstSyncByPriority(int driveDbId, int &syncDbId, bool &found);
        void refreshStatusBar(const DriveInfoClient &driveInfo);
        void refreshStatusBar(std::map<int, DriveInfoClient>::const_iterator driveInfoIt);
        void refreshStatusBar(int userDbId);
        void refreshErrorsButton();
        void setSynchronizedDefaultPage(QWidget **widget, QWidget *parent);
        void displayErrors(int userDbId);
        void reset();
        void addSynchronizedListWidgetItem(DriveInfoClient &driveInfoClient, int row = 0);
        void getDriveErrorList(QList<ErrorsPopup::DriveError> &list);
        void handleRemovedDrives();

    private slots:
        void onOpenErrorsMenu(bool checked = false);
        void onDisplayErrors(int syncDbId);
        void onOpenFolder(bool checked);
        void onOpenWebview(bool checked);
        void onOpenMiscellaneousMenu(bool checked);
        void onOpenPreferences(bool checked = false);
        void onNotificationActionTriggered(bool checked = false);
        void onOpenDriveParameters(bool checked = false);
        void onDisplayHelp(bool checked = false);
        void onExit(bool checked = false);
        void onCrash(bool checked = false);
        void onCrashServer(bool checked = false);
        void onCrashEnforce(bool checked = false);
        void onCrashFatal(bool checked = false);
        void onDriveSelected(int driveDbId);
        void onAddDrive();
        void onPauseSync(ActionTarget target, int syncDbId = 0);
        void onResumeSync(ActionTarget target, int syncDbId = 0);
        void onButtonBarToggled(int position);
        void onOpenFolderItem(const SynchronizedItem &item);
        void onOpenItem(const SynchronizedItem &item);
        void onAddToFavouriteItem(const SynchronizedItem &item);
        // void onManageRightAndSharingItem(const SynchronizedItem &item);
        void onCopyLinkItem(const SynchronizedItem &item);
        void onOpenWebviewItem(const SynchronizedItem &item);
        void onSelectionChanged(bool isSelected);
        void onLinkActivated(const QString &link);
        void onUpdateSynchronizedListWidget();
        void onStartInstaller() const noexcept;
        void retranslateUi();
};

} // namespace KDC
