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

#include "adddriveserverfolderswidget.h"
#include "custommessagebox.h"
#include "guiutility.h"
#include "clientgui.h"
#include "libcommon/utility/utility.h"
#include "libcommongui/matomoclient.h"
#include "libcommongui/utility/utility.h"

#include <QBoxLayout>
#include <QDir>
#include <QLoggingCategory>
#include <QProgressBar>
#include <config.h>

namespace KDC {

static const int boxHSpacing = 10;
static const int logoBoxVMargin = 20;
static const int progressBarVMargin = 40;
static const int hLogoSpacing = 20;
static const int logoIconSize = 39;
static const QSize logoTextIconSize = QSize(60, 42);
static const int titleBoxVMargin = 20;
static const int availableSpaceBoxVMargin = 20;
static const int folderTreeBoxVMargin = 20;
static const int progressBarMin = 0;
static const int progressBarMax = 5;

Q_LOGGING_CATEGORY(lcAddDriveServerFoldersWidget, "gui.adddriveserverfolderswidget", QtInfoMsg)

AddDriveServerFoldersWidget::AddDriveServerFoldersWidget(std::shared_ptr<ClientGui> gui, QWidget *parent) :
    QWidget(parent),
    _gui(gui),
    _logoTextIconLabel(nullptr),
    _infoIconLabel(nullptr),
    _availableSpaceTextLabel(nullptr),
    _folderTreeItemWidget(nullptr),
    _backButton(nullptr),
    _continueButton(nullptr),
    _infoIconColor(QColor()),
    _infoIconSize(QSize()),
    _logoColor(QColor()),
    _needToSave(false) {
    initUI();
    updateUI();
}

void AddDriveServerFoldersWidget::init(int userDbId, const DriveAvailableInfo &driveInfo) {
    _driveInfo = driveInfo;
    _folderTreeItemWidget->setUserDbIdAndDriveInfo(userDbId, driveInfo);
    _folderTreeItemWidget->loadSubFolders();
}

qint64 AddDriveServerFoldersWidget::selectionSize() const {
    CustomTreeWidgetItem *root = static_cast<CustomTreeWidgetItem *>(_folderTreeItemWidget->topLevelItem(0));
    if (root) {
        return _folderTreeItemWidget->nodeSize(root);
    }
    return 0;
}

QSet<QString> AddDriveServerFoldersWidget::createBlackList() const {
    return _folderTreeItemWidget->createBlackSet();
}

QSet<QString> AddDriveServerFoldersWidget::createWhiteList() const {
    return _folderTreeItemWidget->createWhiteSet();
}

void AddDriveServerFoldersWidget::setButtonIcon(const QColor &value) {
    if (_backButton) {
        _backButton->setIcon(KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/chevron-left.svg", value));
    }
}

void AddDriveServerFoldersWidget::initUI() {
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
    progressBar->setValue(2);
    progressBar->setFormat(QString());
    mainLayout->addWidget(progressBar);
    mainLayout->addSpacing(progressBarVMargin);

    // Title
    QLabel *titleLabel = new QLabel(this);
    titleLabel->setObjectName("titleLabel");
    titleLabel->setContentsMargins(0, 0, 0, 0);
    titleLabel->setText(tr("Select kDrive folders to synchronize on your desktop"));
    titleLabel->setWordWrap(true);
    mainLayout->addWidget(titleLabel);
    mainLayout->addSpacing(titleBoxVMargin);

    // Available space
    QHBoxLayout *availableSpaceHBox = new QHBoxLayout();
    availableSpaceHBox->setContentsMargins(0, 0, 0, 0);
    availableSpaceHBox->setSpacing(boxHSpacing);
    mainLayout->addLayout(availableSpaceHBox);
    mainLayout->addSpacing(availableSpaceBoxVMargin);

    _infoIconLabel = new QLabel(this);
    availableSpaceHBox->addWidget(_infoIconLabel);

    _availableSpaceTextLabel = new QLabel(this);
    _availableSpaceTextLabel->setObjectName("largeMediumTextLabel");
    availableSpaceHBox->addWidget(_availableSpaceTextLabel);
    availableSpaceHBox->addStretch();

    // Folder tree
    QHBoxLayout *folderTreeHBox = new QHBoxLayout();
    folderTreeHBox->setContentsMargins(0, 0, 0, 0);
    mainLayout->addLayout(folderTreeHBox);
    mainLayout->addSpacing(folderTreeBoxVMargin);

    _folderTreeItemWidget = new FolderTreeItemWidget(_gui, true, this);
    folderTreeHBox->addWidget(_folderTreeItemWidget);
    mainLayout->setStretchFactor(_folderTreeItemWidget, 1);

    // Add dialog buttons
    QHBoxLayout *buttonsHBox = new QHBoxLayout();
    buttonsHBox->setContentsMargins(0, 0, 0, 0);
    buttonsHBox->setSpacing(boxHSpacing);
    mainLayout->addLayout(buttonsHBox);

    _backButton = new QPushButton(this);
    _backButton->setObjectName("nondefaultbutton");
    _backButton->setFlat(true);
    buttonsHBox->addWidget(_backButton);
    buttonsHBox->addStretch();

    _continueButton = new QPushButton(this);
    _continueButton->setObjectName("defaultbutton");
    _continueButton->setFlat(true);
    _continueButton->setText(tr("CONTINUE"));
    buttonsHBox->addWidget(_continueButton);

    connect(_folderTreeItemWidget, &FolderTreeItemWidget::terminated, this, &AddDriveServerFoldersWidget::onSubfoldersLoaded);
    connect(_folderTreeItemWidget, &FolderTreeItemWidget::needToSave, this, &AddDriveServerFoldersWidget::onNeedToSave);
    connect(_backButton, &QPushButton::clicked, this, &AddDriveServerFoldersWidget::onBackButtonTriggered);
    connect(_continueButton, &QPushButton::clicked, this, &AddDriveServerFoldersWidget::onContinueButtonTriggered);
}

void AddDriveServerFoldersWidget::updateUI() {
    // Available space
    qint64 freeBytes = KDC::CommonUtility::freeDiskSpace(dirSeparator);
    _availableSpaceTextLabel->setText(
            tr("Space available on your computer : %1").arg(CommonGuiUtility::octetsToString(freeBytes)));
}

void AddDriveServerFoldersWidget::setInfoIcon() {
    if (_infoIconLabel && _infoIconSize != QSize() && _infoIconColor != QColor()) {
        _infoIconLabel->setPixmap(
                KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/information.svg", _infoIconColor)
                        .pixmap(_infoIconSize));
    }
}

void AddDriveServerFoldersWidget::setLogoColor(const QColor &color) {
    _logoColor = color;
    _logoTextIconLabel->setPixmap(KDC::GuiUtility::getIconWithColor(":/client/resources/logos/kdrive-text-only.svg", _logoColor)
                                          .pixmap(logoTextIconSize));
}

void AddDriveServerFoldersWidget::onSubfoldersLoaded(const bool error, const ExitCause exitCause) {
    if (error) {
        QString errorMsg = tr("An error occurred while loading the list of subfolders.");
        if (exitCause == ExitCause::Http5xx) {
            QString driveLink = QString(APPLICATION_PREVIEW_URL).arg(_driveInfo.driveId()).arg("");
            errorMsg =
                    tr(R"(Impossible to load the list of subfolders. Your kDrive might be in maintenance.<br>)"
                       R"(Please, login to the <a style="%1" href="%2">web version</a> to check your kDrive's status, or contact your administrator.)")
                            .arg(CommonUtility::linkStyle, driveLink);
        }
        CustomMessageBox msgBox(QMessageBox::Warning, errorMsg, QMessageBox::Ok, this);
        msgBox.setDefaultButton(QMessageBox::Ok);
        msgBox.exec();
        emit terminated(false);
    }
}

void AddDriveServerFoldersWidget::onNeedToSave() {
    _needToSave = true;
}

void AddDriveServerFoldersWidget::onBackButtonTriggered(bool checked) {
    Q_UNUSED(checked)
    MatomoClient::sendEvent("addDriveServerFolders", MatomoEventAction::Click, "backButton");

    emit terminated(false);
}

void AddDriveServerFoldersWidget::onContinueButtonTriggered(bool checked) {
    Q_UNUSED(checked)
    MatomoClient::sendEvent("addDriveServerFolders", MatomoEventAction::Click, "continueButton");

    emit terminated();
}

} // namespace KDC
