/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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
#include "libcommon/utility/cstypes.h"

#include <QLabel>
#include <QProgressBar>
#include <QPushButton>

namespace KDC {

//! Pop-up window informing the user of the progress of a file opening requested through a "kdrive://open/..."
//! deeplink. While the item is being synchronized, the window explains that it may take some time; while it is being
//! hydrated, it displays the progress of the download. In both cases, the user can cancel the request.
class FileOpeningDialog : public CustomDialog {
        Q_OBJECT

    public:
        explicit FileOpeningDialog(QWidget *parent = nullptr);

        //! Updates the window contents according to the state reported by the server: an informative message while the
        //! item is being synchronized, and the name of the item being downloaded along with a progress gauge while it
        //! is being downloaded.
        void updateStatus(FileOpeningPhase phase, int8_t percent, const QString &itemName);

    signals:
        //! Emitted when the user requests the cancellation of the file opening.
        void canceled();

    private:
        QLabel *_messageLabel = nullptr;
        QProgressBar *_progressBar = nullptr;
        QPushButton *_cancelButton = nullptr;
};

} // namespace KDC
