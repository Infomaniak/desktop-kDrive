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

#include "driveselectionwidget.h"
#include "menuitemwidget.h"
#include "menuwidget.h"
#include "guiutility.h"
#include "clientgui.h"
#include "libcommongui/matomoclient.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSizePolicy>
#include <QWidgetAction>
#include <QLoggingCategory>

namespace KDC {

static const int hMargin = 0;
static const int vMargin = 0;
static const int boxHMargin = 10;
static const int boxVMargin = 5;
static const int boxSpacing = 10;
static const char driveIdProperty[] = "driveId";
static const int driveNameMaxSize = 30;

Q_LOGGING_CATEGORY(lcDriveSelectionWidget, "gui.driveselectionwidget", QtInfoMsg)

DriveSelectionWidget::DriveSelectionWidget(std::shared_ptr<ClientGui> gui, QWidget *parent) :
    QPushButton(parent), _currentDriveDbId(0), _gui(gui), _driveIconSize(QSize()), _downIconSize(QSize()),
    _downIconColor(QColor()), _menuRightIconSize(QSize()), _driveIconLabel(nullptr), _driveTextLabel(nullptr),
    _downIconLabel(nullptr) {
    setContentsMargins(hMargin, vMargin, hMargin, vMargin);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    QHBoxLayout *hbox = new QHBoxLayout();
    hbox->setContentsMargins(boxHMargin, boxVMargin, boxHMargin, boxVMargin);
    hbox->setSpacing(boxSpacing);
    setLayout(hbox);

    _driveIconLabel = new QLabel(this);
    hbox->addWidget(_driveIconLabel);

    _driveTextLabel = new QLabel(this);
    _driveTextLabel->setObjectName("driveTextLabel");
    hbox->addWidget(_driveTextLabel);
    hbox->addStretch();

    _downIconLabel = new QLabel(this);
    hbox->addWidget(_downIconLabel);

    connect(this, &DriveSelectionWidget::clicked, this, &DriveSelectionWidget::onClick);
}

QSize DriveSelectionWidget::sizeHint() const {
    return QSize(_driveIconLabel->sizeHint().width() + _driveTextLabel->sizeHint().width() + _downIconLabel->sizeHint().width() +
                         2 * boxSpacing + 2 * boxHMargin,
                 QPushButton::sizeHint().height());
}

void DriveSelectionWidget::clear() {
    _currentDriveDbId = 0;
    _downIconLabel->setVisible(false);
    setDriveIcon();
    retranslateUi();
}

void DriveSelectionWidget::selectDrive(int driveDbId) {
    const auto driveInfoIt = _gui->driveInfoMap().find(driveDbId);
    if (driveInfoIt == _gui->driveInfoMap().end()) {
        qCWarning(lcDriveSelectionWidget()) << "Drive not found in drive map for driveDbId=" << driveDbId;
        clear();
        return;
    }

    QString driveName = driveInfoIt->second.name();
    GuiUtility::makePrintablePath(driveName, driveNameMaxSize);

    _currentDriveDbId = driveDbId;
    _driveTextLabel->setText(driveName);
    _downIconLabel->setVisible(true);
    setDriveIcon();
    emit driveSelected(driveDbId);
}

void DriveSelectionWidget::onClick(bool checked) {
    Q_UNUSED(checked)

    MatomoClient::sendEvent("driveSelection", MatomoEventAction::Click, "selectedDriveButton", _currentDriveDbId);
    // Remove hover
    QApplication::sendEvent(this, new QEvent(QEvent::Leave));
    QApplication::sendEvent(this, new QEvent(QEvent::HoverLeave));

    if (_gui->driveInfoMap().size() > 0) {
        MenuWidget *menu = new MenuWidget(MenuWidget::List, this);

        for (auto const &driveInfoMapElt: _gui->driveInfoMap()) {
            QWidgetAction *selectDriveAction = new QWidgetAction(this);
            selectDriveAction->setProperty(driveIdProperty, driveInfoMapElt.first);
            MenuItemWidget *driveMenuItemWidget = new MenuItemWidget(driveInfoMapElt.second.name());
            driveMenuItemWidget->setLeftIcon(":/client/resources/icons/actions/drive.svg", driveInfoMapElt.second.color());

            const auto &accountInfoMapIt = _gui->accountInfoMap().find(driveInfoMapElt.second.accountDbId());
            if (accountInfoMapIt == _gui->accountInfoMap().end()) {
                qCWarning(lcDriveSelectionWidget)
                        << "Account not found in account map for accountDbId=" << driveInfoMapElt.second.accountDbId();
                return;
            }

            const auto &userInfoMapIt = _gui->userInfoMap().find(accountInfoMapIt->second.userDbId());
            if (userInfoMapIt == _gui->userInfoMap().end()) {
                qCWarning(lcDriveSelectionWidget)
                        << "User not found in user map for userDbId=" << accountInfoMapIt->second.userDbId();
                return;
            }

            KDC::GuiUtility::StatusInfo statusInfo;
            statusInfo._disconnected = !userInfoMapIt->second.connected();
            statusInfo._status = driveInfoMapElt.second.status();
            driveMenuItemWidget->setRightIcon(QIcon(KDC::GuiUtility::getDriveStatusIconPath(statusInfo)), _menuRightIconSize);

            selectDriveAction->setDefaultWidget(driveMenuItemWidget);
            connect(selectDriveAction, &QWidgetAction::triggered, this, &DriveSelectionWidget::onSelectDriveActionTriggered);
            menu->addAction(selectDriveAction);
        }

        QWidgetAction *addDriveAction = new QWidgetAction(this);
        MenuItemWidget *addDriveMenuItemWidget = new MenuItemWidget(tr("Add a kDrive"));
        addDriveMenuItemWidget->setLeftIcon(":/client/resources/icons/actions/add.svg");
        addDriveAction->setDefaultWidget(addDriveMenuItemWidget);
        connect(addDriveAction, &QWidgetAction::triggered, this, &DriveSelectionWidget::onAddDriveActionTriggered);
        menu->addAction(addDriveAction);

        menu->exec(QWidget::mapToGlobal(rect().bottomLeft()));
    } else {
        onAddDriveActionTriggered();
    }
}

void DriveSelectionWidget::onSelectDriveActionTriggered(bool checked) {
    Q_UNUSED(checked)

    QString driveIdStr = qvariant_cast<QString>(sender()->property(driveIdProperty));
    int driveId = driveIdStr.toInt();
    if (driveId != _currentDriveDbId) {
        selectDrive(driveId);
        MatomoClient::sendEvent("driveSelection", MatomoEventAction::Click, "selectAnotherDriveButton", driveId);
    } else {
        MatomoClient::sendEvent("driveSelection", MatomoEventAction::Click, "selectSameDriveButton", driveId);
    }
}

void DriveSelectionWidget::onAddDriveActionTriggered(bool checked) {
    Q_UNUSED(checked)

    emit addDrive();
}

void DriveSelectionWidget::setDriveIcon() {
    if (_driveIconLabel) {
        if (_currentDriveDbId) {
            const auto driveInfoIt = _gui->driveInfoMap().find(_currentDriveDbId);
            if (driveInfoIt == _gui->driveInfoMap().end()) {
                qCWarning(lcDriveSelectionWidget()) << "Drive not found in drive map for driveDbId=" << _currentDriveDbId;
                return;
            }
            _driveIconLabel->setPixmap(
                    KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/drive.svg", driveInfoIt->second.color())
                            .pixmap(_driveIconSize));
        } else {
            _driveIconLabel->setPixmap(
                    KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/add.svg", _addIconColor)
                            .pixmap(_addIconSize));
        }
    }
}

void DriveSelectionWidget::setDownIcon() {
    if (_downIconLabel && _downIconSize != QSize() && _downIconColor != QColor()) {
        _downIconLabel->setPixmap(
                KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/chevron-down.svg", _downIconColor)
                        .pixmap(_downIconSize));
    }
}

void DriveSelectionWidget::showEvent(QShowEvent *event) {
    Q_UNUSED(event)

    retranslateUi();
}

void DriveSelectionWidget::retranslateUi() {
    if (_currentDriveDbId == 0) {
        _driveTextLabel->setText(tr("Synchronize a kDrive"));
    }
}

} // namespace KDC
