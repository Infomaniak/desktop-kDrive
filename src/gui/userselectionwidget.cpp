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

#include "userselectionwidget.h"
#include "gui/menuitemuserwidget.h"
#include "menuitemwidget.h"
#include "menuwidget.h"
#include "guiutility.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSizePolicy>
#include <QWidgetAction>
#include <QFile>
#include <QTextStream>

namespace KDC {

static const int hMargin = 10;
static const int vMargin = 5;
static const int boxHSpacing = 7;
static const char userDbIdProperty[] = "userDbId";
static const int userIconSize = 32;

UserSelectionWidget::UserSelectionWidget(QWidget *parent) :
    QPushButton(parent), _downIconSize(QSize()), _downIconColor(QColor()), _menuRightIconSize(QSize()), _currentUserDbId(0),
    _userIconLabel(nullptr), _downIconLabel(nullptr) {
    setContentsMargins(hMargin, vMargin, hMargin, vMargin);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    QHBoxLayout *hbox = new QHBoxLayout();
    hbox->setContentsMargins(0, 0, 0, 0);
    hbox->setSpacing(boxHSpacing);
    setLayout(hbox);

    _userIconLabel = new QLabel(this);
    hbox->addWidget(_userIconLabel);
    hbox->addStretch();

    _downIconLabel = new QLabel(this);
    hbox->addWidget(_downIconLabel);

    connect(this, &UserSelectionWidget::clicked, this, &UserSelectionWidget::onClick);
}

QSize UserSelectionWidget::sizeHint() const {
    return QSize(_userIconLabel->sizeHint().width() + _downIconLabel->sizeHint().width() + 1 * boxHSpacing + 2 * hMargin,
                 _userIconLabel->sizeHint().height() + 2 * vMargin);
}

void UserSelectionWidget::clear() {
    _currentUserDbId = 0;
    _userMap.clear();
    _downIconLabel->setVisible(false);
}

void UserSelectionWidget::addOrUpdateUser(int userDbId, const UserInfo &userInfo) {
    _userMap[userDbId] = userInfo;
}

void UserSelectionWidget::selectUser(int userDbId) {
    if (_userMap.find(userDbId) != _userMap.end()) {
        _downIconLabel->setVisible(true);
        setUserAvatar(_userMap[userDbId].avatar());
        _currentUserDbId = userDbId;
        emit userSelected(userDbId);
    }
}

void UserSelectionWidget::setUserAvatar(const QImage &avatar) {
    if (_userIconLabel) {
        if (!avatar.isNull()) {
            _userIconLabel->setPixmap(KDC::GuiUtility::getAvatarFromImage(avatar).scaled(
                    userIconSize, userIconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
}

void UserSelectionWidget::setDownIcon() {
    if (_downIconLabel && _downIconSize != QSize() && _downIconColor != QColor()) {
        _downIconLabel->setPixmap(
                KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/chevron-down.svg", _downIconColor)
                        .pixmap(_downIconSize));
    }
}

void UserSelectionWidget::onClick(bool checked) {
    Q_UNUSED(checked)

    // Remove hover
    QApplication::sendEvent(this, new QEvent(QEvent::Leave));
    QApplication::sendEvent(this, new QEvent(QEvent::HoverLeave));

    MenuWidget *menu = new MenuWidget(MenuWidget::List, this);
    if (_userMap.size() > 0) {
        auto userMapIt = _userMap.find(_currentUserDbId);
        if (userMapIt != _userMap.end()) {
            addMenuItem(menu, userMapIt->second, true);
        }

        for (auto &userMapElt: _userMap) {
            UserInfo userInfo = userMapElt.second;
            if (userInfo.dbId() != _currentUserDbId) {
                addMenuItem(menu, userInfo, false);
            }
        }

        QWidgetAction *addUserAction = new QWidgetAction(this);
        MenuItemWidget *addDriveMenuItemWidget =
                new MenuItemWidget(tr("Add an account"), nullptr, MenuItemUserWidget::getMargins());
        addDriveMenuItemWidget->setLeftIcon(":/client/resources/icons/actions/add.svg");
        addUserAction->setDefaultWidget(addDriveMenuItemWidget);
        connect(addUserAction, &QWidgetAction::triggered, this, &UserSelectionWidget::onAddUserActionTriggered);
        menu->addAction(addUserAction);

        menu->exec(QWidget::mapToGlobal(rect().bottomLeft()));
    }
}

void UserSelectionWidget::addMenuItem(MenuWidget *menu, UserInfo &userInfo, bool current) {
    QWidgetAction *selectUserAction = new QWidgetAction(this);
    selectUserAction->setProperty(userDbIdProperty, userInfo.dbId());
    MenuItemUserWidget *userMenuItemWidget = new MenuItemUserWidget(userInfo.name(), userInfo.email(), current);
    userMenuItemWidget->setLeftImage(userInfo.avatar());

    selectUserAction->setDefaultWidget(userMenuItemWidget);
    menu->addAction(selectUserAction);
    connect(selectUserAction, &QAction::triggered, this, &UserSelectionWidget::onSelectUserActionTriggered);
}

void UserSelectionWidget::onAddUserActionTriggered(bool checked) {
    Q_UNUSED(checked)

    emit addUser();
}

void UserSelectionWidget::onSelectUserActionTriggered(bool checked) {
    Q_UNUSED(checked)

    int userDbId = qvariant_cast<int>(sender()->property(userDbIdProperty));

    selectUser(userDbId);
}

} // namespace KDC
