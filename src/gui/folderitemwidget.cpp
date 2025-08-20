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

#include "folderitemwidget.h"
#include "menuwidget.h"
#include "menuitemwidget.h"
#include "guiutility.h"
#include "clientgui.h"
#include "parameterscache.h"
#include "libcommongui/matomoclient.h"
#include "libcommon/utility/utility.h"

#include <QBoxLayout>
#include <QPushButton>
#include <QWidgetAction>
#include <QLoggingCategory>

#include <unordered_map>

namespace KDC {

static const int hSpacing = 10;
static const int vSpacing = 10;
static const int expandButtonVMargin = 5;
static const int statusIconSize = 20;
#ifdef _MACOS
static const QString liteSyncActivatedQstr = QObject::tr(
        "Lite sync (Beta) is enabled. Files from kDrive remain in the Cloud and do not use your computer's storage space.");
static const QString liteSyncDeactivatedQstr =
        QObject::tr("Lite sync (Beta) is disabled. The kDrive files use the storage space of your computer.");
#else
static const QString liteSyncActivatedQstr =
        QObject::tr("Lite sync is enabled. Files from kDrive remain in the Cloud and do not use your computer's storage space.");
static const QString liteSyncDeactivatedQstr =
        QObject::tr("Lite sync is disabled. The kDrive files use the storage space of your computer.");
#endif
Q_LOGGING_CATEGORY(lcFolderItemWidget, "gui.folderitemwidget", QtInfoMsg)

enum class MenuAction : char {
    LiteSyncOn,
    LiteSyncOff,
    Pause,
    Remove,
    Resume
};

QString menuIconPath(MenuAction action) {
    static const QString actionIconsPath = ":/client/resources/icons/actions/";
    static const std::unordered_map<MenuAction, QString> iconPathsMap{
            {MenuAction::LiteSyncOn, actionIconsPath + "litesync-on.svg"},
            {MenuAction::LiteSyncOff, actionIconsPath + "litesync-off.svg"},
            {MenuAction::Pause, actionIconsPath + "pause.svg"},
            {MenuAction::Remove, actionIconsPath + "delete.svg"},
            {MenuAction::Resume, actionIconsPath + "start.svg"},
    };

    return iconPathsMap.at(action);
}


FolderItemWidget::FolderItemWidget(int syncDbId, std::shared_ptr<ClientGui> gui, QWidget *parent) :
    QWidget(parent),
    _gui(gui),
    _syncDbId(syncDbId) {
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
    _liteSyncIconLabel = new CustomLabel(this);
    _liteSyncIconLabel->setVisible(false);
    detailHBoxLayout->addWidget(_liteSyncIconLabel);

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
        const auto syncInfoClient = getSyncInfoClient();
        if (!syncInfoClient) {
            return;
        }

        updateItem();
        setExpandButton();
        QString name = syncInfoClient->name();
        GuiUtility::makePrintablePath(name);
        _nameLabel->setText(name);

        QString path = syncInfoClient->localPath();
        GuiUtility::makePrintablePath(path);
        _synchroLabel->setText(tr("Synchronized into <a style=\"%1\" href=\"ref\">%2</a>").arg(CommonUtility::linkStyle, path));
    }
}

bool FolderItemWidget::isBeingDeleted() const noexcept {
    if (const auto syncInfoClient = getSyncInfoClient(); !syncInfoClient || syncInfoClient->isBeingDeleted()) return true;
    if (const auto driveInfoClient = getDriveInfoClient(); !driveInfoClient || driveInfoClient->isBeingDeleted()) return true;

    return false;
}

void FolderItemWidget::updateItem() {
    const auto userInfoClient = getUserInfoClient();
    if (!userInfoClient) {
        return;
    }

    // Lock GUI if a synchronization is being deleted
    const bool enabled = !isBeingDeleted();
    GuiUtility::setEnabledRecursively(this, enabled);
    setToolTipsEnabled(enabled);
    if (!enabled) {
        _isExpanded = false;
        setExpandButton();
    }

    KDC::GuiUtility::StatusInfo statusInfo;
    statusInfo._disconnected = !userInfoClient->connected();
    const auto syncInfoClient = getSyncInfoClient();
    statusInfo._status = syncInfoClient->status();
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
    _liteSyncAvailable = value;
    _liteSyncIconLabel->setVisible(value);
}

void FolderItemWidget::setLiteSyncActivated(bool value) {
    _liteSyncActivated = value;

    static const QPixmap liteSynOnPixmap =
            KDC::GuiUtility::getIconWithColor(menuIconPath(MenuAction::LiteSyncOn), QColor(16, 117, 187)).pixmap(QSize(18, 18));
    static const QPixmap liteSynOffPixmap =
            KDC::GuiUtility::getIconWithColor(menuIconPath(MenuAction::LiteSyncOff), QColor(159, 159, 159)).pixmap(QSize(18, 18));

    _liteSyncIconLabel->setPixmap(value ? liteSynOnPixmap : liteSynOffPixmap);
    _liteSyncIconLabel->setToolTip(value ? liteSyncActivatedQstr : liteSyncDeactivatedQstr);
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


SyncInfoClient *FolderItemWidget::getSyncInfoClient() const noexcept {
    const auto &syncInfoMapIt = _gui->syncInfoMap().find(_syncDbId);
    if (syncInfoMapIt == _gui->syncInfoMap().end()) {
        qCWarning(lcFolderItemWidget) << "Sync not found in sync map for syncDbId=" << _syncDbId;
        return nullptr;
    }

    return &(syncInfoMapIt->second);
}


const DriveInfoClient *FolderItemWidget::getDriveInfoClient() const noexcept {
    const auto syncInfoClient = getSyncInfoClient();
    if (!syncInfoClient) {
        return nullptr;
    }

    const auto &driveInfoMapIt = _gui->driveInfoMap().find(syncInfoClient->driveDbId());
    if (driveInfoMapIt == _gui->driveInfoMap().end()) {
        qCWarning(lcFolderItemWidget()) << "Drive not found in driveInfoMap for driveDbId=" << syncInfoClient->driveDbId();
        return nullptr;
    }

    return &(driveInfoMapIt->second);
}


const UserInfoClient *FolderItemWidget::getUserInfoClient() const noexcept {
    if (const auto syncInfoClient = getSyncInfoClient(); !syncInfoClient) {
        return nullptr;
    }

    const auto driveInfoClient = getDriveInfoClient();
    if (!driveInfoClient) {
        return nullptr;
    }

    const auto &accountInfoMapIt = _gui->accountInfoMap().find(driveInfoClient->accountDbId());
    if (accountInfoMapIt == _gui->accountInfoMap().end()) {
        qCWarning(lcFolderItemWidget()) << "Account not found in accountInfoMap for accountDbId="
                                        << driveInfoClient->accountDbId();
        return nullptr;
    }

    const auto userInfoMapIt = _gui->userInfoMap().find(accountInfoMapIt->second.userDbId());
    if (userInfoMapIt == _gui->userInfoMap().end()) {
        qCWarning(lcFolderItemWidget()) << "User not found in userInfoMap for userDbId=" << accountInfoMapIt->second.userDbId();
        return nullptr;
    }

    return &(userInfoMapIt->second);
}

void FolderItemWidget::onMenuButtonClicked() {
    const auto syncInfoClient = getSyncInfoClient();
    if (!syncInfoClient) {
        return;
    }

    const auto userInfoClient = getUserInfoClient();
    if (!userInfoClient) {
        return;
    }

    _menu.reset(new MenuWidget(MenuWidget::Menu, this));

    if (syncInfoClient->supportVfs()) {
        if (syncInfoClient->virtualFileMode() == VirtualFileMode::Off) {
            QWidgetAction *activateLitesyncAction = new QWidgetAction(this);
            MenuItemWidget *activateLitesyncMenuItemWidget = new MenuItemWidget(
#ifdef Q_OS_WIN
                    tr("Activate Lite Sync"));
#else
                    tr("Activate Lite Sync (Beta)"));
#endif
            activateLitesyncMenuItemWidget->setLeftIcon(menuIconPath(MenuAction::LiteSyncOn));
            activateLitesyncAction->setDefaultWidget(activateLitesyncMenuItemWidget);
            activateLitesyncAction->setDisabled(!userInfoClient->connected());
            connect(activateLitesyncAction, &QWidgetAction::triggered, this, &FolderItemWidget::onActivateLitesyncTriggered);
            _menu->addAction(activateLitesyncAction);
        } else {
            QWidgetAction *deactivateLitesyncAction = new QWidgetAction(this);
            MenuItemWidget *deactivateLitesyncMenuItemWidget = new MenuItemWidget(tr("Deactivate Lite Sync"));
            deactivateLitesyncMenuItemWidget->setLeftIcon(menuIconPath(MenuAction::LiteSyncOff));
            deactivateLitesyncAction->setDefaultWidget(deactivateLitesyncMenuItemWidget);
            deactivateLitesyncAction->setDisabled(!userInfoClient->connected());
            connect(deactivateLitesyncAction, &QWidgetAction::triggered, this, &FolderItemWidget::onDeactivateLitesyncTriggered);
            _menu->addAction(deactivateLitesyncAction);
        }
    }

    if (KDC::GuiUtility::getPauseActionAvailable(syncInfoClient->status())) {
        QWidgetAction *pauseAction = new QWidgetAction(this);
        MenuItemWidget *pauseMenuItemWidget = new MenuItemWidget(tr("Pause synchronization"));
        pauseMenuItemWidget->setLeftIcon(menuIconPath(MenuAction::Pause));
        pauseAction->setDefaultWidget(pauseMenuItemWidget);
        pauseAction->setDisabled(!userInfoClient->connected());
        connect(pauseAction, &QWidgetAction::triggered, this, &FolderItemWidget::onPauseTriggered);
        _menu->addAction(pauseAction);
    }

    if (KDC::GuiUtility::getResumeActionAvailable(syncInfoClient->status())) {
        QWidgetAction *resumeAction = new QWidgetAction(this);
        MenuItemWidget *resumeMenuItemWidget = new MenuItemWidget(tr("Resume synchronization"));
        resumeMenuItemWidget->setLeftIcon(menuIconPath(MenuAction::Resume));
        resumeAction->setDefaultWidget(resumeMenuItemWidget);
        resumeAction->setDisabled(!userInfoClient->connected());
        connect(resumeAction, &QWidgetAction::triggered, this, &FolderItemWidget::onResumeTriggered);
        _menu->addAction(resumeAction);
    }

    _menu->addSeparator();

    QWidgetAction *unsyncAction = new QWidgetAction(this);
    MenuItemWidget *unsyncMenuItemWidget = new MenuItemWidget(tr("Remove synchronization"));
    unsyncMenuItemWidget->setLeftIcon(menuIconPath(MenuAction::Remove));
    unsyncAction->setDefaultWidget(unsyncMenuItemWidget);
    connect(unsyncAction, &QWidgetAction::triggered, this, &FolderItemWidget::onUnsyncTriggered);
    _menu->addAction(unsyncAction);

    _menu->exec(QWidget::mapToGlobal(_menuButton->geometry().center()));
}

void FolderItemWidget::onExpandButtonClicked() {
    _isExpanded = !_isExpanded;
    setExpandButton();
    MatomoClient::sendEvent("folderItem", MatomoEventAction::Click, "expandButton", _isExpanded ? 1 : 0);
    emit displayFolderDetail(_syncDbId, _isExpanded);
}

void FolderItemWidget::onCancelButtonClicked() {
    MatomoClient::sendEvent("folderItem", MatomoEventAction::Click, "cancelButton");
    emit cancelUpdate(_syncDbId);
}

void FolderItemWidget::onValidateButtonClicked() {
    MatomoClient::sendEvent("folderItem", MatomoEventAction::Click, "validateButton");
    emit validateUpdate(_syncDbId);
}

void FolderItemWidget::onOpenFolder(const QString &link) {
    Q_UNUSED(link)

    const auto syncInfoClient = getSyncInfoClient();
    if (!syncInfoClient) {
        return;
    }

    emit openFolder(syncInfoClient->localPath());
}

void FolderItemWidget::onPauseTriggered() {
    MatomoClient::sendEvent("folderItem", MatomoEventAction::Click, "pauseSyncButton");
    emit pauseSync(_syncDbId);
}

void FolderItemWidget::onResumeTriggered() {
    MatomoClient::sendEvent("folderItem", MatomoEventAction::Click, "resumeSyncButton");
    emit resumeSync(_syncDbId);
}

void FolderItemWidget::onUnsyncTriggered() {
    if (auto syncInfoClient = getSyncInfoClient(); syncInfoClient) {
        syncInfoClient->setIsBeingDeleted(true);
    }
    MatomoClient::sendEvent("folderItem", MatomoEventAction::Click, "removeSyncButton");
    emit unSync(_syncDbId);
}

void FolderItemWidget::onDisplayFolderDetailCanceled() {
    _isExpanded = false;
    setExpandButton();
}

void FolderItemWidget::onActivateLitesyncTriggered() {
    MatomoClient::sendEvent("folderItem", MatomoEventAction::Click, "turnOnLiteSyncButton");
    emit triggerLiteSyncChanged(_syncDbId, true);
}

void FolderItemWidget::onDeactivateLitesyncTriggered() {
    MatomoClient::sendEvent("folderItem", MatomoEventAction::Click, "turnOffLiteSyncButton");
    emit triggerLiteSyncChanged(_syncDbId, false);
}

void FolderItemWidget::setToolTipsEnabled(bool enabled) noexcept {
    if (enabled) {
        _menuButton->setToolTip(tr("More actions"));
        _liteSyncIconLabel->setToolTip(_liteSyncActivated ? liteSyncActivatedQstr : liteSyncDeactivatedQstr);
    } else {
        _menuButton->setToolTip("");
        _liteSyncIconLabel->setToolTip("");
    }
}

void FolderItemWidget::retranslateUi() {
    const auto syncInfoClient = getSyncInfoClient();
    if (!syncInfoClient) {
        return;
    }

    setToolTipsEnabled(!isBeingDeleted());

    QString path = syncInfoClient->localPath();
    GuiUtility::makePrintablePath(path);
    _synchroLabel->setText(tr("Synchronized into <a style=\"%1\" href=\"ref\">%2</a>").arg(CommonUtility::linkStyle, path));

    if (ParametersCache::instance()->parametersInfo().moveToTrash()) {
        if (_liteSyncActivated) {
            _saveLabel->setText(
                    tr("Unselected folders will be moved to trash provided they contain offline items. Folders synced to kDrive "
                       "will remain available online."));
        } else {
            _saveLabel->setText(
                    tr("Unselected folders will be moved to trash. Folders synced to kDrive will remain available online."));
        }
    } else {
        _saveLabel->setText(tr(
                "Unselected folders will be <b>permanently</b> deleted from the computer. Folders synced to kDrive will remain "
                "available online."));
    }

    _cancelButton->setText(tr("Cancel"));
    _validateButton->setText(tr("VALIDATE"));
}

} // namespace KDC
