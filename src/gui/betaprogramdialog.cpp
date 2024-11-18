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

#include "betaprogramdialog.h"

#include "adddriveconfirmationwidget.h"
#include "utility/utility.h"

#include <QCheckBox>
#include <QDesktopServices>
#include <QPushButton>

static constexpr int mainLayoutHMargin = 40;
static constexpr int mainLayoutSpacing = 24;
static constexpr int titleBoxVSpacing = 14;
static constexpr int subLayoutSpacing = 8;
static constexpr int iconSize = 16;
static constexpr auto iconColor = QColor(239, 139, 52);

static const QString shareCommentsLink = "shareCommentsLink";

namespace KDC {

BetaProgramDialog::BetaProgramDialog(const bool isQuit, QWidget *parent /*= nullptr*/) :
    CustomDialog(true, parent), _isQuit(isQuit) {
    setObjectName("BetaProgramDialog");
    setMinimumHeight(380);

    /*
     * |--------------------------------------------------------|
     * |                        layout                          |
     * |                                                        |
     * |     |---------------------------------------------|    |
     * |     |              acknowledmentLayout            |    |
     * |     |                                             |    |
     * |     |---------------------------------------------|    |
     * |                                                        |
     * |     |---------------------------------------------|    |
     * |     |                buttonLayout                 |    |
     * |     |---------------------------------------------|    |
     * |--------------------------------------------------------|
     */

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(mainLayoutHMargin, 0, mainLayoutHMargin, 0);
    layout->setSpacing(mainLayoutSpacing);
    mainLayout()->addLayout(layout);

    // Title
    auto *titleLabel = new QLabel(this);
    titleLabel->setObjectName("titleLabel");
    titleLabel->setText(_isQuit ? tr("Quit the beta program") : tr("Join the beta program"));
    layout->addWidget(titleLabel);
    layout->addSpacing(titleBoxVSpacing);

    // Main text box
    if (!_isQuit) {
        auto *mainTextBox = new QLabel(this);
        mainTextBox->setObjectName("largeNormalTextLabel");
        mainTextBox->setText(tr(
                R"(Get early access to new versions of the application before they are released to the general public, and take )"
                R"(part in improving the application by sending us your comments.)"));
        mainTextBox->setWordWrap(true);
        layout->addWidget(mainTextBox);
    }

    // Acknowlegment
    auto *acknowlegmentFrame = new QFrame(this);
    acknowlegmentFrame->setObjectName("acknowlegmentFrame");
    acknowlegmentFrame->setStyleSheet("QFrame#acknowlegmentFrame {border-radius: 8px; background-color: #F4F6FC;}");
    layout->addWidget(acknowlegmentFrame);

    auto *acknowledmentLayout = new QGridLayout(this);
    acknowlegmentFrame->setLayout(acknowledmentLayout);
    acknowledmentLayout->setSpacing(subLayoutSpacing);

    auto *warningIcon = new QLabel(this);
    warningIcon->setPixmap(GuiUtility::getIconWithColor(":/client/resources/icons/actions/warning.svg", iconColor)
                                   .pixmap(QSize(iconSize, iconSize)));
    warningIcon->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    acknowledmentLayout->addWidget(warningIcon, 0, 0);

    auto *acknowledmentLabel = new QLabel(this);
    acknowledmentLabel->setObjectName("largeNormalTextLabel");
    if (_isQuit) {
        acknowledmentLabel->setText(
                tr("Your current version of the application might be too recent, so you won't be able to downgrade to a lower "
                   "version until an update is available."));
    } else {
        acknowledmentLabel->setText(tr("Beta versions may leave unexpectedly or cause instabilities."));
    }
    acknowledmentLabel->setWordWrap(true);
    acknowledmentLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    acknowledmentLayout->addWidget(acknowledmentLabel, 0, 1);

    _acknowledgmentCheckbox = new QCheckBox(tr("I understand"), this);
    acknowledmentLayout->addWidget(_acknowledgmentCheckbox, 1, 1);

    if (_isQuit) {
        auto *bottomTextBox = new QLabel(this);
        bottomTextBox->setObjectName("largeNormalTextLabel");
        bottomTextBox->setText(tr("Are you sure you want to leave the beta program?"));
        bottomTextBox->setWordWrap(true);
        layout->addWidget(bottomTextBox);
    }

    layout->addStretch();

    // Buttons
    auto *buttonLayout = new QHBoxLayout(this);
    layout->addItem(buttonLayout);
    buttonLayout->setSpacing(subLayoutSpacing);
    _saveButton = new QPushButton(this);
    _saveButton->setObjectName("defaultbutton");
    _saveButton->setFlat(true);
    _saveButton->setText(tr("Save"));
    _saveButton->setEnabled(false);
    buttonLayout->addWidget(_saveButton);
    auto *cancelButton = new QPushButton(this);
    cancelButton->setObjectName("nondefaultbutton");
    cancelButton->setFlat(true);
    cancelButton->setText(tr("Cancel"));
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addStretch();

    connect(_saveButton, &QPushButton::clicked, this, &BetaProgramDialog::onSave);
    connect(cancelButton, &QPushButton::clicked, this, &BetaProgramDialog::reject);
    connect(this, &BetaProgramDialog::exit, this, &BetaProgramDialog::reject);
    connect(_acknowledgmentCheckbox, &QCheckBox::clicked, this, &BetaProgramDialog::onAcknowledgement);
}

void BetaProgramDialog::onAcknowledgement() {
    _saveButton->setEnabled(_acknowledgmentCheckbox->isChecked());
}

void BetaProgramDialog::onSave() {
    if (_isQuit) {
        _channel = DistributionChannel::Prod;
    } else {
        // TODO : add Internal channel for collaborators
        _channel = DistributionChannel::Beta;
    }

    accept();
}

} // namespace KDC
