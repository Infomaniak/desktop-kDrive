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

#include "clientgui.h"
#include "serverbasefolderdialog.h"
#include "custommessagebox.h"
#include "guiutility.h"
#include "libcommon/utility/utility.h"
#include "libcommongui/utility/utility.h"

#include <QBoxLayout>
#include <QDir>

namespace KDC {

static const int boxHMargin = 40;
static const int boxHSpacing = 10;
static const int titleBoxVMargin = 14;
static const int descriptionBoxVMargin = 15;
static const int availableSpaceBoxVMargin = 20;
static const int folderTreeBoxVMargin = 20;

Q_LOGGING_CATEGORY(lcServerBaseFolderDialog, "gui.serverbasefolderdialog", QtInfoMsg)

ServerBaseFolderDialog::ServerBaseFolderDialog(std::shared_ptr<ClientGui> gui, int driveDbId, const QString &localFolderName,
                                               const QString &localFolderPath, QWidget *parent)
    : CustomDialog(true, parent),
      _gui(gui),
      _driveDbId(driveDbId),
      _localFolderName(localFolderName),
      _localFolderPath(localFolderPath),
      _infoIconLabel(nullptr),
      _availableSpaceTextLabel(nullptr),
      _folderTreeItemWidget(nullptr),
      _backButton(nullptr),
      _continueButton(nullptr),
      _infoIconColor(QColor()),
      _infoIconSize(QSize()),
      _okToContinue(false),
      _serverFolderBasePath(QString()),
      _serverFolderList(QList<QPair<QString, QString>>()) {
    initUI();
    updateUI();
}

void ServerBaseFolderDialog::setButtonIcon(const QColor &value) {
    _backButton->setIcon(KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/chevron-left.svg", value));
}

void ServerBaseFolderDialog::setInfoIcon() {
    if (_infoIconLabel && _infoIconSize != QSize() && _infoIconColor != QColor()) {
        _infoIconLabel->setPixmap(
            KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/information.svg", _infoIconColor)
                .pixmap(_infoIconSize));
    }
}

void ServerBaseFolderDialog::initUI() {
    QVBoxLayout *mainLayout = this->mainLayout();

    // Title
    QLabel *titleLabel = new QLabel(this);
    titleLabel->setObjectName("titleLabel");
    titleLabel->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    titleLabel->setText(tr("Select a folder on your kDrive"));
    mainLayout->addWidget(titleLabel);
    mainLayout->addSpacing(titleBoxVMargin);

    // Description
    QLabel *descriptionLabel = new QLabel(this);
    descriptionLabel->setObjectName("descriptionLabel");
    descriptionLabel->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    descriptionLabel->setText(
        tr("The content of the selected folder will be synchronized into the <b>%1</b> folder.").arg(_localFolderName));
    mainLayout->addWidget(descriptionLabel);
    mainLayout->addSpacing(descriptionBoxVMargin);

    // Available space
    QHBoxLayout *availableSpaceHBox = new QHBoxLayout();
    availableSpaceHBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
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
    folderTreeHBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    mainLayout->addLayout(folderTreeHBox);
    mainLayout->addSpacing(folderTreeBoxVMargin);

    _folderTreeItemWidget = new BaseFolderTreeItemWidget(_gui, _driveDbId, true, this);
    folderTreeHBox->addWidget(_folderTreeItemWidget);
    mainLayout->setStretchFactor(_folderTreeItemWidget, 1);

    // Add dialog buttons
    QHBoxLayout *buttonsHBox = new QHBoxLayout();
    buttonsHBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
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
    _continueButton->setEnabled(false);
    buttonsHBox->addWidget(_continueButton);

    connect(_folderTreeItemWidget, &BaseFolderTreeItemWidget::message, this, &ServerBaseFolderDialog::onDisplayMessage);
    connect(_folderTreeItemWidget, &BaseFolderTreeItemWidget::folderSelected, this, &ServerBaseFolderDialog::onFolderSelected);
    connect(_folderTreeItemWidget, &BaseFolderTreeItemWidget::noFolderSelected, this,
            &ServerBaseFolderDialog::onNoFolderSelected);
    connect(_backButton, &QPushButton::clicked, this, &ServerBaseFolderDialog::onBackButtonTriggered);
    connect(_continueButton, &QPushButton::clicked, this, &ServerBaseFolderDialog::onContinueButtonTriggered);
    connect(this, &CustomDialog::exit, this, &ServerBaseFolderDialog::onExit);
}

void ServerBaseFolderDialog::updateUI() {
    // Available space
    qint64 freeBytes = KDC::CommonUtility::freeDiskSpace(_localFolderPath);
    _availableSpaceTextLabel->setText(
        tr("Space available on your computer for the current folder : %1").arg(KDC::CommonGuiUtility::octetsToString(freeBytes)));

    _folderTreeItemWidget->loadSubFolders();
}

void ServerBaseFolderDialog::setOkToContinue(bool value) {
    _okToContinue = value;
    _continueButton->setEnabled(value);
}

QString ServerBaseFolderDialog::remotePathTrailingSlash(const QString &path) {
    QString newPath(path);
    if (!newPath.endsWith('/')) newPath.append('/');
    return newPath;
}

void ServerBaseFolderDialog::onExit() {
    reject();
}

void ServerBaseFolderDialog::onContinueButtonTriggered(bool checked) {
    Q_UNUSED(checked)

    QStringList warnStrings;

    QString folderPath;
    for (qsizetype i = 0; i < _serverFolderList.size(); i++) {
        folderPath += dirSeparator + _serverFolderList[i].first;
    }

    for (const auto &syncInfoMapIt : _gui->syncInfoMap()) {
        if (syncInfoMapIt.second.driveDbId() != _driveDbId) {
            continue;
        }

        QString currentFolderPath = remotePathTrailingSlash(syncInfoMapIt.second.targetPath());

        if (QDir::cleanPath(folderPath) == QDir::cleanPath(currentFolderPath)) {
            warnStrings.append(tr("This folder is already being synced."));
        }
    }

    if (warnStrings.size() > 0) {
        QString text = QString();
        for (const QString &warnString : warnStrings) {
            if (!text.isEmpty()) {
                text += "<br>";
            }
            text += warnString;
        }

        CustomMessageBox msgBox(QMessageBox::Warning, text, QMessageBox::NoButton, this);
        msgBox.addButton(tr("CONFIRM"), QMessageBox::Yes);
        msgBox.addButton(tr("CANCEL"), QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::No);
        int ret = msgBox.exec();
        if (ret != QDialog::Rejected) {
            if (ret == QMessageBox::Yes) {
                accept();
            }
        }
    } else {
        accept();
    }
}

void ServerBaseFolderDialog::onBackButtonTriggered(bool checked) {
    Q_UNUSED(checked)

    done(-1);
}

void ServerBaseFolderDialog::onDisplayMessage(const QString &text) {
    CustomMessageBox msgBox(QMessageBox::Warning, text, QMessageBox::Ok, this);
    msgBox.setDefaultButton(QMessageBox::Ok);
    msgBox.exec();
}

void ServerBaseFolderDialog::onFolderSelected(const QString &folderBasePath, const QList<QPair<QString, QString>> &folderList) {
    _serverFolderBasePath = folderBasePath;
    _serverFolderList = folderList;

    setOkToContinue(true);
}

void ServerBaseFolderDialog::onNoFolderSelected() {
    setOkToContinue(false);
}

}  // namespace KDC
