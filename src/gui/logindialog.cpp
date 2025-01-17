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

#include "logindialog.h"
#include "config.h"
#include "libcommon/utility/types.h"

#include <QLoggingCategory>

namespace KDC {

static const QSize windowSize(625, 800);
static const int boxHMargin = 40;
static const int boxVTMargin = 20;
static const int boxVBMargin = 40;

Q_LOGGING_CATEGORY(lcLoginDialog, "gui.logindialog", QtInfoMsg)

LoginDialog::LoginDialog(int userDbId, QWidget *parent) :
    CustomDialog(false, parent), _userDbId(userDbId), _loginWidget(nullptr) {
    initUI();
}

void LoginDialog::initUI() {
    setBackgroundForcedColor(Qt::white);
    setMinimumSize(windowSize);
    setMaximumSize(windowSize);

    QVBoxLayout *mainLayout = this->mainLayout();
    mainLayout->setContentsMargins(boxHMargin, boxVTMargin, boxHMargin, boxVBMargin);

    _loginWidget = new AddDriveLoginWidget(this);
    mainLayout->addWidget(_loginWidget);

    connect(_loginWidget, &AddDriveLoginWidget::terminated, this, &LoginDialog::onTerminated);
    connect(this, &CustomDialog::exit, this, &LoginDialog::onExit);

    _loginWidget->init();
}

void LoginDialog::onTerminated(bool next) {
    if (next) {
        accept();
    } else {
        reject();
    }
}

void LoginDialog::onExit() {
    reject();
}

} // namespace KDC
