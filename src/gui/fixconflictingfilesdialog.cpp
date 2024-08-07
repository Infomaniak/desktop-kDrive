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

#include "fixconflictingfilesdialog.h"
#include "custommessagebox.h"
#include "gui/fileitemwidget.h"
#include "parameterscache.h"
#include "customtoolbutton.h"
#include "guirequests.h"
#include "libcommon/theme/theme.h"

#include <algorithm>

#include <QDesktopServices>
#include <QLabel>
#include <QPushButton>
#include <QScrollBar>
#include <QStackedWidget>

namespace KDC {

// TODO : this should be unified in CustomDialog
static const int boxHMargin = 40;
static const int boxVMargin = 10;
static const int boxHSpacing = 10;
static const int titleBoxVMargin = 14;

static const QString learnMoreLink = "learnMoreLink";

FixConflictingFilesDialog::FixConflictingFilesDialog(int driveDbId, QWidget *parent /*= nullptr*/)
    : CustomDialog(true, parent), _driveDbId(driveDbId) {
    setModal(true);
    setResizable(true);
    GuiRequests::getConflictList(
        _driveDbId, {ConflictType::CreateCreate, ConflictType::EditEdit, ConflictType::MoveCreate, ConflictType::MoveMoveDest},
        _conflictList);
    initUi();
}

void FixConflictingFilesDialog::onLinkActivated(const QString &link) {
    if (link == learnMoreLink) {
        if (!QDesktopServices::openUrl(QUrl(Theme::instance()->conflictHelpUrl()))) {
            CustomMessageBox msgBox(QMessageBox::Warning, tr("Unable to open link %1.").arg(Theme::instance()->conflictHelpUrl()),
                                    QMessageBox::Ok, this);
            msgBox.exec();
        }
    }
}

void FixConflictingFilesDialog::onExpandButtonClicked() {
    if (_fileListWidget->isVisible()) {
        _expandButton->setIconPath(":/client/resources/icons/actions/chevron-down.svg");
        _stackedWidget->setCurrentIndex(0);
    } else {
        _expandButton->setIconPath(":/client/resources/icons/actions/chevron-up.svg");
        _stackedWidget->setCurrentIndex(1);
        if (_fileListWidget->count() == 0) {
            insertFileItems(20);
        }
    }
}

void FixConflictingFilesDialog::onScrollBarValueChanged() {
    if (_fileListWidget->verticalScrollBar()->value() > _fileListWidget->verticalScrollBar()->maximum() - 1000) {
        insertFileItems(20);
    }
}

void FixConflictingFilesDialog::onValidate() {
    _keepLocalVersion = _keepLocalButton->isChecked();
    accept();
}

void FixConflictingFilesDialog::initUi() {
    setObjectName("ResolveConflictsDialog");

    QVBoxLayout *mainLayout = this->mainLayout();
    mainLayout->setContentsMargins(boxHMargin, 0, boxHMargin, boxVMargin);

    // Title
    auto titleLabel = new QLabel(this);
    titleLabel->setObjectName("titleLabel");
    titleLabel->setText(tr("Solve conflict(s)"));

    mainLayout->addWidget(titleLabel);
    mainLayout->addSpacing(titleBoxVMargin);

    // Description
    auto textLabel = new QLabel(this);
    textLabel->setObjectName("textLabel");
    textLabel->setWordWrap(true);
    textLabel->setText(descriptionText());
    textLabel->setContextMenuPolicy(Qt::PreventContextMenu);
    connect(textLabel, &QLabel::linkActivated, this, &FixConflictingFilesDialog::onLinkActivated);

    mainLayout->addWidget(textLabel);

    // Selection buttons
    auto selectionLabel = new QLabel(this);
    selectionLabel->setObjectName("descriptionLabel");
    selectionLabel->setText(tr("<b>What do you want to do with the %1 conflicted item(s) that is(are) not synced in kDrive?</b>")
                                .arg(_conflictList.length()));

    _keepLocalButton = new CustomRadioButton(this);
    _keepLocalButton->setText(tr("Synchronize the local version of my item(s) in kDrive."));
    _keepLocalButton->setChecked(true);

    _keepRemoteButton = new CustomRadioButton(this);
    if (ParametersCache::instance()->parametersInfo().moveToTrash()) {
        _keepRemoteButton->setText(tr("Move the local version of my item(s) to the computer's trash."));
    } else {
        _keepRemoteButton->setText(tr("Permanently delete the local version of my item(s)."));
    }

    auto selectionLayout = new QVBoxLayout(this);
    selectionLayout->setContentsMargins(0, 4 * boxVMargin, 0, boxVMargin);
    selectionLayout->setSpacing(boxHSpacing);
    selectionLayout->addWidget(selectionLabel);
    selectionLayout->addWidget(_keepLocalButton);
    selectionLayout->addWidget(_keepRemoteButton);
    mainLayout->addLayout(selectionLayout);

    // Details
    auto detailsLabel = new QLabel(this);
    detailsLabel->setObjectName("descriptionLabel");
    detailsLabel->setText(tr("Show item(s)"));

    _expandButton = new CustomToolButton(this);
    _expandButton->setObjectName("expandButton");
    _expandButton->setIconPath(":/client/resources/icons/actions/chevron-down.svg");
    connect(_expandButton, &CustomToolButton::clicked, this, &FixConflictingFilesDialog::onExpandButtonClicked);

    auto detailsLabelLayout = new QHBoxLayout(this);
    detailsLabelLayout->addWidget(detailsLabel);
    detailsLabelLayout->addSpacing(5);
    detailsLabelLayout->addWidget(_expandButton);
    detailsLabelLayout->addStretch();

    _fileListWidget = new QListWidget(this);
    _fileListWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    _fileListWidget->setContentsMargins(0, boxVMargin, 0, boxVMargin);
    _fileListWidget->setSpacing(5);
    connect(_fileListWidget->verticalScrollBar(), &QScrollBar::valueChanged, this,
            &FixConflictingFilesDialog::onScrollBarValueChanged);

    _stackedWidget = new QStackedWidget(this);

    auto dummyWidget = new QWidget(this);
    dummyWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    _stackedWidget->insertWidget(0, dummyWidget);
    _stackedWidget->insertWidget(1, _fileListWidget);

    auto detailsLayout = new QVBoxLayout(this);
    detailsLayout->setContentsMargins(0, 2 * boxVMargin, 0, boxVMargin);
    detailsLayout->addLayout(detailsLabelLayout);
    detailsLayout->addWidget(_stackedWidget);
    mainLayout->addLayout(detailsLayout);

    // Validation buttons
    auto validateButton = new QPushButton(this);
    validateButton->setObjectName("defaultbutton");
    validateButton->setFlat(true);
    validateButton->setText(tr("VALIDATE"));
    connect(validateButton, &QPushButton::clicked, this, &FixConflictingFilesDialog::onValidate);

    auto cancelButton = new QPushButton(this);
    cancelButton->setObjectName("nondefaultbutton");
    cancelButton->setFlat(true);
    cancelButton->setText(tr("CANCEL"));
    connect(cancelButton, &QPushButton::clicked, this, &FixConflictingFilesDialog::reject);

    auto buttonsLayout = new QHBoxLayout();
    buttonsLayout->setContentsMargins(0, 0, 0, boxVMargin);
    buttonsLayout->setSpacing(boxHSpacing);
    buttonsLayout->addWidget(validateButton);
    buttonsLayout->addWidget(cancelButton);
    buttonsLayout->addStretch();
    mainLayout->addLayout(buttonsLayout);

    connect(this, &CustomDialog::exit, this, &FixConflictingFilesDialog::reject);
}

QString FixConflictingFilesDialog::descriptionText() const {
    QString str;
    str =
        tr("When an item has been modified on both the computer and the kDrive or when an item has been created on the computer "
           "with a name that already exists on the kDrive, "
           "kDrive renames your local item and downloads kDrive's version on your computer so as not to lose any data.<br>");
    str += tr("The local version of your item <b>is not synced</b> with kDrive. <a style=\"color: #489EF3\" href=\"%1\">Learn "
              "more</a>")
               .arg(learnMoreLink);

    return str;
}

void FixConflictingFilesDialog::insertFileItems(const int nbItems) {
    uint64_t max = std::min(_fileListWidget->count() + nbItems, (int)_conflictList.size());
    for (uint64_t i = _fileListWidget->count(); i < max; i++) {
        auto w = new FileItemWidget(_conflictList[i].destinationPath(), _conflictList[i].nodeType(), this);

        auto listWidgetItem = new QListWidgetItem();
        listWidgetItem->setFlags(Qt::NoItemFlags);
        listWidgetItem->setForeground(Qt::transparent);
        _fileListWidget->insertItem(i, listWidgetItem);

        listWidgetItem->setSizeHint(w->sizeHint());
        _fileListWidget->setItemWidget(listWidgetItem, w);
    }
}

}  // namespace KDC
