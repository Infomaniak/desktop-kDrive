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

#include "adddrivelistwidget.h"
#include "customwordwraplabel.h"
#include "driveitemwidget.h"
#include "guiutility.h"
#include "guirequests.h"
#include "clientgui.h"

#include <QBoxLayout>
#include <QDesktopServices>
#include <QListWidget>
#include <QProgressBar>
#include <QLoggingCategory>

namespace KDC {

static const std::string redirectionLink = "https://shop.infomaniak.com/order/select/drive";
static const int hLogoSpacing = 20;
static const int boxHSpacing = 10;
static const int logoIconSize = 39;
static const int titleLogoMargin = 40;
static const int titleBoxVMargin = 24;
static const int listBoxVMargin = 40;
static const QSize logoTextIconSize = QSize(60, 42);
static const int textSpacing = 12;
static const int imgKdriveSpacing = 87;
static const int labelKdriveMaxWidth = 538;

Q_LOGGING_CATEGORY(lcAddDriveListWidget, "gui.adddrivelistwidget", QtInfoMsg)

AddDriveListWidget::AddDriveListWidget(std::shared_ptr<ClientGui> gui, QWidget *parent)
    : QWidget(parent),
      _gui(gui),
      _userDbId(0),
      _withoutDrives(false),
      _driveInfo(DriveAvailableInfo()),
      _addUserClicked(false),
      _logoTextIconLabel(nullptr),
      _backButton(nullptr),
      _nextButton(nullptr),
      _userSelector(nullptr),
      _listWidget(nullptr),
      _titleLabel(nullptr),
      _logoColor(QColor()),
      _buttonColor(QColor()),
      _mainWithDriveWidget(nullptr),
      _mainWithoutDriveWidget(nullptr),
      _mainLayout(nullptr) {
    initUI();
}

void AddDriveListWidget::setDrivesData() {
    _listWidget->clear();

    ExitCode exitCode;
    QHash<int, DriveAvailableInfo> driveInfoList;
    exitCode = GuiRequests::getUserAvailableDrives(_userDbId, driveInfoList);
    if (exitCode != ExitCodeOk) {
        qCWarning(lcAddDriveListWidget()) << "Error in Requests::getUserAvailableDrives";
        return;
    }

    showUi(driveInfoList.empty());
    if (driveInfoList.empty()) {
        _withoutDrives = true;
    } else {
        // min 2 drives
        for (const auto &driveInfo : driveInfoList) {
            _listWidget->addItem(new DriveItemWidget(driveInfo, _listWidget));
        }

        _listWidget->setSelectionMode(QAbstractItemView::SingleSelection);

        // select the first drive
        _listWidget->setCurrentRow(0);
        _listWidget->sortItems();
        auto *driveItemWidget = dynamic_cast<DriveItemWidget *>(_listWidget->currentItem());
        _driveInfo = driveItemWidget->driveInfo();

        _nextButton->setEnabled(true);
    }
}

void AddDriveListWidget::setUsersData() {
    for (const auto &userInfoMapIt : _gui->userInfoMap()) {
        _userSelector->addOrUpdateUser(userInfoMapIt.first, userInfoMapIt.second);
    }
    _userSelector->selectUser(_userDbId);
}

void AddDriveListWidget::setLogoColor(const QColor &color) {
    _logoColor = color;
    _logoTextIconLabel->setPixmap(
        KDC::GuiUtility::getIconWithColor(":/client/resources/logos/kdrive-text-only.svg", _logoColor).pixmap(logoTextIconSize));
}

void AddDriveListWidget::initUI() {
    _mainLayout = new QVBoxLayout();
    _mainWithoutDriveWidget = new QWidget();
    _mainWithDriveWidget = new QWidget();
    setLayout(_mainLayout);
    _mainLayout->setContentsMargins(0, 0, 0, 0);
    _mainLayout->setSpacing(0);

    initMainUi();

    connect(_userSelector, &UserSelectionWidget::addUser, this, &AddDriveListWidget::onAddUser);
    connect(_userSelector, &UserSelectionWidget::userSelected, this, &AddDriveListWidget::onUserSelected);
}

void AddDriveListWidget::initMainUi() {
    // Logo
    QHBoxLayout *logoHBox = new QHBoxLayout();
    logoHBox->setContentsMargins(0, 0, 0, 0);
    _mainLayout->addLayout(logoHBox);

    QLabel *logoIconLabel = new QLabel(this);
    logoIconLabel->setPixmap(KDC::GuiUtility::getIconWithColor(":/client/resources/logos/kdrive-without-text.svg")
                                 .pixmap(QSize(logoIconSize, logoIconSize)));
    logoHBox->addWidget(logoIconLabel);
    logoHBox->addSpacing(hLogoSpacing);

    _logoTextIconLabel = new QLabel(this);
    logoHBox->addWidget(_logoTextIconLabel);
    logoHBox->addStretch();

    _userSelector = new UserSelectionWidget(this);
    logoHBox->addWidget(_userSelector);

    initMainWithDriveUi();
    initMainWithoutDriveUi();

    _mainWithoutDriveWidget->hide();
    _mainLayout->addWidget(_mainWithoutDriveWidget);
    _mainLayout->setStretchFactor(_mainWithoutDriveWidget, 1);

    _mainWithDriveWidget->hide();
    _mainLayout->addWidget(_mainWithDriveWidget);
    _mainLayout->setStretchFactor(_mainWithDriveWidget, 1);

    _mainLayout->addSpacing(listBoxVMargin);

    // Add dialog buttons
    QHBoxLayout *buttonsHBox = new QHBoxLayout();
    buttonsHBox->setContentsMargins(0, 0, 0, 0);
    buttonsHBox->setSpacing(boxHSpacing);
    _mainLayout->addLayout(buttonsHBox);

    _backButton = new QPushButton(this);
    _backButton->setObjectName("nondefaultbutton");
    _backButton->setFlat(true);
    _backButton->setText(tr("Cancel"));
    buttonsHBox->addWidget(_backButton);
    buttonsHBox->addStretch();

    _nextButton = new QPushButton(this);
    _nextButton->setObjectName("defaultbutton");
    _nextButton->setFlat(true);
    _nextButton->setText(tr("NEXT"));
    _nextButton->setEnabled(false);
    buttonsHBox->addWidget(_nextButton);

    connect(_listWidget, &QListWidget::itemPressed, this, &AddDriveListWidget::onWidgetPressed);
    connect(_backButton, &QPushButton::clicked, this, &AddDriveListWidget::onBackButtonTriggered);
    connect(_nextButton, &QPushButton::clicked, this, &AddDriveListWidget::onNextButtonTriggered);
}

void AddDriveListWidget::initMainWithDriveUi() {
    QVBoxLayout *mainListLayout = new QVBoxLayout();
    mainListLayout->setContentsMargins(0, 0, 0, 0);
    mainListLayout->setSpacing(0);
    _mainWithDriveWidget->setLayout(mainListLayout);
    mainListLayout->addSpacing(titleLogoMargin);

    // Title
    _titleLabel = new QLabel(this);
    _titleLabel->setObjectName("titleLabel");
    _titleLabel->setText(tr("Select the kDrive you want to synchronize"));
    _titleLabel->setWordWrap(true);
    mainListLayout->addWidget(_titleLabel);
    mainListLayout->addSpacing(titleBoxVMargin);

    _listWidget = new QListWidget();
    _listWidget->setObjectName("driveListWidget");
    _listWidget->setContentsMargins(0, 0, 0, 0);
    _listWidget->setSpacing(0);
    _listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mainListLayout->addWidget(_listWidget);
    mainListLayout->setStretchFactor(_listWidget, 1);
}

void AddDriveListWidget::initMainWithoutDriveUi() {
    QVBoxLayout *mainLayout = new QVBoxLayout();
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    _mainWithoutDriveWidget->setLayout(mainLayout);

    mainLayout->addSpacing(titleLogoMargin);

    mainLayout->addSpacing(imgKdriveSpacing);

    QVBoxLayout *kDriveFreeVBox = new QVBoxLayout();
    kDriveFreeVBox->setContentsMargins(56, 0, 56, 0);
    kDriveFreeVBox->setSpacing(0);
    mainLayout->addLayout(kDriveFreeVBox);

    QLabel *kDriveImgLabel = new QLabel(this);
    kDriveImgLabel->setPixmap(QPixmap(":/client/resources/pictures/img-kDrive2x.png").scaled(QSize(266, 140)));

    kDriveFreeVBox->addWidget(kDriveImgLabel);
    kDriveFreeVBox->setAlignment(kDriveImgLabel, Qt::AlignHCenter);
    kDriveFreeVBox->addSpacing(textSpacing);

    QLabel *titleFreeKdrive = new QLabel(this);
    titleFreeKdrive->setObjectName("chapterTitleLabel");
    titleFreeKdrive->setText(tr("Get kDrive for free"));
    titleFreeKdrive->setWordWrap(true);
    kDriveFreeVBox->addWidget(titleFreeKdrive);
    kDriveFreeVBox->setAlignment(titleFreeKdrive, Qt::AlignHCenter);

    kDriveFreeVBox->addSpacing(textSpacing);

    CustomWordWrapLabel *descrFreeKdrives = new CustomWordWrapLabel(this);
    descrFreeKdrives->setObjectName("descrLabel");
    descrFreeKdrives->setMaxWidth(labelKdriveMaxWidth);
    descrFreeKdrives->setText(
        tr("Store your pictures, documents and e-mails in Switzerland from an independent company that respects privacy. Learn "
           "more"));
    descrFreeKdrives->setAlignment(Qt::AlignHCenter);
    kDriveFreeVBox->addWidget(descrFreeKdrives);

    mainLayout->addStretch();
}

void AddDriveListWidget::showUi(bool withoutDrives) {
    if (withoutDrives) {
        changeNextButtonText();
        _mainWithoutDriveWidget->show();
    } else {
        _mainWithDriveWidget->show();
    }
}

void AddDriveListWidget::changeNextButtonText() {
    _nextButton->setText(tr("Test for free"));
}

void AddDriveListWidget::onBackButtonTriggered(bool checked) {
    Q_UNUSED(checked)

    emit terminated(false);
}

void AddDriveListWidget::onNextButtonTriggered(bool checked) {
    Q_UNUSED(checked)

    if (_withoutDrives) {
        QDesktopServices::openUrl(QUrl(QString::fromStdString(redirectionLink)));
    } else {
        emit terminated();
    }
}

void AddDriveListWidget::onWidgetPressed(QListWidgetItem *item) {
    Q_UNUSED(item)

    DriveItemWidget *driveItem = (DriveItemWidget *)item;
    _driveInfo = driveItem->driveInfo();
}

void AddDriveListWidget::onAddUser() {
    _addUserClicked = true;
    emit terminated(false);
}

void AddDriveListWidget::onUserSelected(int userDbId) {
    _userDbId = userDbId;
    setDrivesData();
}

}  // namespace KDC
