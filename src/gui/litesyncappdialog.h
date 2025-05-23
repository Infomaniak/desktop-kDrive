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
#include "customcombobox.h"
#include "gui/clientgui.h"

#include <QLineEdit>
#include <QPushButton>

namespace KDC {

class LiteSyncAppDialog : public CustomDialog {
        Q_OBJECT

    public:
        explicit LiteSyncAppDialog(std::shared_ptr<ClientGui> gui, QWidget *parent = nullptr);

        void appInfo(QString &appId, QString &appName);

    private:
        std::shared_ptr<ClientGui> _gui;

        CustomComboBox *_appIdComboBox;
        QLineEdit *_appIdLineEdit;
        QLineEdit *_appNameLineEdit;
        QPushButton *_validateButton;
        QHash<QString, QString> _appTable;

    private slots:
        void onExit();
        void onComboBoxActivated(int index);
        void onTextEdited(const QString &text);
        void onValidateButtonTriggered(bool checked = false);
};

} // namespace KDC
