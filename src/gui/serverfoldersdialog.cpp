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

#include "serverfoldersdialog.h"
#include "custommessagebox.h"
#include "guiutility.h"
#include "clientgui.h"

#include <QBoxLayout>
#include <QDir>
#include <QLoggingCategory>

namespace KDC {

static const int boxHMargin = 40;
static const int boxHSpacing = 10;
static const int titleBoxVMargin = 14;
static const int folderTreeBoxVMargin = 20;

Q_LOGGING_CATEGORY(lcServerFoldersDialog, "gui.serverfoldersdialog", QtInfoMsg)

ServerFoldersDialog::ServerFoldersDialog(std::shared_ptr<ClientGui> gui, int driveDbId, const QString &serverFolderName,
                                         const QString &serverFolderNodeId, QWidget *parent) :
    CustomDialog(true, parent),
    _gui(gui),
    _driveDbId(driveDbId),
    _serverFolderName(serverFolderName),
    _serverFolderNodeId(serverFolderNodeId),
    _folderTreeItemWidget(nullptr),
    _backButton(nullptr),
    _continueButton(nullptr),
    _needToSave(false) {
    initUI();
    updateUI();
}

qint64 ServerFoldersDialog::selectionSize() const {
    CustomTreeWidgetItem *root = static_cast<CustomTreeWidgetItem *>(_folderTreeItemWidget->topLevelItem(0));
    if (root) {
        return _folderTreeItemWidget->nodeSize(root);
    }
    return 0;
}

void ServerFoldersDialog::createBlackList() {
    _blacklist = _folderTreeItemWidget->createBlackSet();
}

QSet<QString> ServerFoldersDialog::getWhiteList() const {
    return _folderTreeItemWidget->createWhiteSet();
}

void ServerFoldersDialog::setButtonIcon(const QColor &value) {
    _backButton->setIcon(KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/chevron-left.svg", value));
}

void ServerFoldersDialog::initUI() {
    QVBoxLayout *mainLayout = this->mainLayout();

    // Title
    QLabel *titleLabel = new QLabel(this);
    titleLabel->setObjectName("titleLabel");
    titleLabel->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    titleLabel->setText(
            tr("The <b>%1</b> folder contains subfolders,<br> select the ones you want to synchronize").arg(_serverFolderName));
    mainLayout->addWidget(titleLabel);
    mainLayout->addSpacing(titleBoxVMargin);

    // Folder tree
    QHBoxLayout *folderTreeHBox = new QHBoxLayout();
    folderTreeHBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    mainLayout->addLayout(folderTreeHBox);
    mainLayout->addSpacing(folderTreeBoxVMargin);

    _folderTreeItemWidget = new FolderTreeItemWidget(_gui, false, this);

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
    buttonsHBox->addWidget(_continueButton);

    connect(_folderTreeItemWidget, &FolderTreeItemWidget::terminated, this, &ServerFoldersDialog::onSubfoldersLoaded);
    connect(_folderTreeItemWidget, &FolderTreeItemWidget::needToSave, this, &ServerFoldersDialog::onNeedToSave);
    connect(_backButton, &QPushButton::clicked, this, &ServerFoldersDialog::onBackButtonTriggered);
    connect(_continueButton, &QPushButton::clicked, this, &ServerFoldersDialog::onContinueButtonTriggered);
    connect(this, &CustomDialog::exit, this, &ServerFoldersDialog::onExit);
}

void ServerFoldersDialog::updateUI() {
    _folderTreeItemWidget->setDriveDbIdAndFolderNodeId(_driveDbId, _serverFolderNodeId);
    _folderTreeItemWidget->loadSubFolders();
}

void ServerFoldersDialog::onExit() {
    reject();
}

void ServerFoldersDialog::onBackButtonTriggered(bool checked) {
    Q_UNUSED(checked)

    done(-1);
}

void ServerFoldersDialog::onContinueButtonTriggered(bool checked) {
    Q_UNUSED(checked)
    createBlackList();
    if (!GuiUtility::checkBlacklistSize(_blacklist.size(), this)) return;
    accept();
}

void ServerFoldersDialog::onSubfoldersLoaded(bool, ExitCause, const bool empty) {
    FolderTreeItemWidget *folderTreeItemWidget = qobject_cast<FolderTreeItemWidget *>(sender());
    folderTreeItemWidget->setVisible(!empty);
    if (empty) {
        CustomMessageBox msgBox(QMessageBox::Warning, tr("No subfolders currently on the server."), QMessageBox::Ok, this);
        msgBox.setDefaultButton(QMessageBox::Ok);
        msgBox.exec();
    }
}

void ServerFoldersDialog::onNeedToSave() {
    _needToSave = true;
}

} // namespace KDC
