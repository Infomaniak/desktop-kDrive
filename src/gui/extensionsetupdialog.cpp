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

#include "extensionsetupdialog.h"

#include <QBoxLayout>
#include <QLoggingCategory>
#include <QPushButton>

namespace KDC {

static const int boxHMargin = 40;
static const int boxHSpacing = 20;

Q_LOGGING_CATEGORY(lcExtensionSetupDialog, "gui.extensionsetupdialog", QtInfoMsg)

ExtensionSetupDialog::ExtensionSetupDialog(QWidget *parent) : CustomDialog(true, parent), _extensionSetupWidget(nullptr) {
    QVBoxLayout *mainLayout = this->mainLayout();
    mainLayout->setContentsMargins(boxHMargin, 0, boxHMargin, boxHSpacing);
    mainLayout->setSpacing(0);
    _extensionSetupWidget = new CustomExtensionSetupWidget(this, false);
    mainLayout->addWidget(_extensionSetupWidget);

    connect(_extensionSetupWidget, &CustomExtensionSetupWidget::finishedButtonTriggered, this,
            &ExtensionSetupDialog::onOkButtonTriggered);
    connect(this, &CustomDialog::exit, this, &ExtensionSetupDialog::onExit);
}

void ExtensionSetupDialog::onExit() {
    reject();
}

void ExtensionSetupDialog::onOkButtonTriggered() {
    accept();
}

} // namespace KDC
