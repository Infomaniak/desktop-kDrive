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

#include "libcommon/theme/theme.h"
#include "libcommon/info/driveinfo.h"

#include <QFont>
#include <QColor>
#include <QLabel>
#include <QList>
#include <QPushButton>
#include <QSize>
#include <QString>

namespace KDC {

class ClientGui;

class DriveSelectionWidget : public QPushButton {
        Q_OBJECT

        Q_PROPERTY(QSize drive_icon_size READ driveIconSize WRITE setDriveIconSize)
        Q_PROPERTY(QSize down_icon_size READ downIconSize WRITE setDownIconSize)
        Q_PROPERTY(QColor down_icon_color READ downIconColor WRITE setDownIconColor)
        Q_PROPERTY(QSize menu_right_icon_size READ menuRightIconSize WRITE setMenuRightIconSize)

    public:
        explicit DriveSelectionWidget(std::shared_ptr<ClientGui> gui, QWidget *parent = nullptr);
        QSize sizeHint() const override;

        void clear();
        void selectDrive(int driveDbId);

    signals:
        void driveSelected(int driveDbId);
        void addDrive();

    private:
        int _currentDriveDbId;
        std::shared_ptr<ClientGui> _gui;

        QSize _driveIconSize;
        QSize _downIconSize;
        QColor _downIconColor;
        QSize _menuRightIconSize;
        QLabel *_driveIconLabel;
        QLabel *_driveTextLabel;
        QLabel *_downIconLabel;

        inline QSize driveIconSize() const { return _driveIconSize; }
        inline void setDriveIconSize(QSize size) {
            _driveIconSize = size;
            setDriveIcon();
        }

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

        void setDriveIcon();
        void setDriveIcon(const QColor &color);
        void setDownIcon();

        void showEvent(QShowEvent *event) override;

        void retranslateUi();

    private slots:
        void onClick(bool checked);
        void onSelectDriveActionTriggered(bool checked = false);
        void onAddDriveActionTriggered(bool checked = false);
};

}  // namespace KDC
