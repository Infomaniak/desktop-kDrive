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

#include "customcombobox.h"
#include "customdialog.h"

#include "utility/types.h"


class QLabel;
class QCheckBox;

namespace KDC {

class CustomComboBox;

class BetaProgramDialog final : public CustomDialog {
        Q_OBJECT

    public:
        explicit BetaProgramDialog(bool isQuit, bool isStaff, QWidget *parent = nullptr);

        [[nodiscard]] DistributionChannel selectedDistributionChannel() const { return _newChannel; }

    private slots:
        void onAcknowledgement();
        void onSave();
        void onChannelChange(int index);

    private:
        void setTooRecentMessage() const;
        void setInstabilityMessage() const;

        bool _isQuit{false};
        bool _isStaff{false};
        DistributionChannel _newChannel{DistributionChannel::Unknown};
        QCheckBox *_acknowledgmentCheckbox{nullptr};
        QFrame *_acknowlegmentFrame{nullptr};
        QLabel *_acknowledmentLabel{nullptr};
        CustomComboBox *_staffSelectionBox{nullptr};
        QPushButton *_saveButton{nullptr};
        int _initialIndex{0};
};

} // namespace KDC
