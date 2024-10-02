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

#include "mainmenubarwidget.h"
#include "guiutility.h"
#include "languagechangefilter.h"
#include "clientgui.h"

#include <QBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace KDC {

static const int hMargin = 10;
static const int vMargin = 0;
static const int driveBoxHMargin = 10;
static const int driveBoxVMargin = 10;
static const int hButtonsSpacing = 50;

MainMenuBarWidget::MainMenuBarWidget(std::shared_ptr<ClientGui> gui, QWidget *parent) :
    HalfRoundRectWidget(parent), _gui(gui), _driveSelectionWidget(nullptr), _progressBarWidget(nullptr), _helpButton(nullptr) {
    setContentsMargins(hMargin, vMargin, hMargin, vMargin);
    setSpacing(0);

    QVBoxLayout *vBox = new QVBoxLayout();
    vBox->setContentsMargins(0, 0, 0, 0);
    addLayout(vBox);

    // 1st line
    QHBoxLayout *hBox = new QHBoxLayout();
    hBox->setContentsMargins(driveBoxHMargin, driveBoxVMargin, driveBoxHMargin, driveBoxVMargin);
    vBox->addLayout(hBox);

    // Drive selection
    _driveSelectionWidget = new DriveSelectionWidget(_gui, this);
    hBox->addWidget(_driveSelectionWidget);
    hBox->addStretch();

    // Preferences button
    _preferencesButton = new QPushButton(this);
    _preferencesButton->setObjectName("preferencesButton");
    _preferencesButton->setFlat(true);
    hBox->addWidget(_preferencesButton);
    hBox->addSpacing(hButtonsSpacing);

    // Help button
    _helpButton = new CustomToolButton(this);
    _helpButton->setObjectName("helpButton");
    _helpButton->setIconPath(":/client/resources/icons/actions/help.svg");
    hBox->addWidget(_helpButton);

    // Quota line
    QHBoxLayout *quotaHBox = new QHBoxLayout();
    quotaHBox->setContentsMargins(0, 0, 0, 0);
    vBox->addLayout(quotaHBox);
    vBox->addStretch();

    // Progress bar
    _progressBarWidget = new ProgressBarWidget(this);
    quotaHBox->addWidget(_progressBarWidget);

    // Init labels and setup connection for on the fly translation
    retranslateUi();
    LanguageChangeFilter *languageFilter = new LanguageChangeFilter(this);
    installEventFilter(languageFilter);
    connect(languageFilter, &LanguageChangeFilter::retranslate, this, &MainMenuBarWidget::retranslateUi);

    connect(_driveSelectionWidget, &DriveSelectionWidget::driveSelected, this, &MainMenuBarWidget::onDriveSelected);
    connect(_driveSelectionWidget, &DriveSelectionWidget::addDrive, this, &MainMenuBarWidget::onAddDrive);
    connect(_preferencesButton, &QPushButton::clicked, this, &MainMenuBarWidget::onPreferencesButtonClicked);
    connect(_helpButton, &CustomToolButton::clicked, this, &MainMenuBarWidget::onHelpButtonClicked);
}

void MainMenuBarWidget::onDriveSelected(int id) {
    emit driveSelected(id);
}

void MainMenuBarWidget::onAddDrive() {
    emit addDrive();
}

void MainMenuBarWidget::onPreferencesButtonClicked(bool checked) {
    Q_UNUSED(checked)

    emit preferencesButtonClicked();
}

void MainMenuBarWidget::onHelpButtonClicked(bool checked) {
    Q_UNUSED(checked)

    emit openHelp();
}

void MainMenuBarWidget::retranslateUi() {
    _preferencesButton->setText(tr("Preferences"));
    _helpButton->setToolTip(tr("Help"));
}

} // namespace KDC
