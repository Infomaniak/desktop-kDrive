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

#include "utility/types.h"

class QCheckBox;

namespace KDC {

class BetaProgramDialog final : public CustomDialog {
        Q_OBJECT

    public:
        explicit BetaProgramDialog(bool isQuit, QWidget *parent = nullptr);

        [[nodiscard]] DistributionChannel selectedDistributionChannel() const { return _channel; }

    private slots:
        void onAcknowledgement();
        void onSave();

    private:
        bool _isQuit{false};
        DistributionChannel _channel{DistributionChannel::Unknown};
        QCheckBox *_acknowledgmentCheckbox{nullptr};
        QPushButton *_saveButton{nullptr};
};

} // namespace KDC
