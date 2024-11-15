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

#include <QCheckBox>
#include <QPushButton>

static const int boxMargin = 40;
static const int boxHSpacing = 10;
static const int titleBoxVMargin = 14;

namespace KDC {

BetaProgramDialog::BetaProgramDialog(const bool isQuit /*= false*/, QWidget *parent /*= nullptr*/) : CustomDialog(true, parent) {
    setObjectName("BetaProgramDialog");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(boxMargin, boxMargin, boxMargin, boxMargin);
    layout->setSpacing(boxHSpacing);
    mainLayout()->addLayout(layout);

    // Title
    auto *titleLabel = new QLabel(this);
    titleLabel->setObjectName("titleLabel");
    titleLabel->setText(isQuit ? tr("Quit the beta program") : tr("Join the beta program"));
    layout->addWidget(titleLabel);
    layout->addSpacing(titleBoxVMargin);

    // Main text box
    if (!isQuit) {
        auto *mainTextBox = new QLabel(this);
        mainTextBox->setText(
                tr("Get early access to new versions of the application before they are released to the general public, and take "
                   "part in improving the application by sending us your comments."));
        mainTextBox->setObjectName("textLabel");
        mainTextBox->setWordWrap(true);
        layout->addWidget(mainTextBox);
    }

    // TODO : change background
    auto *acknowlegmentWidget = new QWidget(this);
    auto *acknowledmentLayout = new QGridLayout(acknowlegmentWidget);
    acknowlegmentWidget->setLayout(acknowledmentLayout);
    acknowlegmentWidget->setObjectName("acknowledgmentWidget");
    acknowlegmentWidget->setStyleSheet(
            R"({border-radius: 8px; background-color: #F4F6FC; font-family: "Suisse Int'l"; font-weight: $$normal_font$$; font-size: 14px;})");

    acknowledmentLayout->setSpacing(8);
    layout->addWidget(acknowlegmentWidget);
    auto *warningIcon = new QLabel(this);
    warningIcon->setPixmap(GuiUtility::getIconWithColor(":/client/resources/icons/actions/warning.svg", QColor(239, 139, 52))
                                   .pixmap(QSize(16, 16)));
    warningIcon->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    acknowledmentLayout->addWidget(warningIcon, 0, 0);
    auto *label = new QLabel(this);
    label->setText(tr("Beta versions may leave unexpectedly or cause instabilities."));
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    acknowledmentLayout->addWidget(label, 0, 1);
    auto *checkbox = new QCheckBox(tr("I understand"), this);
    acknowledmentLayout->addWidget(checkbox, 1, 1);


    auto *bottomTextBox = new QLabel(this);
    bottomTextBox->setText(tr("Are you sure you want to leave the beta program?"));
    bottomTextBox->setObjectName("textLabel");
    bottomTextBox->setWordWrap(true);
    layout->addWidget(bottomTextBox);

    layout->addStretch();

    auto *buttonLayout = new QHBoxLayout(this);
    layout->addItem(buttonLayout);
    buttonLayout->setSpacing(boxHSpacing);
    auto *saveButton = new QPushButton(this);
    saveButton->setObjectName("defaultbutton");
    saveButton->setFlat(true);
    saveButton->setText(tr("Save"));
    saveButton->setEnabled(false);
    buttonLayout->addWidget(saveButton);
    auto *cancelButton = new QPushButton(this);
    cancelButton->setObjectName("nondefaultbutton");
    cancelButton->setFlat(true);
    cancelButton->setText(tr("Cancel"));
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addStretch();

    connect(saveButton, &QPushButton::clicked, this, &BetaProgramDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &BetaProgramDialog::reject);
}

} // namespace KDC
