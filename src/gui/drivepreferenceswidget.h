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

#include "customswitch.h"
#include "actionwidget.h"
#include "customtoolbutton.h"
#include "preferencesblocwidget.h"
#include "folderitemwidget.h"
#include "foldertreeitemwidget.h"
#include "widgetwithcustomtooltip.h"

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

class DrivePreferencesWidget : public LargeWidgetWithCustomToolTip {
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
        int _driveDbId{0};
        int _userDbId{0};

        QVBoxLayout *_mainVBox{nullptr};
        ActionWidget *_displayErrorsWidget{nullptr};
        ActionWidget *_displayBigFoldersWarningWidget{nullptr};
        QLabel *_userAvatarLabel{nullptr};
        QLabel *_userNameLabel{nullptr};
        QLabel *_userMailLabel{nullptr};
        CustomSwitch *_notificationsSwitch{nullptr};
        int _foldersBeginIndex{0};
        QLabel *_foldersLabel{nullptr};
        CustomPushButton *_addLocalFolderButton{nullptr};
        QLabel *_notificationsLabel{nullptr};
        QLabel *_notificationsTitleLabel{nullptr};
        QLabel *_notificationsDescriptionLabel{nullptr};
        QLabel *_connectedWithLabel{nullptr};
        CustomToolButton *_removeDriveButton{nullptr};
        bool _updatingFoldersBlocs{false};

        void showEvent(QShowEvent *event) override;

        bool existUndecidedSet();
        void updateUserInfo();
        void askEnableLiteSync(const std::function<void(bool enable)> &callback);
        void askDisableLiteSync(const std::function<void(bool enable, bool diskSpaceWarning)> &callback, int syncDbId);
        bool switchVfsOn(int syncDbId);
        bool switchVfsOff(int syncDbId, bool diskSpaceWarning);
        void resetFoldersBlocs();
        void updateFoldersBlocs();
        void refreshFoldersBlocs() const;
        FolderTreeItemWidget *blocTreeItemWidget(PreferencesBlocWidget *folderBloc);
        FolderItemWidget *blocItemWidget(PreferencesBlocWidget *folderBloc);
        QFrame *blocSeparatorFrame(PreferencesBlocWidget *folderBloc);
        bool addSync(const QString &localFolderPath, bool liteSync, const QString &serverFolderPath,
                     const QString &serverFolderNodeId, QSet<QString> blackSet, QSet<QString> whiteSet);
        bool updateSelectiveSyncList(const QHash<int, QHash<const QString, bool>> &mapUndefinedFolders);
        void updateGuardedFoldersBlocs();

    private slots:
        void onErrorsWidgetClicked();
        void onBigFoldersWarningWidgetClicked();
        void onAddLocalFolder(bool checked = false);
        void onLiteSyncSwitchSyncChanged(int syncDbId, bool activate);
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
