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

#include "gui/userselectionwidget.h"
#include "libcommon/info/driveavailableinfo.h"

#include <QListWidget>
#include <QIcon>
#include <QColor>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace KDC {

class ClientGui;

class AddDriveListWidget : public QWidget {
        Q_OBJECT

        Q_PROPERTY(QColor logo_color READ logoColor WRITE setLogoColor)

    public:
        explicit AddDriveListWidget(std::shared_ptr<ClientGui> gui, QWidget *parent = nullptr);

        void setDrivesData();
        void setUsersData();
        inline void setUserDbId(int userDbId) { _userDbId = userDbId; }
        inline DriveAvailableInfo driveInfo() { return _driveInfo; }
        inline bool isAddUserClicked() const { return _addUserClicked; }
        inline void setAddUserClicked(bool addUserClicked) { _addUserClicked = addUserClicked; }
    signals:
        void exit();
        void terminated(bool next = true);
        void removeUser(int userDbId);

    private:
        std::shared_ptr<ClientGui> _gui;
        int _userDbId;
        bool _withoutDrives;
        DriveAvailableInfo _driveInfo;
        bool _addUserClicked;

        QLabel *_logoTextIconLabel;
        QPushButton *_backButton;
        QPushButton *_nextButton;
        UserSelectionWidget *_userSelector;
        QListWidget *_listWidget;
        QLabel *_titleLabel;
        QColor _logoColor;
        QColor _buttonColor;
        QWidget *_mainWithDriveWidget;
        QWidget *_mainWithoutDriveWidget;
        QVBoxLayout *_mainLayout;

        inline QColor logoColor() const { return _logoColor; }
        void setLogoColor(const QColor &color);

        void initUI();
        void initMainUi();
        void initMainWithDriveUi();
        void initMainWithoutDriveUi();
        void showUi(bool withoutDrives);
        void changeNextButtonText();

    private slots:
        void onBackButtonTriggered(bool checked = false);
        void onNextButtonTriggered(bool checked = false);
        void onWidgetPressed(QListWidgetItem *item);
        void onAddUser();
        void onUserSelected(int userDbId);
};

} // namespace KDC
