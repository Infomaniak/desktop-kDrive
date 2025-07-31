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

#include "localfolderdialog.h"
#include "customtoolbutton.h"
#include "guiutility.h"
#include "custommessagebox.h"
#include "config.h"
#include "enablestateholder.h"
#include "guirequests.h"
#include "libcommon/utility/utility.h"
#include "libcommongui/matomoclient.h"

#include <QBoxLayout>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QLabel>

namespace KDC {

static const int boxHMargin = 40;
static const int titleBoxVMargin = 14;
static const int descriptionBoxVMargin = 35;
static const int folderSelectionBoxVMargin = 30;
static const int selectionBoxHMargin = 15;
static const int selectionBoxVMargin = 20;
static const int selectionBoxSpacing = 10;
static const int warningBoxSpacing = 10;

Q_LOGGING_CATEGORY(lcLocalFolderDialog, "gui.localfolderdialog", QtInfoMsg)

LocalFolderDialog::LocalFolderDialog(std::shared_ptr<ClientGui> gui, const QString &localFolderPath, QWidget *parent) :
    CustomDialog(true, parent),
    _gui(gui),
    _localFolderPath(localFolderPath) {
    initUI();
    updateUI();
}

void LocalFolderDialog::initUI() {
    QVBoxLayout *mainLayout = this->mainLayout();

    // Title
    QLabel *titleLabel = new QLabel(this);
    titleLabel->setObjectName("titleLabel");
    titleLabel->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    titleLabel->setText(tr("Which folder on your computer would you like to<br>synchronize ?"));
    titleLabel->setWordWrap(true);
    mainLayout->addWidget(titleLabel);
    mainLayout->addSpacing(titleBoxVMargin);

    // Description
    QLabel *descriptionLabel = new QLabel(this);
    descriptionLabel->setObjectName("descriptionLabel");
    descriptionLabel->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    descriptionLabel->setText(tr("The content of this folder will be synchronized on the kDrive"));
    mainLayout->addWidget(descriptionLabel);
    mainLayout->addSpacing(descriptionBoxVMargin);

    QVBoxLayout *folderSelectionVBox = new QVBoxLayout();
    folderSelectionVBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    mainLayout->addLayout(folderSelectionVBox);
    mainLayout->addSpacing(folderSelectionBoxVMargin);

    // Folder selection widget
    _folderSelectionWidget = new QWidget(this);
    _folderSelectionWidget->setObjectName("folderSelectionWidget");
    folderSelectionVBox->addWidget(_folderSelectionWidget);

    QHBoxLayout *folderSelectionButtonHBox = new QHBoxLayout();
    folderSelectionButtonHBox->setContentsMargins(selectionBoxHMargin, selectionBoxVMargin, selectionBoxHMargin,
                                                  selectionBoxVMargin);
    _folderSelectionWidget->setLayout(folderSelectionButtonHBox);

    QPushButton *selectButton = new QPushButton(tr("Select a folder"), this);
    selectButton->setObjectName("selectButton");
    selectButton->setFlat(true);
    folderSelectionButtonHBox->addWidget(selectButton);
    folderSelectionButtonHBox->addStretch();

    // Folder selected widget
    _folderSelectedWidget = new QWidget(this);
    _folderSelectedWidget->setObjectName("folderSelectionWidget");
    folderSelectionVBox->addWidget(_folderSelectedWidget);

    QHBoxLayout *folderSelectedHBox = new QHBoxLayout();
    folderSelectedHBox->setContentsMargins(selectionBoxHMargin, selectionBoxVMargin, selectionBoxHMargin, selectionBoxVMargin);
    _folderSelectedWidget->setLayout(folderSelectedHBox);

    QVBoxLayout *folderSelectedVBox = new QVBoxLayout();
    folderSelectedVBox->setContentsMargins(0, 0, 0, 0);
    folderSelectedHBox->addLayout(folderSelectedVBox);
    folderSelectedHBox->addStretch();

    QHBoxLayout *folderNameSelectedHBox = new QHBoxLayout();
    folderNameSelectedHBox->setContentsMargins(0, 0, 0, 0);
    folderNameSelectedHBox->setSpacing(selectionBoxSpacing);
    folderSelectedVBox->addLayout(folderNameSelectedHBox);

    _folderIconLabel = new QLabel(this);
    folderNameSelectedHBox->addWidget(_folderIconLabel);

    _folderNameLabel = new QLabel(this);
    _folderNameLabel->setObjectName("foldernamelabel");
    folderNameSelectedHBox->addWidget(_folderNameLabel);
    folderNameSelectedHBox->addStretch();

    _folderPathLabel = new QLabel(this);
    _folderPathLabel->setObjectName("folderpathlabel");
    _folderPathLabel->setContextMenuPolicy(Qt::PreventContextMenu);
    folderSelectedVBox->addWidget(_folderPathLabel);

    CustomToolButton *updateButton = new CustomToolButton(this);
    updateButton->setIconPath(":/client/resources/icons/actions/edit.svg");
    updateButton->setToolTip(tr("Edit folder"));
    folderSelectedHBox->addWidget(updateButton);

    // Warning
    _warningWidget = new QWidget(this);
    _warningWidget->setVisible(false);
    mainLayout->addWidget(_warningWidget);
    mainLayout->addStretch();

    QVBoxLayout *warningVBox = new QVBoxLayout();
    warningVBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    _warningWidget->setLayout(warningVBox);

    QHBoxLayout *warningHBox = new QHBoxLayout();
    warningHBox->setContentsMargins(0, 0, 0, 0);
    warningHBox->setSpacing(warningBoxSpacing);
    warningVBox->addLayout(warningHBox);

    _warningIconLabel = new QLabel(this);
    warningHBox->addWidget(_warningIconLabel);

    _warningLabel = new QLabel(this);
    _warningLabel->setObjectName("largeMediumTextLabel");
    _warningLabel->setWordWrap(true);
    warningHBox->addWidget(_warningLabel);
    warningHBox->setStretchFactor(_warningLabel, 1);

    // Add dialog buttons
    QHBoxLayout *buttonsHBox = new QHBoxLayout();
    buttonsHBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    mainLayout->addLayout(buttonsHBox);

    QPushButton *cancelButton = new QPushButton(this);
    cancelButton->setObjectName("nondefaultbutton");
    cancelButton->setFlat(true);
    cancelButton->setText(tr("CANCEL"));
    buttonsHBox->addWidget(cancelButton);
    buttonsHBox->addStretch();

    _continueButton = new QPushButton(this);
    _continueButton->setObjectName("defaultbutton");
    _continueButton->setFlat(true);
    _continueButton->setText(tr("CONTINUE"));
    _continueButton->setEnabled(false);
    buttonsHBox->addWidget(_continueButton);

    connect(selectButton, &QPushButton::clicked, this, &LocalFolderDialog::onSelectFolderButtonTriggered);
    connect(updateButton, &CustomToolButton::clicked, this, &LocalFolderDialog::onUpdateFolderButtonTriggered);
    connect(_folderPathLabel, &QLabel::linkActivated, this, &LocalFolderDialog::onLinkActivated);
    connect(_continueButton, &QPushButton::clicked, this, &LocalFolderDialog::onContinueButtonTriggered);
    connect(cancelButton, &QPushButton::clicked, this, &LocalFolderDialog::onExit);
    connect(this, &CustomDialog::exit, this, &LocalFolderDialog::onExit);
}

void LocalFolderDialog::updateUI() {
    bool ok = !_localFolderPath.isEmpty();
    if (ok) {
        QDir dir(_localFolderPath);
        _folderNameLabel->setText(dir.dirName());
        _folderPathLabel->setText(
                QString("<a style=\"%1\" href=\"ref\">%2</a>").arg(CommonUtility::linkStyle).arg(_localFolderPath));
    }
    _folderSelectionWidget->setVisible(!ok);
    _folderSelectedWidget->setVisible(ok);

    if (_liteSync) {
        VirtualFileMode virtualFileMode;
        ExitCode exitCode = GuiRequests::bestAvailableVfsMode(virtualFileMode);
        if (exitCode != ExitCode::Ok) {
            qCWarning(lcLocalFolderDialog) << "Error in Requests::bestAvailableVfsMode";
            return;
        }

        if (virtualFileMode == VirtualFileMode::Win || virtualFileMode == VirtualFileMode::Mac) {
            // Check file system
            _folderCompatibleWithLiteSync =
                    ((virtualFileMode == VirtualFileMode::Win && CommonUtility::isNTFS(QStr2Path(_localFolderPath))) ||
                     (virtualFileMode == VirtualFileMode::Mac && CommonUtility::isAPFS(QStr2Path(_localFolderPath))));
            if (!_folderCompatibleWithLiteSync) {
                _warningLabel->setText(tr(R"(This folder is not compatible with Lite Sync.<br>
Please select another folder. If you continue Lite Sync will be disabled.<br>
<a style="%1" href="%2">Learn more</a>)")
                                               .arg(CommonUtility::linkStyle, KDC::GuiUtility::learnMoreLink));
                _warningWidget->setVisible(true);
            } else {
                _warningWidget->setVisible(false);
            }
        }
    }

    setOkToContinue(ok);
    forceRedraw();
}

void LocalFolderDialog::setOkToContinue(bool value) {
    _okToContinue = value;
    _continueButton->setEnabled(value);
}

void LocalFolderDialog::selectFolder(const QString &startDirPath) {
    EnableStateHolder _(this);

    QString dirPath = QFileDialog::getExistingDirectory(this, tr("Select folder"), startDirPath,
                                                        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!dirPath.isEmpty()) {
        if (!GuiUtility::warnOnInvalidSyncFolder(dirPath, _gui->syncInfoMap(), this)) {
            return;
        }

        QDir dir(dirPath);
        _localFolderPath = dir.canonicalPath();
        updateUI();
    }
}

void LocalFolderDialog::setFolderIcon() {
    if (_folderIconColor != QColor() && _folderIconSize != QSize()) {
        _folderIconLabel->setPixmap(
                KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/folder.svg", _folderIconColor)
                        .pixmap(_folderIconSize));
    }
}

void LocalFolderDialog::setWarningIcon() {
    if (_warningIconColor != QColor() && _warningIconSize != QSize()) {
        _warningIconLabel->setPixmap(
                KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/warning.svg", _warningIconColor)
                        .pixmap(_warningIconSize));
    }
}

void LocalFolderDialog::onExit() {
    MatomoClient::sendEvent("localFolderDialog", MatomoEventAction::Click, "cancelButton");
    reject();
}

void LocalFolderDialog::onContinueButtonTriggered(bool checked) {
    Q_UNUSED(checked)

    MatomoClient::sendEvent("localFolderDialog", MatomoEventAction::Click, "continueButton");
    accept();
}

void LocalFolderDialog::onSelectFolderButtonTriggered(bool checked) {
    Q_UNUSED(checked)

    MatomoClient::sendEvent("localFolderDialog", MatomoEventAction::Click, "selectFolderButton");
    selectFolder(QDir::homePath());
}

void LocalFolderDialog::onUpdateFolderButtonTriggered(bool checked) {
    Q_UNUSED(checked)

    selectFolder(_localFolderPath);
}

void LocalFolderDialog::onLinkActivated(const QString &link) {
    if (link == KDC::GuiUtility::learnMoreLink) {
        // Learn more: Folder not compatible with Lite Sync
        if (!QDesktopServices::openUrl(QUrl(LEARNMORE_LITESYNC_COMPATIBILITY_URL))) {
            qCWarning(lcLocalFolderDialog) << "QDesktopServices::openUrl failed for " << link;
            CustomMessageBox msgBox(QMessageBox::Warning, tr("Unable to open link %1.").arg(link), QMessageBox::Ok, this);
            msgBox.exec();
        }
    } else {
        emit openFolder(_localFolderPath);
    }
}

} // namespace KDC
