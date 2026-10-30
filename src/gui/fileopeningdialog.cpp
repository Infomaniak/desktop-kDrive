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

#include "fileopeningdialog.h"

#include <cassert>

namespace KDC {

FileOpeningDialog::FileOpeningDialog(QWidget *parent) :
    CustomDialog(true, parent) {
    _messageLabel = new QLabel(this);
    _messageLabel->setWordWrap(true);

    _progressBar = new QProgressBar(this);
    _progressBar->setRange(0, 100);
    _progressBar->setValue(0);

    _cancelButton = new QPushButton(tr("Cancel"), this);

    auto *layout = mainLayout();
    layout->addWidget(_messageLabel);
    layout->addWidget(_progressBar);
    layout->addWidget(_cancelButton, 0, Qt::AlignRight);

    (void) connect(_cancelButton, &QPushButton::clicked, this, &FileOpeningDialog::canceled);

    updateStatus(FileOpeningPhase::WaitingSync, -1, QString());
}

void FileOpeningDialog::updateStatus(const FileOpeningPhase phase, const int8_t percent, const QString &itemName) {
    switch (phase) {
        case FileOpeningPhase::WaitingSync:
            _messageLabel->setText(tr("This item is not synchronized yet. Synchronizing it may take some time."));
            _progressBar->hide();
            break;
        case FileOpeningPhase::Hydrating:
            // The name of the item is always known once the download has started.
            _messageLabel->setText(itemName.isEmpty() ? tr("Downloading the item...")
                                                      : tr("Downloading <b>%1</b>...").arg(itemName.toHtmlEscaped()));
            _progressBar->setValue(percent);
            _progressBar->show();
            break;
        case FileOpeningPhase::Finished:
            // Handled by the caller: the window is closed when the file opening request has been processed.
            return;
        case FileOpeningPhase::EnumEnd: {
            assert(false && "Invalid enum value in switch statement.");
        }
    }

    // The window is non-modal: show or raise it every time the server reports a new state.
    show();
    raise();
    activateWindow();
}

} // namespace KDC
