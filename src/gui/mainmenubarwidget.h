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

#pragma once

#include "halfroundrectwidget.h"
#include "customtoolbutton.h"
#include "driveselectionwidget.h"
#include "progressbarwidget.h"

#include <QLabel>
#include <QWidget>

namespace KDC {

class ClientGui;

class MainMenuBarWidget : public HalfRoundRectWidget {
        Q_OBJECT

    public:
        explicit MainMenuBarWidget(std::shared_ptr<ClientGui> gui, QWidget *parent = nullptr);

        inline DriveSelectionWidget *driveSelectionWidget() const { return _driveSelectionWidget; }
        inline ProgressBarWidget *progressBarWidget() const { return _progressBarWidget; }

    signals:
        void driveSelected(int accountId);
        void addDrive();
        void preferencesButtonClicked();
        void openHelp();

    private:
        std::shared_ptr<ClientGui> _gui;
        DriveSelectionWidget *_driveSelectionWidget;
        ProgressBarWidget *_progressBarWidget;
        QPushButton *_preferencesButton;
        CustomToolButton *_helpButton;

    private slots:
        void onDriveSelected(int id);
        void onAddDrive();
        void onPreferencesButtonClicked(bool checked = false);
        void onHelpButtonClicked(bool checked = false);
        void retranslateUi();
};

} // namespace KDC
