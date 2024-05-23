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

#include "adddriveconfirmationwidget.h"
#include "guiutility.h"

#include <QBoxLayout>
#include <QDir>
#include <QFileDialog>
#include <QLoggingCategory>
#include <QProgressBar>

namespace KDC {

static const int boxHSpacing = 10;
static const int logoBoxVMargin = 20;
static const int progressBarVMargin = 90;
static const QSize okIconSize = QSize(60, 60);
static const int okIconVMargin = 30;
static const int hLogoSpacing = 20;
static const int logoIconSize = 39;
static const QSize logoTextIconSize = QSize(60, 42);
static const int titleBoxVMargin = 30;
static const int descriptionVMargin = 50;
static const int buttonBoxVMargin = 20;
static const int progressBarMin = 0;
static const int progressBarMax = 5;

Q_LOGGING_CATEGORY(lcAddDriveConfirmationWidget, "gui.adddriveconfirmationwidget", QtInfoMsg)

AddDriveConfirmationWidget::AddDriveConfirmationWidget(QWidget *parent) : QWidget(parent) {
    initUI();
}

void AddDriveConfirmationWidget::setFolderPath(const QString &path) {
    QDir dir(path);
    _descriptionLabel->setText(
        tr("Synchronization will start and you will be able to add files to your %1 folder.").arg(dir.dirName()));
}

void AddDriveConfirmationWidget::initUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout();
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    setLayout(mainLayout);

    // Logo
    QHBoxLayout *logoHBox = new QHBoxLayout();
    logoHBox->setContentsMargins(0, 0, 0, 0);
    mainLayout->addLayout(logoHBox);
    mainLayout->addSpacing(logoBoxVMargin);

    QLabel *logoIconLabel = new QLabel(this);
    logoIconLabel->setPixmap(KDC::GuiUtility::getIconWithColor(":/client/resources/logos/kdrive-without-text.svg")
                                 .pixmap(QSize(logoIconSize, logoIconSize)));
    logoHBox->addWidget(logoIconLabel);
    logoHBox->addSpacing(hLogoSpacing);

    _logoTextIconLabel = new QLabel(this);
    logoHBox->addWidget(_logoTextIconLabel);
    logoHBox->addStretch();

    // Progress bar
    QProgressBar *progressBar = new QProgressBar(this);
    progressBar->setMinimum(progressBarMin);
    progressBar->setMaximum(progressBarMax);
    progressBar->setValue(5);
    progressBar->setFormat(QString());
    mainLayout->addWidget(progressBar);
    mainLayout->addSpacing(progressBarVMargin);

    // OK icon
    QLabel *okIconLabel = new QLabel(this);
    okIconLabel->setPixmap(KDC::GuiUtility::getIconWithColor(":/client/resources/icons/statuts/success.svg").pixmap(okIconSize));
    okIconLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(okIconLabel);
    mainLayout->addSpacing(okIconVMargin);

    // Title
    QLabel *titleLabel = new QLabel(this);
    titleLabel->setObjectName("titleLabel");
    titleLabel->setText(tr("Your kDrive is ready!"));
    titleLabel->setContentsMargins(0, 0, 0, 0);
    titleLabel->setWordWrap(true);
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);
    mainLayout->addSpacing(titleBoxVMargin);

    // Description
    _descriptionLabel = new QLabel(this);
    _descriptionLabel->setObjectName("largeNormalTextLabel");
    _descriptionLabel->setWordWrap(true);
    _descriptionLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(_descriptionLabel);
    mainLayout->addSpacing(descriptionVMargin);

    // Add dialog buttons
    QHBoxLayout *buttonsHBox = new QHBoxLayout();
    buttonsHBox->setContentsMargins(0, 0, 0, 0);
    buttonsHBox->setSpacing(boxHSpacing);
    mainLayout->addLayout(buttonsHBox);

    QPushButton *openFolderButton = new QPushButton(this);
    openFolderButton->setObjectName("defaultbutton");
    openFolderButton->setFlat(true);
    openFolderButton->setText(tr("OPEN FOLDER"));
    buttonsHBox->addStretch();
    buttonsHBox->addWidget(openFolderButton);

    QPushButton *openParametersButton = new QPushButton(this);
    openParametersButton->setObjectName("nondefaultbutton");
    openParametersButton->setFlat(true);
    openParametersButton->setText(tr("PARAMETERS"));
    buttonsHBox->addWidget(openParametersButton);
    buttonsHBox->addStretch();
    mainLayout->addSpacing(buttonBoxVMargin);

    QPushButton *addDriveButton = new QPushButton(this);
    addDriveButton->setObjectName("transparentbutton");
    addDriveButton->setFlat(true);
    addDriveButton->setText(tr("Synchronize another drive"));
    mainLayout->addWidget(addDriveButton);
    mainLayout->addStretch();

    connect(openFolderButton, &QPushButton::clicked, this, &AddDriveConfirmationWidget::onOpenFoldersButtonTriggered);
    connect(openParametersButton, &QPushButton::clicked, this, &AddDriveConfirmationWidget::onOpenParametersButtonTriggered);
    connect(addDriveButton, &QPushButton::clicked, this, &AddDriveConfirmationWidget::onAddDriveButtonTriggered);
}

void AddDriveConfirmationWidget::setLogoColor(const QColor &color) {
    _logoColor = color;
    _logoTextIconLabel->setPixmap(
        KDC::GuiUtility::getIconWithColor(":/client/resources/logos/kdrive-text-only.svg", _logoColor).pixmap(logoTextIconSize));
}

void AddDriveConfirmationWidget::onOpenFoldersButtonTriggered(bool checked) {
    Q_UNUSED(checked)

    _action = KDC::GuiUtility::WizardAction::OpenFolder;
    emit terminated();
}

void AddDriveConfirmationWidget::onOpenParametersButtonTriggered(bool checked) {
    Q_UNUSED(checked)

    _action = KDC::GuiUtility::WizardAction::OpenParameters;
    emit terminated();
}

void AddDriveConfirmationWidget::onAddDriveButtonTriggered(bool checked) {
    Q_UNUSED(checked)

    _action = KDC::GuiUtility::WizardAction::AddDrive;
    emit terminated();
}

}  // namespace KDC
