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

#include "gui/menuwidget.h"
#include "libcommon/info/userinfo.h"
#include "libcommon/theme/theme.h"

#include <map>

#include <QFont>
#include <QColor>
#include <QLabel>
#include <QList>
#include <QPushButton>
#include <QSize>
#include <QString>

namespace KDC {

class UserSelectionWidget : public QPushButton {
        Q_OBJECT

        Q_PROPERTY(QSize down_icon_size READ downIconSize WRITE setDownIconSize)
        Q_PROPERTY(QColor down_icon_color READ downIconColor WRITE setDownIconColor)
        Q_PROPERTY(QSize menu_right_icon_size READ menuRightIconSize WRITE setMenuRightIconSize)

    public:
        explicit UserSelectionWidget(QWidget *parent = nullptr);
        QSize sizeHint() const override;

        void clear();
        void addOrUpdateUser(int userDbId, const UserInfo &userInfo);
        void selectUser(int userDbId);

    signals:
        void userSelected(int userDbId);
        void addUser();

    private:
        QSize _downIconSize;
        QColor _downIconColor;
        QSize _menuRightIconSize;
        std::map<int, UserInfo> _userMap;
        int _currentUserDbId;
        QLabel *_userIconLabel;
        QLabel *_downIconLabel;

        void addMenuItem(MenuWidget *menu, UserInfo &userInfo, bool current);

        inline QSize downIconSize() const { return _downIconSize; }
        inline void setDownIconSize(QSize size) {
            _downIconSize = size;
            setDownIcon();
        }

        inline QColor downIconColor() const { return _downIconColor; }
        inline void setDownIconColor(QColor color) {
            _downIconColor = color;
            setDownIcon();
        }

        inline QSize menuRightIconSize() const { return _menuRightIconSize; }
        inline void setMenuRightIconSize(QSize size) { _menuRightIconSize = size; }

        void setUserAvatar(const QImage &avatar);
        void setDownIcon();

    private slots:
        void onClick(bool checked);
        void onAddUserActionTriggered(bool checked = false);
        void onSelectUserActionTriggered(bool checked = false);
};

} // namespace KDC
