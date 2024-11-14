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
#include "libcommon/utility/utility.h"
#include "guiutility.h"

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
static const QString learnMoreMoveToTrashLink = "learnMoreLinkMoveToTrash";

FixConflictingFilesDialog::FixConflictingFilesDialog(int driveDbId, QWidget *parent /*= nullptr*/) :
    CustomDialog(true, parent), _driveDbId(driveDbId) {
    setModal(true);
    setResizable(true);
    GuiRequests::getConflictList(
            _driveDbId,
            {ConflictType::CreateCreate, ConflictType::EditEdit, ConflictType::MoveCreate, ConflictType::MoveMoveDest},
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

void FixConflictingFilesDialog::onKeepRemoteButtonToggled(bool checked) {
    _keepRemoteDisclaimerWidget->setVisible(checked);
}

void FixConflictingFilesDialog::initUi() {
    setObjectName("ResolveConflictsDialog");

    QVBoxLayout *mainLayout = this->mainLayout();
    mainLayout->setContentsMargins(boxHMargin, 0, boxHMargin, boxVMargin);

    // Title
    const auto titleLabel = new QLabel(this);
    titleLabel->setObjectName("titleLabel");
    titleLabel->setText(tr("Solve conflict(s)"));

    mainLayout->addWidget(titleLabel);
    mainLayout->addSpacing(titleBoxVMargin);

    // Description
    const auto textLabel = new QLabel(this);
    textLabel->setObjectName("textLabel");
    textLabel->setWordWrap(true);
    textLabel->setText(descriptionText());
    textLabel->setContextMenuPolicy(Qt::PreventContextMenu);
    connect(textLabel, &QLabel::linkActivated, this, &FixConflictingFilesDialog::onLinkActivated);

    mainLayout->addWidget(textLabel);

    // Selection buttons
    const auto selectionLabel = new QLabel(this);
    selectionLabel->setObjectName("descriptionLabel");
    selectionLabel->setText(tr("<b>What do you want to do with the %1 conflicted item(s)?</b>").arg(_conflictList.length()));

    _keepLocalButton = new CustomRadioButton(this);
    _keepLocalButton->setText(tr("Save my changes and replace other users' versions."));
    _keepLocalButton->setChecked(true);

    _keepRemoteButton = new CustomRadioButton(this);
    _keepRemoteButton->setText(tr("Undo my changes and keep other users' versions."));
    connect(_keepRemoteButton, &CustomRadioButton::toggled, this, &FixConflictingFilesDialog::onKeepRemoteButtonToggled);

    _keepRemoteDisclaimerWidget = new QWidget();
    _keepRemoteDisclaimerWidget->setStyleSheet("background-color: #F4F6FD; border-radius: 5px;");
    const auto keepLocalDisclaimerLayout = new QHBoxLayout(_keepRemoteDisclaimerWidget);
    const auto warningIconLabel = new QLabel();
    warningIconLabel->setPixmap(
            KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/warning.svg", QColor(255, 140, 0))
                    .pixmap(20, 20));
    warningIconLabel->setContentsMargins(0, 0, boxHMargin / 4, 0);
    keepLocalDisclaimerLayout->addWidget(warningIconLabel);

    const auto keepRemoteDisclaimerLabel = new QLabel();
    keepRemoteDisclaimerLabel->setWordWrap(true);
    keepLocalDisclaimerLayout->addWidget(keepRemoteDisclaimerLabel);


    if (ParametersCache::instance()->parametersInfo().moveToTrash()) {
        const auto keepRemoteDisclaimerLearnMoreLabel = new QLabel();
        connect(keepRemoteDisclaimerLearnMoreLabel, &QLabel::linkActivated, this, &FixConflictingFilesDialog::onLinkActivated);
        keepRemoteDisclaimerLabel->setText(
                tr("Your changes may be permanently deleted. They cannot be restored from the kDrive web application."));
        keepRemoteDisclaimerLearnMoreLabel->setText(
                tr("<a style=%1 href=\"%2\">Learn more</a>").arg(CommonUtility::linkStyle, learnMoreMoveToTrashLink));
        keepLocalDisclaimerLayout->addWidget(keepRemoteDisclaimerLearnMoreLabel);
    } else {
        keepRemoteDisclaimerLabel->setText(
                tr("Your changes will be permanently deleted. They cannot be restored from the kDrive web application."));
    }
    keepLocalDisclaimerLayout->setStretchFactor(keepRemoteDisclaimerLabel, 1);
    _keepRemoteDisclaimerWidget->hide();
    auto selectionLayout = new QVBoxLayout(this);
    selectionLayout->setContentsMargins(0, 4 * boxVMargin, 0, boxVMargin);
    selectionLayout->setSpacing(boxHSpacing);
    selectionLayout->addWidget(selectionLabel);
    selectionLayout->addWidget(_keepLocalButton);
    selectionLayout->addWidget(_keepRemoteButton);
    selectionLayout->addWidget(_keepRemoteDisclaimerWidget);
    mainLayout->addLayout(selectionLayout);

    // Details
    const auto detailsLabel = new QLabel(this);
    detailsLabel->setObjectName("descriptionLabel");
    detailsLabel->setText(tr("Show item(s)"));

    _expandButton = new CustomToolButton(this);
    _expandButton->setObjectName("expandButton");
    _expandButton->setIconPath(":/client/resources/icons/actions/chevron-down.svg");
    connect(_expandButton, &CustomToolButton::clicked, this, &FixConflictingFilesDialog::onExpandButtonClicked);

    const auto detailsLabelLayout = new QHBoxLayout(this);
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

    const auto dummyWidget = new QWidget(this);
    dummyWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    _stackedWidget->insertWidget(0, dummyWidget);
    _stackedWidget->insertWidget(1, _fileListWidget);

    const auto detailsLayout = new QVBoxLayout(this);
    detailsLayout->setContentsMargins(0, 2 * boxVMargin, 0, boxVMargin);
    detailsLayout->addLayout(detailsLabelLayout);
    detailsLayout->addWidget(_stackedWidget);
    mainLayout->addLayout(detailsLayout);

    // Validation buttons
    const auto validateButton = new QPushButton(this);
    validateButton->setObjectName("defaultbutton");
    validateButton->setFlat(true);
    validateButton->setText(tr("VALIDATE"));
    connect(validateButton, &QPushButton::clicked, this, &FixConflictingFilesDialog::onValidate);

    const auto cancelButton = new QPushButton(this);
    cancelButton->setObjectName("nondefaultbutton");
    cancelButton->setFlat(true);
    cancelButton->setText(tr("CANCEL"));
    connect(cancelButton, &QPushButton::clicked, this, &FixConflictingFilesDialog::reject);

    const auto buttonsLayout = new QHBoxLayout();
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
            tr("Modifications have been made to these files by several users in several places (online on kDrive, a computer or "
               "a mobile). Folders containing these files may also have been deleted.<br>");
    str += tr("The local version of your item <b>is not synced</b> with kDrive. <a style=\"color: #489EF3\" href=\"%1\">Learn "
              "more</a>")
                   .arg(learnMoreLink);

    return str;
}

void FixConflictingFilesDialog::insertFileItems(const int nbItems) {
    int max = (std::min)(_fileListWidget->count() + nbItems, static_cast<int>(_conflictList.size()));
    for (auto i = _fileListWidget->count(); i < max; i++) {
        const auto w = new FileItemWidget(_conflictList[i].destinationPath(), _conflictList[i].nodeType(), this);

        const auto listWidgetItem = new QListWidgetItem();
        listWidgetItem->setFlags(Qt::NoItemFlags);
        listWidgetItem->setForeground(Qt::transparent);
        _fileListWidget->insertItem(i, listWidgetItem);

        listWidgetItem->setSizeHint(w->sizeHint());
        _fileListWidget->setItemWidget(listWidgetItem, w);
    }
}

} // namespace KDC
