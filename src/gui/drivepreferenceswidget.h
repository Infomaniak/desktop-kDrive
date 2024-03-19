/*
Infomaniak Drive
Copyright (C) 2021 christophe.larchier@infomaniak.com

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#pragma once

#include "customswitch.h"
#include "actionwidget.h"
#include "customtoolbutton.h"
#include "preferencesblocwidget.h"
#include "folderitemwidget.h"
#include "foldertreeitemwidget.h"
#include "parameterswidget.h"

#include <QBoxLayout>
#include <QColor>
#include <QFrame>
#include <QLabel>
#include <QMetaObject>
#include <QProgressBar>
#include <QString>
#include <QMutex>

namespace KDC {

class CustomPushButton;
class ClientGui;

class DrivePreferencesWidget : public ParametersWidget {
        Q_OBJECT

    public:
        explicit DrivePreferencesWidget(std::shared_ptr<ClientGui> gui, QWidget *parent = nullptr);

        void setDrive(int driveDbId, bool unresolvedErrors);
        void reset();
        void showErrorBanner(bool unresolvedErrors);
        void refreshStatus();

    signals:
        void displayErrors(int driveDbId);
        void errorAdded();
        void openFolder(const QString &filePath);
        void removeDrive(int driveDbId);
        void newBigFolderDiscovered(int syncDbId, const QString &path);
        void undecidedListsCleared();
        void runSync(int syncDbId);
        void pauseSync(int syncDbId);
        void resumeSync(int syncDbId);

    private:
        enum AddFolderStep { SelectLocalFolder = 0, SelectServerBaseFolder, SelectServerFolders, Confirm };

        std::shared_ptr<ClientGui> _gui;
        int _driveDbId;
        int _userDbId;

        QVBoxLayout *_mainVBox;
        ActionWidget *_displayErrorsWidget;
        ActionWidget *_displayBigFoldersWarningWidget;
        QLabel *_userAvatarLabel;
        QLabel *_userNameLabel;
        QLabel *_userMailLabel;
        CustomSwitch *_notificationsSwitch;
        int _foldersBeginIndex;
        QLabel *_foldersLabel;
        CustomPushButton *_addLocalFolderButton;
        QLabel *_notificationsLabel;
        QLabel *_notificationsTitleLabel;
        QLabel *_notificationsDescriptionLabel;
        QLabel *_connectedWithLabel;
        CustomToolButton *_removeDriveButton;
        bool _updatingFoldersBlocs;

        void showEvent(QShowEvent *event) override;

        bool existUndecidedSet();
        void updateUserInfo();
        void askEnableSmartSync(const std::function<void(bool enable)> &callback);
        void askDisableSmartSync(const std::function<void(bool enable, bool diskSpaceWarning)> &callback, int syncDbId);
        bool switchVfsOn(int syncDbId);
        bool switchVfsOff(int syncDbId, bool diskSpaceWarning);
        void resetFoldersBlocs();
        void updateFoldersBlocs();
        void refreshFoldersBlocs();
        FolderTreeItemWidget *blocTreeItemWidget(PreferencesBlocWidget *folderBloc);
        FolderItemWidget *blocItemWidget(PreferencesBlocWidget *folderBloc);
        QFrame *blocSeparatorFrame(PreferencesBlocWidget *folderBloc);
        bool addSync(const QString &localFolderPath, bool smartSync, const QString &serverFolderPath,
                     const QString &serverFolderNodeId, QSet<QString> blackSet, QSet<QString> whiteSet);
        bool updateSelectiveSyncList(const QHash<int, QHash<const QString, bool>> &mapUndefinedFolders);

    private slots:
        void onErrorsWidgetClicked();
        void onBigFoldersWarningWidgetClicked();
        void onAddLocalFolder(bool checked = false);
        void onSmartSyncSwitchSyncChanged(int syncDbId, bool activate);
        void onNotificationsSwitchClicked(bool checked = false);
        void onErrorAdded();
        void onRemoveDrive(bool checked = false);
        void onUnsyncTriggered(int syncDbId);
        void onDisplayFolderDetail(int syncDbId, bool display);
        void onOpenFolder(const QString &filePath);
        void onSubfoldersLoaded(bool error, bool empty);
        void onNeedToSave(bool isFolderItemBlackListed);
        void onCancelUpdate(int syncDbId);
        void onValidateUpdate(int syncDbId);
        void onNewBigFolderDiscovered(int syncDbId, const QString &path);
        void onUndecidedListsCleared();
        void retranslateUi();
        void onVfsConversionCompleted(int syncDbId);
        void onDriveBeingRemoved();
};

}  // namespace KDC
