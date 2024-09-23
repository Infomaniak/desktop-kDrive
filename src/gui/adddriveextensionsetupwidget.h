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

#include "guiutility.h"
#include "customextensionsetupwidget.h"

#include <QColor>
#include <QIcon>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSize>
#include <QTimer>


namespace KDC {

class AddDriveExtensionSetupWidget : public QWidget {
        Q_OBJECT

    public:
        explicit AddDriveExtensionSetupWidget(QWidget *parent = nullptr);

        inline KDC::GuiUtility::WizardAction action() { return _action; }

    signals:
        void terminated(bool next = true);

    private:
        QLabel *_logoTextIconLabel;
        QLabel *_descriptionLabel;
        QColor _logoColor;
        QLabel *_step1Label;
        QLabel *_step2Label;
        QPushButton *_nextButton;
        QTimer *_timer;
        KDC::GuiUtility::WizardAction _action;

        CustomExtensionSetupWidget *_extensionSetupWidget;


    private slots:
        void onContinueButtonTriggered();
};

} // namespace KDC
