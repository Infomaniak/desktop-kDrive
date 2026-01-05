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

#include "../customdialog.h"
#include "utility/types.h"

/**
 * This update dialog is only used on Windows.
 *
 * - MacOS: the update dialog is fully managed by Sparkle.
 * - Linux: there is no update dialog (this `UpdateDialog` could be used in the future).
 **/

namespace KDC {

class UpdateDialog : public CustomDialog {
        Q_OBJECT

    public:
        explicit UpdateDialog(const VersionInfo &versionInfo, QWidget *parent = nullptr);
        ~UpdateDialog() override;

        void paintEvent(QPaintEvent *event) override;
        void reject() override;
        void accept() override;

        [[nodiscard]] bool skip() const { return _skip; }

    private:
        void initUi(const VersionInfo &versionInfo);

        QPushButton *_skipButton{nullptr};
        bool _skip{false};
};

} // namespace KDC
