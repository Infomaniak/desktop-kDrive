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

#include "customtoolbutton.h"
#include "gui/customradiobutton.h"
#include "info/errorinfo.h"

class QStackedWidget;
class QListWidget;

namespace KDC {

class FixConflictingFilesDialog : public CustomDialog {
        Q_OBJECT

    public:
        FixConflictingFilesDialog(int driveDbId, QWidget *parent = nullptr);

        inline bool keepLocalVersion() const { return _keepLocalVersion; }

    private slots:
        void onLinkActivated(const QString &link);
        void onExpandButtonClicked();
        void onScrollBarValueChanged();
        void onValidate();
        void onKeepRemoteButtonToggled(bool checked);

    private:
        void initUi();
        QString descriptionText() const;

        void insertFileItems(const int nbItems);

        int _driveDbId = 0;
        QList<ErrorInfo> _conflictList;

        bool _keepLocalVersion = false;

        QStackedWidget *_stackedWidget = nullptr;
        CustomRadioButton *_keepLocalButton = nullptr;
        CustomRadioButton *_keepRemoteButton = nullptr;
        QWidget *_keepRemoteDisclaimerWidget = nullptr;
        CustomToolButton *_expandButton = nullptr;
        QListWidget *_fileListWidget = nullptr;
};

} // namespace KDC
