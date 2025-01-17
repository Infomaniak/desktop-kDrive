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

#pragma once

#include "customdialog.h"
#include "adddriveloginwidget.h"

#include <QMap>
#include <QSet>

namespace KDC {

class LoginDialog : public CustomDialog {
        Q_OBJECT

    public:
        LoginDialog(int userDbId, QWidget *parent = nullptr);

    private:
        int _userDbId;

        AddDriveLoginWidget *_loginWidget;

        void initUI();

    private slots:
        void onTerminated(bool next = true);
        void onExit();
};

} // namespace KDC
