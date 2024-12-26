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
#include "customcombobox.h"
#include "guirequests.h"
#include "parameterscache.h"
#include "utility/utility.h"

#include <QCheckBox>
#include <QPushButton>

static constexpr int mainLayoutHMargin = 40;
static constexpr int mainLayoutSpacing = 24;
static constexpr int titleBoxVSpacing = 14;
static constexpr int subLayoutSpacing = 8;
static constexpr int iconSize = 16;
static constexpr auto iconColor = QColor(239, 139, 52);
static constexpr int indexNo = 0;
static constexpr int indexBeta = 1;
static constexpr int indexInternal = 2;

namespace KDC {

BetaProgramDialog::BetaProgramDialog(const bool isQuit, const bool isStaff, QWidget *parent /*= nullptr*/) :
    CustomDialog(true, parent), _isQuit(isQuit && !isStaff), _isStaff(isStaff) {
    setObjectName("BetaProgramDialog");

    /*
     * |-------------------------------------------------------|
     * |                       layout                          |
     * |                                                       |
     * |    |---------------------------------------------|    |
     * |    |              acknowledmentLayout            |    |
     * |    |                                             |    |
     * |    |---------------------------------------------|    |
     * |                                                       |
     * |    |---------------------------------------------|    |
     * |    |                buttonLayout                 |    |
     * |    |---------------------------------------------|    |
     * |-------------------------------------------------------|
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
        mainTextBox->setObjectName("largeNormalBoldTextLabel");
        mainTextBox->setText(tr(
                R"(Get early access to new versions of the application before they are released to the general public, and take )"
                R"(part in improving the application by sending us your comments.)"));
        mainTextBox->setWordWrap(true);
        layout->addWidget(mainTextBox);
    }

    if (_isStaff) {
        auto *staffLabel = new QLabel(this);
        staffLabel->setObjectName("largeMediumBoldTextLabel");
        staffLabel->setText(tr("Benefit from application beta updates"));
        layout->addWidget(staffLabel);

        _staffSelectionBox = new CustomComboBox(this);
        _staffSelectionBox->insertItem(indexNo, tr("No"));
        _staffSelectionBox->insertItem(indexBeta, tr("Public beta version"));
        _staffSelectionBox->insertItem(indexInternal, tr("Internal beta version"));

        switch (ParametersCache::instance()->parametersInfo().distributionChannel()) {
            case DistributionChannel::Prod:
                _initialIndex = indexNo;
                break;
            case DistributionChannel::Beta:
                _initialIndex = indexBeta;
                break;
            case DistributionChannel::Internal:
                _initialIndex = indexInternal;
                break;
            default:
                break;
        }
        _staffSelectionBox->setCurrentIndex(_initialIndex);
        layout->addWidget(_staffSelectionBox);

        connect(_staffSelectionBox, &CustomComboBox::currentIndexChanged, this, &BetaProgramDialog::onChannelChange);
    }

    // Acknowledgment
    _acknowledgmentFrame = new QFrame(this);
    _acknowledgmentFrame->setStyleSheet("QFrame {border-radius: 8px; background-color: #F4F6FC;}");
    _acknowledgmentFrame->setVisible(!_isStaff);
    layout->addWidget(_acknowledgmentFrame);

    auto *acknowledgmentLayout = new QGridLayout(this);
    _acknowledgmentFrame->setLayout(acknowledgmentLayout);
    acknowledgmentLayout->setSpacing(subLayoutSpacing);

    auto *warningIcon = new QLabel(this);
    warningIcon->setPixmap(GuiUtility::getIconWithColor(":/client/resources/icons/actions/warning.svg", iconColor)
                                   .pixmap(QSize(iconSize, iconSize)));
    warningIcon->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    acknowledgmentLayout->addWidget(warningIcon, 0, 0);

    _acknowledgmentLabel = new QLabel(this);
    _acknowledgmentLabel->setObjectName("largeNormalTextLabel");
    if (_isQuit) {
        setTooRecentMessage();
    } else {
        setInstabilityMessage();
    }
    _acknowledgmentLabel->setWordWrap(true);
    _acknowledgmentLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    acknowledgmentLayout->addWidget(_acknowledgmentLabel, 0, 1);

    _acknowledgmentCheckbox = new QCheckBox(tr("I understand"), this);
    acknowledgmentLayout->addWidget(_acknowledgmentCheckbox, 1, 1);

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
    connect(_acknowledgmentCheckbox, &QCheckBox::toggled, this, &BetaProgramDialog::onAcknowledgment);
}

void BetaProgramDialog::onAcknowledgment() {
    _saveButton->setEnabled(_acknowledgmentCheckbox->isChecked());
}

DistributionChannel toDistributionChannel(const int index) {
    switch (index) {
        case indexNo:
            return DistributionChannel::Prod;
        case indexBeta:
            return DistributionChannel::Beta;
        case indexInternal:
            return DistributionChannel::Internal;
        default:
            break;
    }
    return DistributionChannel::Unknown;
}

void BetaProgramDialog::onSave() {
    if (_isStaff) {
        _newChannel = toDistributionChannel(_staffSelectionBox->currentIndex());
    } else {
        if (_isQuit) {
            _newChannel = DistributionChannel::Prod;
        } else {
            _newChannel = DistributionChannel::Beta;
        }
    }

    accept();
}

void BetaProgramDialog::onChannelChange(const int index) {
    _acknowledgmentCheckbox->setChecked(false);
    if (_initialIndex == index) {
        _acknowledgmentFrame->setVisible(false);
        return;
    }

    _acknowledgmentFrame->setVisible(true);
    if (index > _initialIndex)
        setInstabilityMessage();
    else
        setTooRecentMessage();
}

void BetaProgramDialog::setTooRecentMessage() const {
    _acknowledgmentLabel->setText(
            tr("Your current version of the application may be too recent, your choice will be effective when the next update is "
               "available."));
}

void BetaProgramDialog::setInstabilityMessage() const {
    _acknowledgmentLabel->setText(tr("Beta versions may leave unexpectedly or cause instabilities."));
}

} // namespace KDC
