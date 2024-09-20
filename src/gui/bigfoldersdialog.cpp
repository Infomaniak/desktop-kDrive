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

#include "bigfoldersdialog.h"
#include "guiutility.h"
#include "customcheckbox.h"
#include "guirequests.h"

#include <QBoxLayout>
#include <QDir>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QLoggingCategory>

namespace KDC {

static const int boxHMargin = 40;
static const int boxHSpacing = 10;
static const int textVMargin = 20;
static const int undecidedListWidgetVMargin = 30;
static const int undecidedListHMargin = 15;
static const int undecidedListVMargin = 15;
static const int undecidedListBoxSpacing = 10;
static const int undecidedItemBoxHSpacing = 10;
static const int undecidedItemBoxVSpacing = 0;
static const int undecidedItemPathSpacing = 6;
static const int undecidedItemPathDriveSpacing = 4;
static const int driveIconSize = 14;

static const char undecidedFolderProperty[] = "undecidedFolder";

Q_LOGGING_CATEGORY(lcBigFoldersDialog, "gui.bigfoldersdialog", QtInfoMsg)

BigFoldersDialog::BigFoldersDialog(const std::unordered_map<int, std::pair<SyncInfoClient, QSet<QString>>> &syncsUndecidedMap,
                                   const DriveInfo &driveInfo, QWidget *parent) :
    CustomDialog(true, parent) {
    QVBoxLayout *mainLayout = this->mainLayout();

    // Text
    QHBoxLayout *textHBox = new QHBoxLayout();
    textHBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    mainLayout->addLayout(textHBox);
    mainLayout->addSpacing(textVMargin);

    QLabel *textLabel = new QLabel(this);
    textLabel->setObjectName("largeNormalTextLabel");
    textLabel->setText(
            (tr("Some folders were not synchronized because they are too large.\nSelect the ones you want to synchronize:")));
    textLabel->setWordWrap(true);
    textHBox->addWidget(textLabel);

    // Undecided list
    QHBoxLayout *undecidedListWidgetHBox = new QHBoxLayout();
    undecidedListWidgetHBox->setContentsMargins(boxHMargin, 0, boxHMargin, undecidedListWidgetVMargin);
    mainLayout->addLayout(undecidedListWidgetHBox);
    mainLayout->setStretchFactor(undecidedListWidgetHBox, 1);

    QWidget *undecidedListWidget = new QWidget(this);
    undecidedListWidget->setObjectName("undecidedListWidget");

    QScrollArea *undecidedListScrollArea = new QScrollArea(this);
    undecidedListScrollArea->setWidget(undecidedListWidget);
    undecidedListScrollArea->setWidgetResizable(true);
    undecidedListWidgetHBox->addWidget(undecidedListScrollArea);

    QVBoxLayout *undecidedListWidgetVBox = new QVBoxLayout();
    undecidedListWidgetVBox->setContentsMargins(undecidedListHMargin, undecidedListVMargin, undecidedListHMargin,
                                                undecidedListVMargin);
    undecidedListWidgetVBox->setSpacing(undecidedListBoxSpacing);
    undecidedListWidget->setLayout(undecidedListWidgetVBox);

    for (const auto &syncUndecidedListIt: syncsUndecidedMap) {
        int syncDbId = syncUndecidedListIt.first;

        for (const QString &nodeId: syncUndecidedListIt.second.second) {
            QString nodePath;
            ExitCode exitCode = GuiRequests::getNodePath(syncDbId, nodeId, nodePath);
            if (exitCode != ExitCode::Ok) {
                qCWarning(lcBigFoldersDialog()) << "Error in Requests::getNodePath";
                continue;
            }

            QDir nodeDir(nodePath);
            QString name = nodeDir.dirName();
            QString path = syncUndecidedListIt.second.first.name() + nodeDir.path();
            path.chop(name.size());

            QHBoxLayout *undecidedItemHBox = new QHBoxLayout();
            undecidedItemHBox->setContentsMargins(0, 0, 0, 0);
            undecidedItemHBox->setSpacing(undecidedItemBoxHSpacing);
            undecidedListWidgetVBox->addLayout(undecidedItemHBox);

            QLabel *folderIconLabel = new QLabel(this);
            folderIconLabel->setObjectName("folderIconLabel");
            undecidedItemHBox->addWidget(folderIconLabel);

            QVBoxLayout *undecidedItemVBox = new QVBoxLayout();
            undecidedItemVBox->setContentsMargins(0, 0, 0, 0);
            undecidedItemVBox->setSpacing(undecidedItemBoxVSpacing);
            undecidedItemHBox->addLayout(undecidedItemVBox);
            undecidedItemHBox->setStretchFactor(undecidedItemVBox, 1);

            QLabel *folderName = new QLabel(this);
            folderName->setObjectName("largeNormalBoldTextLabel");
            GuiUtility::makePrintablePath(name);
            folderName->setText(name);
            folderName->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
            undecidedItemVBox->addWidget(folderName);

            QHBoxLayout *undecidedItemPathHBox = new QHBoxLayout();
            undecidedItemPathHBox->setContentsMargins(0, 0, 0, 0);
            undecidedItemVBox->addLayout(undecidedItemPathHBox);

            QLabel *locationTextLabel = new QLabel(this);
            locationTextLabel->setObjectName("descriptionLabel");
            locationTextLabel->setText(tr("Location"));
            undecidedItemPathHBox->addWidget(locationTextLabel);
            undecidedItemPathHBox->addSpacing(undecidedItemPathSpacing);

            QLabel *driveIconLabel = new QLabel(this);
            driveIconLabel->setPixmap(
                    KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/drive.svg", driveInfo.color())
                            .pixmap(QSize(driveIconSize, driveIconSize)));
            undecidedItemPathHBox->addWidget(driveIconLabel);
            undecidedItemPathHBox->addSpacing(undecidedItemPathDriveSpacing);

            QLabel *locationPathLabel = new QLabel(this);
            locationPathLabel->setObjectName("folderPathLabel");
            GuiUtility::makePrintablePath(path);
            locationPathLabel->setText(path);
            locationPathLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
            undecidedItemPathHBox->addWidget(locationPathLabel);
            undecidedItemPathHBox->setStretchFactor(locationPathLabel, 1);

            undecidedItemHBox->addStretch();

            CustomCheckBox *checkBox = new CustomCheckBox(this);
            checkBox->setProperty(undecidedFolderProperty, nodeId);
            undecidedItemHBox->addWidget(checkBox);
            _mapCheckboxToFolder.insert(checkBox, syncDbId);
            _mapWhiteListedSubFolders[syncDbId].insert(nodeId, false); // Initialize all choices to false
            connect(checkBox, &CustomCheckBox::clicked, this, &BigFoldersDialog::slotCheckboxClicked);
        }
    }

    // Add dialog buttons
    QHBoxLayout *buttonsHBox = new QHBoxLayout();
    buttonsHBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    buttonsHBox->setSpacing(boxHSpacing);
    mainLayout->addLayout(buttonsHBox);

    QPushButton *saveButton = new QPushButton(this);
    saveButton->setObjectName("defaultbutton");
    saveButton->setFlat(true);
    saveButton->setText(tr("SAVE"));
    buttonsHBox->addWidget(saveButton);

    QPushButton *cancelButton = new QPushButton(this);
    cancelButton->setObjectName("nondefaultbutton");
    cancelButton->setFlat(true);
    cancelButton->setText(tr("CANCEL"));
    buttonsHBox->addWidget(cancelButton);
    buttonsHBox->addStretch();

    connect(saveButton, &QPushButton::clicked, this, &BigFoldersDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &BigFoldersDialog::reject);
    connect(this, &CustomDialog::exit, this, &BigFoldersDialog::reject);
}

const QHash<int, QHash<const QString, bool>> &BigFoldersDialog::mapWhiteListedSubFolders() const {
    return _mapWhiteListedSubFolders;
}

void BigFoldersDialog::slotCheckboxClicked() {
    CustomCheckBox *checkbox = qobject_cast<CustomCheckBox *>(sender());
    if (!checkbox) return;

    _mapWhiteListedSubFolders[_mapCheckboxToFolder[checkbox]].insert(checkbox->property(undecidedFolderProperty).toString(),
                                                                     checkbox->isChecked());
}

void BigFoldersDialog::setFolderIcon() {
    if (_folderIconSize != QSize() && _folderIconColor != QColor()) {
        QList<QLabel *> labelList = findChildren<QLabel *>("folderIconLabel");
        for (QLabel *label: labelList) {
            label->setPixmap(KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/folder.svg", _folderIconColor)
                                     .pixmap(_folderIconSize));
        }
    }
}

} // namespace KDC
