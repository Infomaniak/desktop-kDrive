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

#include "adddriveslitesyncwidget.h"
#include "custommessagebox.h"
#include "guiutility.h"
#include "config.h"
#include "libcommon/utility/utility.h"

#include <QBoxLayout>
#include <QDesktopServices>
#include <QDir>
#include <QLoggingCategory>
#include <QProgressBar>
#include <QPropertyAnimation>

namespace KDC {

static const int boxHSpacing = 10;
static const int logoBoxVMargin = 20;
static const int progressBarVMargin = 50;
static const int hLogoSpacing = 20;
static const int logoIconSize = 39;
static const QSize logoTextIconSize = QSize(60, 42);
static const int titleBoxVMargin = 20;
static const int pictureBoxVMargin = 40;
static const int textBoxVMargin = 20;
static const int progressBarMin = 0;
static const int progressBarMax = 5;
static const QSize pictureIconSize = QSize(202, 140);
static const QSize checkIconSize = QSize(20, 20);

Q_LOGGING_CATEGORY(lcAddDriveLiteSyncWidget, "gui.adddrivesmartsyncwidget", QtInfoMsg)

AddDriveLiteSyncWidget::AddDriveLiteSyncWidget(QWidget *parent) : QWidget(parent) {
    initUI();
}

void AddDriveLiteSyncWidget::setButtonIcon(const QColor &value) {
    if (_backButton) {
        _backButton->setIcon(KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/chevron-left.svg", value));
    }
}

void AddDriveLiteSyncWidget::initUI() {
    auto *mainLayout = new QVBoxLayout();
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    setLayout(mainLayout);

    // Logo
    auto *logoHBox = new QHBoxLayout();
    logoHBox->setContentsMargins(0, 0, 0, 0);
    mainLayout->addLayout(logoHBox);
    mainLayout->addSpacing(logoBoxVMargin);

    auto *logoIconLabel = new QLabel(this);
    logoIconLabel->setPixmap(KDC::GuiUtility::getIconWithColor(":/client/resources/logos/kdrive-without-text.svg")
                                     .pixmap(QSize(logoIconSize, logoIconSize)));
    logoHBox->addWidget(logoIconLabel);
    logoHBox->addSpacing(hLogoSpacing);

    _logoTextIconLabel = new QLabel(this);
    logoHBox->addWidget(_logoTextIconLabel);
    logoHBox->addStretch();

    // Progress bar
    auto *progressBar = new QProgressBar(this);
    progressBar->setMinimum(progressBarMin);
    progressBar->setMaximum(progressBarMax);
    progressBar->setValue(1);
    progressBar->setFormat(QString());
    mainLayout->addWidget(progressBar);
    mainLayout->addSpacing(progressBarVMargin);

    // Picture
    auto *pictureHBox = new QHBoxLayout();
    pictureHBox->setContentsMargins(0, 0, 0, 0);
    pictureHBox->setSpacing(boxHSpacing);
    mainLayout->addLayout(pictureHBox);
    mainLayout->addSpacing(pictureBoxVMargin);

    auto *pictureIconLabel = new QLabel(this);
    pictureIconLabel->setPixmap(
            KDC::GuiUtility::getIconWithColor(":/client/resources/pictures/lite-sync.svg").pixmap(pictureIconSize));
    pictureIconLabel->setAlignment(Qt::AlignCenter);
    pictureHBox->addWidget(pictureIconLabel);

    // Title
    auto *titleLabel = new QLabel(this);
    titleLabel->setObjectName("titleLabel");
    titleLabel->setContentsMargins(0, 0, 0, 0);

#ifdef Q_OS_MAC
    titleLabel->setText(tr("Would you like to activate Lite Sync (Beta) ?"));
#else
    titleLabel->setText(tr("Would you like to activate Lite Sync ?"));
#endif

    titleLabel->setWordWrap(true);
    mainLayout->addWidget(titleLabel);
    mainLayout->addSpacing(titleBoxVMargin);

    // Text
    auto *textLabel = new QLabel(this);
    textLabel->setObjectName("largeMediumTextLabel");
    textLabel->setContentsMargins(0, 0, 0, 0);
    textLabel->setText(tr("Lite Sync syncs all your files without using your computer space."
                          " You can browse the files in your kDrive and download them locally whenever you want."
                          R"( <a style="%1" href="%2">Learn more</a>)")
                               .arg(CommonUtility::linkStyle, KDC::GuiUtility::learnMoreLink));
    textLabel->setWordWrap(true);
    mainLayout->addWidget(textLabel);
    mainLayout->addSpacing(textBoxVMargin);

    // Point 1
    auto *point1HBox = new QHBoxLayout();
    point1HBox->setContentsMargins(0, 0, 0, 0);
    point1HBox->setSpacing(boxHSpacing);
    mainLayout->addLayout(point1HBox);
    mainLayout->addSpacing(textBoxVMargin);

    auto *point1IconLabel = new QLabel(this);
    point1IconLabel->setPixmap(
            KDC::GuiUtility::getIconWithColor(":/client/resources/icons/statuts/success.svg").pixmap(checkIconSize));
    point1HBox->addWidget(point1IconLabel);

    auto *point1TextLabel = new QLabel(this);
    point1TextLabel->setObjectName("largeMediumTextLabel");
    point1TextLabel->setText(tr("Conserve your computer space"));
    point1TextLabel->setWordWrap(true);
    point1HBox->addWidget(point1TextLabel);
    point1HBox->setStretchFactor(point1TextLabel, 1);

    // Point 2
    auto *point2HBox = new QHBoxLayout();
    point2HBox->setContentsMargins(0, 0, 0, 0);
    point2HBox->setSpacing(boxHSpacing);
    mainLayout->addLayout(point2HBox);
    mainLayout->addSpacing(textBoxVMargin);
    mainLayout->addStretch();

    auto *point2IconLabel = new QLabel(this);
    point2IconLabel->setPixmap(
            KDC::GuiUtility::getIconWithColor(":/client/resources/icons/statuts/success.svg").pixmap(checkIconSize));
    point2HBox->addWidget(point2IconLabel);

    auto *point2TextLabel = new QLabel(this);
    point2TextLabel->setObjectName("largeMediumTextLabel");
    point2TextLabel->setText(tr("Decide which files should be available online or locally"));
    point2TextLabel->setWordWrap(true);
    point2HBox->addWidget(point2TextLabel);
    point2HBox->setStretchFactor(point2TextLabel, 1);

    // Add dialog buttons
    auto *buttonsHBox = new QHBoxLayout();
    buttonsHBox->setContentsMargins(0, 0, 0, 0);
    buttonsHBox->setSpacing(boxHSpacing);
    mainLayout->addLayout(buttonsHBox);

    _backButton = new QPushButton(this);
    _backButton->setObjectName("nondefaultbutton");
    _backButton->setFlat(true);
    buttonsHBox->addWidget(_backButton);
    buttonsHBox->addStretch();

    _laterButton = new QPushButton(this);
    _laterButton->setObjectName("nondefaultbutton");
    _laterButton->setFlat(true);
    _laterButton->setText(tr("LATER"));
    buttonsHBox->addWidget(_laterButton);

    _yesButton = new QPushButton(this);
    _yesButton->setObjectName("defaultbutton");
    _yesButton->setFlat(true);
    _yesButton->setText(tr("YES"));
    buttonsHBox->addWidget(_yesButton);

    connect(textLabel, &QLabel::linkActivated, this, &AddDriveLiteSyncWidget::onLinkActivated);
    connect(_backButton, &QPushButton::clicked, this, &AddDriveLiteSyncWidget::onBackButtonTriggered);
    connect(_laterButton, &QPushButton::clicked, this, &AddDriveLiteSyncWidget::onLaterButtonTriggered);
    connect(_yesButton, &QPushButton::clicked, this, &AddDriveLiteSyncWidget::onYesButtonTriggered);
}

void AddDriveLiteSyncWidget::onLinkActivated(const QString &link) {
    if (link == KDC::GuiUtility::learnMoreLink) {
        // Learn more: Lite Sync
        if (!QDesktopServices::openUrl(QUrl(LEARNMORE_LITESYNC_URL))) {
            qCWarning(lcAddDriveLiteSyncWidget) << "QDesktopServices::openUrl failed for " << link;
            CustomMessageBox msgBox(QMessageBox::Warning, tr("Unable to open link %1.").arg(link), QMessageBox::Ok, this);
            msgBox.exec();
        }
    }
}

void AddDriveLiteSyncWidget::setLogoColor(const QColor &color) {
    _logoColor = color;
    _logoTextIconLabel->setPixmap(KDC::GuiUtility::getIconWithColor(":/client/resources/logos/kdrive-text-only.svg", _logoColor)
                                          .pixmap(logoTextIconSize));
}

void AddDriveLiteSyncWidget::onBackButtonTriggered(bool checked) {
    Q_UNUSED(checked)

    emit terminated(false);
}

void AddDriveLiteSyncWidget::onLaterButtonTriggered(bool checked) {
    Q_UNUSED(checked)

    _liteSync = false;

    emit terminated();
}

void AddDriveLiteSyncWidget::onYesButtonTriggered(bool checked) {
    Q_UNUSED(checked)

    _liteSync = true;

    emit terminated();
}

} // namespace KDC
