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

#include "statusbarwidget.h"
#include "menuitemwidget.h"
#include "guiutility.h"
#include "languagechangefilter.h"
#include "clientgui.h"

#include <QVBoxLayout>
#include <QStringList>
#include <QWidgetAction>
#include <QActionGroup>
#include <QLoggingCategory>

namespace KDC {

static const int hMargin = 15;
static const int vMargin = 15;
static const int statusIconSize = 24;

Q_LOGGING_CATEGORY(lcStatusBarWidget, "gui.statusbarwidget", QtInfoMsg)

StatusBarWidget::StatusBarWidget(std::shared_ptr<ClientGui> gui, QWidget *parent) :
    HalfRoundRectWidget(parent),
    _gui(gui),
    _driveDbId(0),
    _statusIconLabel(nullptr),
    _statusLabel(nullptr),
    _pauseButton(nullptr),
    _resumeButton(nullptr) {
    setContentsMargins(hMargin, vMargin, hMargin, vMargin);

    _statusIconLabel = new QLabel(this);
    _statusIconLabel->setVisible(false);
    addWidget(_statusIconLabel);

    QVBoxLayout *vBoxLayout = new QVBoxLayout();
    vBoxLayout->setContentsMargins(0, 0, 0, 0);
    vBoxLayout->setSpacing(0);
    addLayout(vBoxLayout);
    setStretchFactor(vBoxLayout, 1);

    _statusLabel = new QLabel(this);
    _statusLabel->setObjectName("statusLabel");
    _statusLabel->setWordWrap(true);
    _statusLabel->setContextMenuPolicy(Qt::PreventContextMenu);
    vBoxLayout->addWidget(_statusLabel);

    addStretch();

    _pauseButton = new CustomToolButton(this);
    _pauseButton->setIconPath(":/client/resources/icons/actions/pause.svg");
    _pauseButton->setVisible(false);
    addWidget(_pauseButton);

    _resumeButton = new CustomToolButton(this);
    _resumeButton->setIconPath(":/client/resources/icons/actions/start.svg");
    _resumeButton->setVisible(false);
    addWidget(_resumeButton);

    connect(_pauseButton, &CustomToolButton::clicked, this, &StatusBarWidget::onPauseClicked);
    connect(_resumeButton, &CustomToolButton::clicked, this, &StatusBarWidget::onResumeClicked);
    connect(_statusLabel, &QLabel::linkActivated, this, &StatusBarWidget::onLinkActivated);
}

void StatusBarWidget::setStatus(KDC::GuiUtility::StatusInfo &statusInfo) {
    _statusInfo = statusInfo;

    QString statusIconPath = KDC::GuiUtility::getSyncStatusIconPath(_statusInfo);
    _statusIconLabel->setPixmap(QIcon(statusIconPath).pixmap(QSize(statusIconSize, statusIconSize)));
    _statusIconLabel->setVisible(true);

    if (_statusInfo._disconnected) {
        _pauseButton->setDisabled(true);
        _resumeButton->setDisabled(true);
        _pauseButton->setVisible(true);
        _resumeButton->setVisible(true);
    } else {
        _pauseButton->setDisabled(false);
        _resumeButton->setDisabled(false);
        if (_statusInfo._status == KDC::SyncStatus::PauseAsked || _statusInfo._status == KDC::SyncStatus::StopAsked) {
            _pauseButton->setDisabled(true);
            _resumeButton->setDisabled(true);
        }

        _pauseButton->setVisible(false);
        _resumeButton->setVisible(false);
        if (_driveDbId) {
            for (const auto &sync: _gui->syncInfoMap()) {
                if (sync.second.driveDbId() != _driveDbId) {
                    continue;
                }

                _pauseButton->setVisible(_pauseButton->isVisible() ||
                                         KDC::GuiUtility::getPauseActionAvailable(sync.second.status()));
                _resumeButton->setVisible(_resumeButton->isVisible() ||
                                          KDC::GuiUtility::getResumeActionAvailable(sync.second.status()));
            }
        }
    }

    retranslateUi();
    repaint();
}

void StatusBarWidget::setCurrentDrive(int driveDbId) {
    _driveDbId = driveDbId;
}

void StatusBarWidget::setSeveralSyncs(bool severalSyncs) {
    _severalSyncs = severalSyncs;
    _pauseButton->setWithMenu(_severalSyncs);
    _resumeButton->setWithMenu(_severalSyncs);
}

void StatusBarWidget::reset() {
    _driveDbId = 0;
    KDC::GuiUtility::StatusInfo statusInfo;
    setStatus(statusInfo);
}

void StatusBarWidget::onLinkActivated(const QString &link) {
    emit linkActivated(link);
}

void StatusBarWidget::showStatusMenu(bool pauseClicked) {
    if (_severalSyncs) {
        MenuWidget *menu = new MenuWidget(MenuWidget::Menu, this);
        bool resetButtons = false;
        createStatusActionMenu(menu, resetButtons, pauseClicked);
        if (resetButtons) {
            _pauseButton->setVisible(false);
            _resumeButton->setVisible(false);
        }
    } else {
        _pauseButton->setVisible(false);
        _resumeButton->setVisible(false);

        if (pauseClicked) {
            emit pauseSync(ActionTarget::Drive);
        } else {
            emit resumeSync(ActionTarget::Drive);
        }
    }
}

void StatusBarWidget::onPauseClicked() {
    showStatusMenu(true);
}

void StatusBarWidget::onResumeClicked() {
    showStatusMenu(false);
}

void StatusBarWidget::createStatusActionMenu(MenuWidget *&menu, bool &resetButtons, bool pauseClicked) {
    const auto driveInfoIt = _gui->driveInfoMap().find(_driveDbId);
    if (driveInfoIt == _gui->driveInfoMap().end()) {
        qCWarning(lcStatusBarWidget()) << "Drive not found in drive map for driveDbId=" << _driveDbId;
        return;
    }

    QString resumeMenukDriveSyncStr = tr("Resume kDrive \"%1\" synchronization"); // add (s)
    QString resumeMenuSyncStr = tr("Resume synchronization");
    QString resumeMenuAllSyncStr = tr("Resume all kDrives synchronization");
    QString resumeMenuActionIcon = ":/client/resources/icons/actions/start.svg";

    QString pauseMenukDriveSyncStr = tr("Pause kDrive \"%1\" synchronization"); // add (s)
    QString pauseMenuSyncStr = tr("Pause synchronization");
    QString pauseMenuAllSyncStr = tr("Pause all kDrives synchronization");
    QString pauseMenuActionIcon = ":/client/resources/icons/actions/pause.svg";

    QString menukDriveSyncStr = resumeMenukDriveSyncStr; // add (s)
    QString menuSyncStr = resumeMenuSyncStr;
    QString menuAllSyncStr = resumeMenuAllSyncStr;
    QString menuActionIcon = resumeMenuActionIcon;

    if (pauseClicked) {
        menukDriveSyncStr = pauseMenukDriveSyncStr; // add (s)
        menuSyncStr = pauseMenuSyncStr;
        menuAllSyncStr = pauseMenuAllSyncStr;
        menuActionIcon = pauseMenuActionIcon;
    }

    QWidgetAction *widgetAction = new QWidgetAction(this);
    MenuItemWidget *actionMenuItemWidget =
            new MenuItemWidget(menukDriveSyncStr.arg(_driveDbId ? driveInfoIt->second.name() : QString()));
    actionMenuItemWidget->setLeftIcon(menuActionIcon);
    widgetAction->setDefaultWidget(actionMenuItemWidget);

    if (pauseClicked) {
        connect(widgetAction, &QWidgetAction::triggered, this, &StatusBarWidget::onPauseSync);
    } else {
        connect(widgetAction, &QWidgetAction::triggered, this, &StatusBarWidget::onResumeSync);
    }
    menu->addAction(widgetAction);

    if (_gui->syncInfoMap().size() > 1) {
        // filter to get all
        std::map<int, SyncInfoClient> syncOfCurrentDrive;
        for (auto const &syncInfoMapElt: _gui->syncInfoMap()) {
            if (syncInfoMapElt.second.driveDbId() == _driveDbId) {
                syncOfCurrentDrive.insert(syncInfoMapElt);
            }
        }

        // Pause folder
        if (syncOfCurrentDrive.size() > 1) {
            QWidgetAction *syncWidgetAction = new QWidgetAction(this);
            MenuItemWidget *syncActionMenuItemWidget = new MenuItemWidget(menuSyncStr);
            syncActionMenuItemWidget->setLeftIcon(menuActionIcon);
            syncActionMenuItemWidget->setHasSubmenu(true);
            syncWidgetAction->setDefaultWidget(syncActionMenuItemWidget);
            menu->addAction(syncWidgetAction);

            // Pause folder submenu
            MenuWidget *submenu = new MenuWidget(MenuWidget::Submenu, menu);

            QActionGroup *syncActionGroup = new QActionGroup(this);
            syncActionGroup->setExclusive(true);

            QWidgetAction *syncAction;
            for (auto const &syncInfoMapElt: syncOfCurrentDrive) {
                if (pauseClicked && (syncInfoMapElt.second.status() == SyncStatus::Stopped ||
                                     syncInfoMapElt.second.status() == SyncStatus::Paused)) {
                    continue;
                }

                if (!pauseClicked && (syncInfoMapElt.second.status() != SyncStatus::Stopped &&
                                      syncInfoMapElt.second.status() != SyncStatus::Paused)) {
                    continue;
                }

                syncAction = new QWidgetAction(this);
                syncAction->setProperty(MenuWidget::actionTypeProperty.c_str(), syncInfoMapElt.first);
                MenuItemWidget *syncMenuItemWidget = new MenuItemWidget(syncInfoMapElt.second.name());
                syncMenuItemWidget->setLeftIcon(":/client/resources/icons/actions/drive.svg");
                syncAction->setDefaultWidget(syncMenuItemWidget);

                if (pauseClicked) {
                    connect(syncAction, &QWidgetAction::triggered, this, &StatusBarWidget::onPauseFolderSync);
                } else {
                    connect(syncAction, &QWidgetAction::triggered, this, &StatusBarWidget::onResumeFolderSync);
                }
                syncActionGroup->addAction(syncAction);
            }

            submenu->addActions(syncActionGroup->actions());
            syncWidgetAction->setMenu(submenu);
        }

        QWidgetAction *allAction = new QWidgetAction(this);
        MenuItemWidget *allMenuItemWidget = new MenuItemWidget(menuAllSyncStr);
        allMenuItemWidget->setLeftIcon(menuActionIcon);
        allAction->setDefaultWidget(allMenuItemWidget);

        if (pauseClicked) {
            connect(allAction, &QWidgetAction::triggered, this, &StatusBarWidget::onPauseAllSync);
        } else {
            connect(allAction, &QWidgetAction::triggered, this, &StatusBarWidget::onResumeAllSync);
        }

        menu->addSeparator();
        menu->addAction(allAction);
        if (menu->exec(QWidget::mapToGlobal(pauseClicked ? _pauseButton->geometry().center()
                                                         : _resumeButton->geometry().center()))) {
            resetButtons = true;
        }
    }
}

void StatusBarWidget::showEvent(QShowEvent * /*event*/) {
    retranslateUi();
}

void StatusBarWidget::onPauseSync() {
    emit pauseSync(ActionTarget::Drive);
}

void StatusBarWidget::onPauseFolderSync() {
    int folderId = qvariant_cast<int>(sender()->property(MenuWidget::actionTypeProperty.c_str()));
    emit pauseSync(ActionTarget::Sync, folderId);
}

void StatusBarWidget::onPauseAllSync() {
    emit pauseSync(ActionTarget::AllDrives);
}

void StatusBarWidget::onResumeSync() {
    emit resumeSync(ActionTarget::Drive);
}

void StatusBarWidget::onResumeFolderSync() {
    int folderId = qvariant_cast<int>(sender()->property(MenuWidget::actionTypeProperty.c_str()));
    emit resumeSync(ActionTarget::Sync, folderId);
}

void StatusBarWidget::onResumeAllSync() {
    emit resumeSync(ActionTarget::AllDrives);
}

void StatusBarWidget::retranslateUi() {
    QString statusText = KDC::GuiUtility::getSyncStatusText(_statusInfo);
    _statusLabel->setText(statusText);

    _pauseButton->setToolTip(tr("Pause synchronization"));
    _resumeButton->setToolTip(tr("Resume synchronization"));
}


} // namespace KDC
