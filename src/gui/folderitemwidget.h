/*
Infomaniak Drive
Copyright (C) 2020 christophe.larchier@infomaniak.com

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

#include "customtoolbutton.h"
#include "customlabel.h"
#include "info/syncinfoclient.h"

#include <QLabel>
#include <QString>

namespace KDC {

class ClientGui;
class MenuWidget;
class UserInfoClient;

class FolderItemWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FolderItemWidget(int syncDbId, std::shared_ptr<ClientGui> gui, QWidget *parent = nullptr);

    inline int syncDbId() const { return _syncDbId; }
    void updateItem(const SyncInfoClient &syncInfo);
    void setUpdateWidgetVisible(bool visible);
    void setUpdateWidgetLabelVisible(bool visible);
    void setSupportVfs(bool value);
    void setSmartSyncActivated(bool value);
    void closeFolderView();

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
    CustomToolButton *_expandButton;
    CustomToolButton *_menuButton;
    QLabel *_statusIconLabel;
    QLabel *_nameLabel;
    CustomLabel *_smartSyncIconLabel;
    QWidget *_updateWidget;
    bool _isExpanded;
    bool _smartSyncAvailable;
    bool _smartSyncActivated;
    QLabel *_synchroLabel;
    QLabel *_saveLabel;
    QPushButton *_cancelButton;
    QPushButton *_validateButton;
    std::unique_ptr<MenuWidget> _menu;

    void showEvent(QShowEvent *) override;

    void setExpandButton();
    bool checkInfoMaps() const noexcept;

    SyncInfoClient *getSyncInfoClient() const noexcept;
    const UserInfoClient *getUserInfoClient() const noexcept;
    void setToolTipsEnabled(bool enabled) noexcept;

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

}
