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
#include "customlabel.h"
#include "info/syncinfoclient.h"

#include <QLabel>
#include <QString>

namespace KDC {

class ClientGui;
class DriveInfoClient;
class MenuWidget;
class UserInfoClient;

class FolderItemWidget : public QWidget {
        Q_OBJECT

    public:
        explicit FolderItemWidget(int syncDbId, std::shared_ptr<ClientGui> gui, QWidget *parent = nullptr);

        inline int syncDbId() const { return _syncDbId; }
        void updateItem();
        void setUpdateWidgetVisible(bool visible);
        void setUpdateWidgetLabelVisible(bool visible);
        void setSupportVfs(bool value);
        void setSmartSyncActivated(bool value);
        void closeFolderView();
        SyncInfoClient *getSyncInfoClient() const noexcept;
        const DriveInfoClient *getDriveInfoClient() const noexcept;
        bool isBeingDeleted() const noexcept;
        void setToolTipsEnabled(bool enabled) noexcept;

    signals:
        void runSync(int syncDbId);
        void pauseSync(int syncDbId);
        void resumeSync(int syncDbId);
        void unSync(int syncDbId);
        void displayFolderDetail(int syncDbId, bool display);
        void displayFolderDetailCanceled();
        void openFolder(const QString &filePath);
        void cancelUpdate(int syncDbId);
        void validateUpdate(int syncDbId);
        void triggerLiteSyncChanged(int syncDbId, bool activate);

    private:
        std::shared_ptr<ClientGui> _gui;
        const int _syncDbId;
        CustomToolButton *_expandButton{nullptr};
        CustomToolButton *_menuButton{nullptr};
        QLabel *_statusIconLabel{nullptr};
        QLabel *_nameLabel{nullptr};
        CustomLabel *_smartSyncIconLabel;
        QWidget *_updateWidget{nullptr};
        bool _isExpanded{false};
        bool _smartSyncAvailable{false};
        bool _smartSyncActivated{false};
        QLabel *_synchroLabel{nullptr};
        QLabel *_saveLabel{nullptr};
        QPushButton *_cancelButton{nullptr};
        QPushButton *_validateButton{nullptr};
        std::unique_ptr<MenuWidget> _menu{nullptr};

        void showEvent(QShowEvent *) override;

        void setExpandButton();
        bool checkInfoMaps() const noexcept;

        const UserInfoClient *getUserInfoClient() const noexcept;

    private slots:
        void onMenuButtonClicked();
        void onExpandButtonClicked();
        void onCancelButtonClicked();
        void onValidateButtonClicked();
        void onOpenFolder(const QString &link);
        void onSyncTriggered();
        void onPauseTriggered();
        void onResumeTriggered();
        void onUnsyncTriggered();
        void onDisplayFolderDetailCanceled();
        void onActivateLitesyncTriggered();
        void onDeactivateLitesyncTriggered();
        void retranslateUi();
};

}  // namespace KDC
