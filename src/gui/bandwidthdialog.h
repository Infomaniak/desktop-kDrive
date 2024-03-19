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

#pragma once

#include "customdialog.h"
#include "customradiobutton.h"

#include <QLineEdit>
#include <QPushButton>

namespace KDC {

class BandwidthDialog : public CustomDialog {
        Q_OBJECT

    public:
        explicit BandwidthDialog(QWidget *parent = nullptr);

    private:
        enum LimitType { NoLimit = 0, Limit };

        int _useDownloadLimit;
        int _downloadLimit;
        int _useUploadLimit;
        int _uploadLimit;
        CustomRadioButton *_downloadNoLimitButton;
        CustomRadioButton *_downloadValueLimitButton;
        QLineEdit *_downloadValueLimitLineEdit;
        CustomRadioButton *_uploadNoLimitButton;
        CustomRadioButton *_uploadValueLimitButton;
        QLineEdit *_uploadValueLimitLineEdit;
        QPushButton *_saveButton;
        bool _needToSave;

        void initUI();
        void updateUI();
        void setNeedToSave(bool value);

    private slots:
        void onExit();
        void onSaveButtonTriggered(bool checked = false);
        void onDownloadNoLimitButtonToggled(bool checked);
        void onDownloadValueLimitButtonToggled(bool checked);
        void onDownloadValueLimitTextEdited(const QString &text);
        void onUploadNoLimitButtonToggled(bool checked);
        void onUploadValueLimitButtonToggled(bool checked);
        void onUploadValueLimitTextEdited(const QString &text);
};

}  // namespace KDC
