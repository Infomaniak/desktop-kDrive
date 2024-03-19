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

#include "folderitemwidget.h"
#include "menuwidget.h"
#include "menuitemwidget.h"
#include "guiutility.h"
#include "clientgui.h"
#include "parameterscache.h"
#include "libcommon/utility/utility.h"

#include <QBoxLayout>
#include <QPushButton>
#include <QWidgetAction>
#include <QLoggingCategory>

namespace KDC {

static const int hSpacing = 10;
static const int vSpacing = 10;
static const int expandButtonVMargin = 5;
static const int statusIconSize = 20;
static const int folderNameMaxSize = 50;
static const int folderPathMaxSize = 50;

Q_LOGGING_CATEGORY(lcFolderItemWidget, "gui.folderitemwidget", QtInfoMsg)

FolderItemWidget::FolderItemWidget(int syncDbId, std::shared_ptr<ClientGui> gui, QWidget *parent)
    : QWidget(parent),
      _gui(gui),
      _syncDbId(syncDbId),
      _expandButton(nullptr),
      _menuButton(nullptr),
      _statusIconLabel(nullptr),
      _nameLabel(nullptr),
      _smartSyncIconLabel(nullptr),
      _updateWidget(nullptr),
      _isExpanded(false),
      _smartSyncAvailable(false),
      _synchroLabel(nullptr),
      _saveLabel(nullptr),
      _cancelButton(nullptr),
      _validateButton(nullptr) {
    QHBoxLayout *mainLayout = new QHBoxLayout();
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(hSpacing);
    setLayout(mainLayout);

    // Expand button
    QVBoxLayout *expandVBoxLayout = new QVBoxLayout();
    expandVBoxLayout->setContentsMargins(0, expandButtonVMargin, 0, expandButtonVMargin);
    mainLayout->addLayout(expandVBoxLayout);

    _expandButton = new CustomToolButton(this);
    _expandButton->setObjectName("expandButton");
    expandVBoxLayout->addWidget(_expandButton);

    // Folder detail
    QVBoxLayout *detailVBoxLayout = new QVBoxLayout();
    detailVBoxLayout->setContentsMargins(0, 0, 0, 0);
    detailVBoxLayout->setSpacing(vSpacing);
    mainLayout->addLayout(detailVBoxLayout);
    mainLayout->setStretchFactor(detailVBoxLayout, 1);

    QHBoxLayout *detailHBoxLayout = new QHBoxLayout();
    detailHBoxLayout->setContentsMargins(0, 0, 0, 0);
    detailHBoxLayout->setSpacing(hSpacing);
    detailVBoxLayout->addLayout(detailHBoxLayout);

    QVBoxLayout *folderVBoxLayout = new QVBoxLayout();
    folderVBoxLayout->setContentsMargins(0, 0, 0, 0);
    folderVBoxLayout->setSpacing(hSpacing);
    detailHBoxLayout->addLayout(folderVBoxLayout);

    QHBoxLayout *folderHBoxLayout = new QHBoxLayout();
    folderHBoxLayout->setContentsMargins(0, 0, 0, 0);
    folderHBoxLayout->setSpacing(hSpacing);
    folderVBoxLayout->addLayout(folderHBoxLayout);

    _statusIconLabel = new QLabel(this);
    folderHBoxLayout->addWidget(_statusIconLabel);

    _nameLabel = new QLabel(this);
    _nameLabel->setObjectName("largeMediumBoldTextLabel");
    folderHBoxLayout->addWidget(_nameLabel);
    folderHBoxLayout->addStretch();

    _synchroLabel = new QLabel(this);
    _synchroLabel->setObjectName("descriptionLabel");
    _synchroLabel->setContextMenuPolicy(Qt::PreventContextMenu);
    folderVBoxLayout->addWidget(_synchroLabel);

    // Smart sync activation icon
    _smartSyncIconLabel = new CustomLabel(this);
    _smartSyncIconLabel->setVisible(false);
    detailHBoxLayout->addWidget(_smartSyncIconLabel);

    // Menu button
    _menuButton = new CustomToolButton(this);
    _menuButton->setIconPath(":/client/resources/icons/actions/menu.svg");
    detailHBoxLayout->addWidget(_menuButton);

    // Update widget
    _updateWidget = new QWidget(this);
    _updateWidget->setVisible(false);
    detailVBoxLayout->addWidget(_updateWidget);

    QHBoxLayout *updateHBoxLayout = new QHBoxLayout();
    updateHBoxLayout->setContentsMargins(0, 0, 0, 0);
    updateHBoxLayout->setSpacing(hSpacing);
    _updateWidget->setLayout(updateHBoxLayout);

    _saveLabel = new QLabel(this);
    _saveLabel->setObjectName("subtitleLabel");
    _saveLabel->setWordWrap(true);
    _saveLabel->setVisible(false);
    updateHBoxLayout->addWidget(_saveLabel);
    updateHBoxLayout->setStretchFactor(_saveLabel, 1);
    updateHBoxLayout->addStretch();

    _cancelButton = new QPushButton(this);
    _cancelButton->setObjectName("nondefaultsmallbutton");
    _cancelButton->setFlat(true);
    updateHBoxLayout->addWidget(_cancelButton);

    _validateButton = new QPushButton(this);
    _validateButton->setObjectName("defaultsmallbutton");
    _validateButton->setFlat(true);
    updateHBoxLayout->addWidget(_validateButton);

    connect(_menuButton, &CustomToolButton::clicked, this, &FolderItemWidget::onMenuButtonClicked);
    connect(_expandButton, &CustomToolButton::clicked, this, &FolderItemWidget::onExpandButtonClicked);
    connect(_cancelButton, &QPushButton::clicked, this, &FolderItemWidget::onCancelButtonClicked);
    connect(_validateButton, &QPushButton::clicked, this, &FolderItemWidget::onValidateButtonClicked);
    connect(_synchroLabel, &QLabel::linkActivated, this, &FolderItemWidget::onOpenFolder);
    connect(this, &FolderItemWidget::displayFolderDetailCanceled, this, &FolderItemWidget::onDisplayFolderDetailCanceled);

    if (_syncDbId) {
        const auto &syncInfoMapIt = _gui->syncInfoMap().find(_syncDbId);
        if (syncInfoMapIt == _gui->syncInfoMap().end()) {
            qCWarning(lcFolderItemWidget) << "Sync not found in sync map for syncDbId=" << _syncDbId;
            return;
        }

        updateItem(syncInfoMapIt->second);
        setExpandButton();
        QString name = syncInfoMapIt->second.name();
        if (name.size() > folderNameMaxSize) {
            name = name.left(folderNameMaxSize) + "...";
        }
        _nameLabel->setText(name);

        QString path = syncInfoMapIt->second.localPath();
        if (path.size() > folderPathMaxSize) {
            path = path.left(folderPathMaxSize) + "...";
        }
        _synchroLabel->setText(tr("Synchronized into <a style=\"%1\" href=\"ref\">%2</a>").arg(CommonUtility::linkStyle, path));
    }
}

void FolderItemWidget::updateItem(const SyncInfoClient &syncInfo) {
    const auto &driveInfoMapIt = _gui->driveInfoMap().find(syncInfo.driveDbId());
    if (driveInfoMapIt == _gui->driveInfoMap().end()) {
        qCWarning(lcFolderItemWidget) << "Drive not found in drive map for syncDbId=" << syncInfo.dbId();
        return;
    }

    const auto &accountInfoMapIt = _gui->accountInfoMap().find(driveInfoMapIt->second.accountDbId());
    if (accountInfoMapIt == _gui->accountInfoMap().end()) {
        qCWarning(lcFolderItemWidget) << "Account not found in account map for accountDbId="
                                      << driveInfoMapIt->second.accountDbId();
        return;
    }

    const auto &userInfoMapIt = _gui->userInfoMap().find(accountInfoMapIt->second.userDbId());
    if (userInfoMapIt == _gui->userInfoMap().end()) {
        qCWarning(lcFolderItemWidget) << "User not found in user map for userDbId=" << accountInfoMapIt->second.userDbId();
        return;
    }

    KDC::GuiUtility::StatusInfo statusInfo;
    statusInfo._disconnected = !userInfoMapIt->second.connected();
    statusInfo._status = syncInfo.status();
    _statusIconLabel->setPixmap(
        QIcon(KDC::GuiUtility::getDriveStatusIconPath(statusInfo)).pixmap(QSize(statusIconSize, statusIconSize)));
}

void FolderItemWidget::setUpdateWidgetVisible(bool visible) {
    _updateWidget->setVisible(visible);
}

void FolderItemWidget::setUpdateWidgetLabelVisible(bool visible) {
    _saveLabel->setVisible(visible);
}


void FolderItemWidget::setSupportVfs(bool value) {
    _smartSyncAvailable = value;
    _smartSyncIconLabel->setVisible(value);
}

void FolderItemWidget::setSmartSyncActivated(bool value) {
    _smartSyncActivated = value;
    _smartSyncIconLabel->setPixmap(
        value ? KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/litesync-on.svg", QColor(16, 117, 187))
                    .pixmap(QSize(18, 18))
              : KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/litesync-off.svg", QColor(159, 159, 159))
                    .pixmap(QSize(18, 18)));

    QString liteSyncActivated =
        tr("Lite sync (Beta) is enabled. Files from kDrive remain in the Cloud and do not use your computer's storage space.");
    QString liteSyncDeactivated = tr("Lite sync (Beta) is disabled. The kDrive files use the storage space of your computer.");

    _smartSyncIconLabel->setToolTip(value ? liteSyncActivated : liteSyncDeactivated);
}

void FolderItemWidget::showEvent(QShowEvent *) {
    retranslateUi();

    _isExpanded = false;
    setExpandButton();
    emit displayFolderDetail(_syncDbId, _isExpanded);
}

void FolderItemWidget::setExpandButton() {
    _expandButton->hide();
    if (_isExpanded) {
        _expandButton->setIconPath(":/client/resources/icons/actions/chevron-down.svg");
    } else {
        _expandButton->setIconPath(":/client/resources/icons/actions/chevron-right.svg");
    }
    _expandButton->show();
}

void FolderItemWidget::closeFolderView() {
    //    _isExpanded = false;
    //    setExpandButton();
    //    emit displayFolderDetail(_syncDbId, _isExpanded);
}

void FolderItemWidget::onMenuButtonClicked() {
    MenuWidget *menu = new MenuWidget(MenuWidget::Menu, this);

    const auto &syncInfoMapIt = _gui->syncInfoMap().find(_syncDbId);
    if (syncInfoMapIt == _gui->syncInfoMap().end()) {
        qCWarning(lcFolderItemWidget) << "Sync not found in sync map for syncDbId=" << _syncDbId;
        return;
    }

    const auto &driveInfoMapIt = _gui->driveInfoMap().find(syncInfoMapIt->second.driveDbId());
    if (driveInfoMapIt == _gui->driveInfoMap().end()) {
        qCWarning(lcFolderItemWidget()) << "Drive not found in driveInfoMap for driveDbId=" << syncInfoMapIt->second.driveDbId();
        return;
    }

    const auto &accountInfoMapIt = _gui->accountInfoMap().find(driveInfoMapIt->second.accountDbId());
    if (accountInfoMapIt == _gui->accountInfoMap().end()) {
        qCWarning(lcFolderItemWidget()) << "Account not found in accountInfoMap for accountDbId="
                                        << driveInfoMapIt->second.accountDbId();
        return;
    }

    const auto userInfoMapIt = _gui->userInfoMap().find(accountInfoMapIt->second.userDbId());
    if (userInfoMapIt == _gui->userInfoMap().end()) {
        qCWarning(lcFolderItemWidget()) << "User not found in userInfoMap for userDbId=" << accountInfoMapIt->second.userDbId();
        return;
    }

    if (syncInfoMapIt->second.supportVfs()) {
        if (syncInfoMapIt->second.virtualFileMode() == VirtualFileModeOff) {
            QWidgetAction *activateLitesyncAction = new QWidgetAction(this);
            MenuItemWidget *activateLitesyncMenuItemWidget = new MenuItemWidget(
#ifdef Q_OS_WIN
                tr("Activate Lite Sync"));
#else
                tr("Activate Lite Sync (Beta)"));
#endif
            activateLitesyncMenuItemWidget->setLeftIcon(":/client/resources/icons/actions/litesync-on.svg");
            activateLitesyncAction->setDefaultWidget(activateLitesyncMenuItemWidget);
            activateLitesyncAction->setDisabled(!userInfoMapIt->second.connected());
            connect(activateLitesyncAction, &QWidgetAction::triggered, this, &FolderItemWidget::onActivateLitesyncTriggered);
            menu->addAction(activateLitesyncAction);
        } else {
            QWidgetAction *deactivateLitesyncAction = new QWidgetAction(this);
            MenuItemWidget *deactivateLitesyncMenuItemWidget = new MenuItemWidget(tr("Deactivate Lite Sync"));
            deactivateLitesyncMenuItemWidget->setLeftIcon(":/client/resources/icons/actions/litesync-off.svg");
            deactivateLitesyncAction->setDefaultWidget(deactivateLitesyncMenuItemWidget);
            deactivateLitesyncAction->setDisabled(!userInfoMapIt->second.connected());
            connect(deactivateLitesyncAction, &QWidgetAction::triggered, this, &FolderItemWidget::onDeactivateLitesyncTriggered);
            menu->addAction(deactivateLitesyncAction);
        }
    }

    if (KDC::GuiUtility::getPauseActionAvailable(syncInfoMapIt->second.status())) {
        QWidgetAction *pauseAction = new QWidgetAction(this);
        MenuItemWidget *pauseMenuItemWidget = new MenuItemWidget(tr("Pause synchronization"));
        pauseMenuItemWidget->setLeftIcon(":/client/resources/icons/actions/pause.svg");
        pauseAction->setDefaultWidget(pauseMenuItemWidget);
        pauseAction->setDisabled(!userInfoMapIt->second.connected());
        connect(pauseAction, &QWidgetAction::triggered, this, &FolderItemWidget::onPauseTriggered);
        menu->addAction(pauseAction);
    }

    if (KDC::GuiUtility::getResumeActionAvailable(syncInfoMapIt->second.status())) {
        QWidgetAction *resumeAction = new QWidgetAction(this);
        MenuItemWidget *resumeMenuItemWidget = new MenuItemWidget(tr("Resume synchronization"));
        resumeMenuItemWidget->setLeftIcon(":/client/resources/icons/actions/start.svg");
        resumeAction->setDefaultWidget(resumeMenuItemWidget);
        resumeAction->setDisabled(!userInfoMapIt->second.connected());
        connect(resumeAction, &QWidgetAction::triggered, this, &FolderItemWidget::onResumeTriggered);
        menu->addAction(resumeAction);
    }

    menu->addSeparator();

    QWidgetAction *unsyncAction = new QWidgetAction(this);
    MenuItemWidget *unsyncMenuItemWidget = new MenuItemWidget(tr("Remove synchronization"));
    unsyncMenuItemWidget->setLeftIcon(":/client/resources/icons/actions/delete.svg");
    unsyncAction->setDefaultWidget(unsyncMenuItemWidget);
    connect(unsyncAction, &QWidgetAction::triggered, this, &FolderItemWidget::onUnsyncTriggered);
    menu->addAction(unsyncAction);

    menu->exec(QWidget::mapToGlobal(_menuButton->geometry().center()));
}

void FolderItemWidget::onExpandButtonClicked() {
    _isExpanded = !_isExpanded;
    setExpandButton();
    emit displayFolderDetail(_syncDbId, _isExpanded);
}

void FolderItemWidget::onCancelButtonClicked() {
    emit cancelUpdate(_syncDbId);
}

void FolderItemWidget::onValidateButtonClicked() {
    emit validateUpdate(_syncDbId);
}

void FolderItemWidget::onOpenFolder(const QString &link) {
    Q_UNUSED(link)

    const auto &syncInfoMapIt = _gui->syncInfoMap().find(_syncDbId);
    if (syncInfoMapIt == _gui->syncInfoMap().end()) {
        qCWarning(lcFolderItemWidget) << "Sync not found in sync map for syncDbId=" << _syncDbId;
        return;
    }

    emit openFolder(syncInfoMapIt->second.localPath());
}

void FolderItemWidget::onSyncTriggered() {
    emit runSync(_syncDbId);
}

void FolderItemWidget::onPauseTriggered() {
    emit pauseSync(_syncDbId);
}

void FolderItemWidget::onResumeTriggered() {
    emit resumeSync(_syncDbId);
}

void FolderItemWidget::onUnsyncTriggered() {
    emit unSync(_syncDbId);
}

void FolderItemWidget::onDisplayFolderDetailCanceled() {
    _isExpanded = false;
    setExpandButton();
}

void FolderItemWidget::onActivateLitesyncTriggered() {
    emit triggerLiteSyncChanged(_syncDbId, true);
}

void FolderItemWidget::onDeactivateLitesyncTriggered() {
    emit triggerLiteSyncChanged(_syncDbId, false);
}

void FolderItemWidget::retranslateUi() {
    _menuButton->setToolTip(tr("More actions"));

    const auto &syncInfoMapIt = _gui->syncInfoMap().find(_syncDbId);
    if (syncInfoMapIt == _gui->syncInfoMap().end()) {
        qCWarning(lcFolderItemWidget) << "Sync not found in sync map for syncDbId=" << _syncDbId;
        return;
    }

    QString path = syncInfoMapIt->second.localPath();
    if (path.size() > folderPathMaxSize) {
        path = path.left(folderPathMaxSize) + "...";
    }
    _synchroLabel->setText(tr("Synchronized into <a style=\"%1\" href=\"ref\">%2</a>").arg(CommonUtility::linkStyle, path));

    if (ParametersCache::instance()->parametersInfo().moveToTrash()) {
        _saveLabel->setText(
            tr("Unselected folders will be moved to your computer's recycle bin. Folders synced to kDrive will remain available "
               "online."));
    } else {
        _saveLabel->setText(
            tr("Unselected folders will be <b>permanently</b> deleted from the computer. Folders synced to kDrive will remain "
               "available online."));
    }

    _cancelButton->setText(tr("Cancel"));
    _validateButton->setText(tr("VALIDATE"));
    _smartSyncIconLabel->setToolTip(
        _smartSyncActivated ? tr("Lite sync (Beta) is enabled. Files from kDrive remain in the Cloud and do not use your "
                                 "computer's storage space.")
                            : tr("Lite sync (Beta) is disabled. The kDrive files use the storage space of your computer."));
}

}  // namespace KDC
