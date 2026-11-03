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

#include <QHBoxLayout>
#include <cassert>

namespace KDC {

static const int32_t boxHMargin = 40;
static const int32_t messageVBMargin = 15;
// The window only carries an informative message, a progress gauge and a cancel button: keep it more compact than
// the default minimum size of CustomDialog.
static const QSize windowMinimumSize(625, 280);

FileOpeningDialog::FileOpeningDialog(QWidget *parent) :
    CustomDialog(true, parent) {
    setMinimumSize(windowMinimumSize);
    resize(windowMinimumSize);

    mainLayout()->addStretch();

    // Message
    auto *messageHBox = new QHBoxLayout();
    messageHBox->setContentsMargins(boxHMargin, 0, boxHMargin, messageVBMargin);
    mainLayout()->addLayout(messageHBox);

    _messageLabel = new QLabel(this);
    _messageLabel->setObjectName("titleLabel");
    _messageLabel->setTextFormat(Qt::RichText);
    _messageLabel->setWordWrap(true);
    _messageLabel->setAlignment(Qt::AlignHCenter);
    messageHBox->addWidget(_messageLabel);

    // Progress bar
    auto *progressHBox = new QHBoxLayout();
    progressHBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    mainLayout()->addLayout(progressHBox);

    _progressBar = new QProgressBar(this);
    _progressBar->setRange(0, 100);
    _progressBar->setValue(0);
    progressHBox->addWidget(_progressBar);

    mainLayout()->addStretch();

    // Cancel button
    auto *buttonsHBox = new QHBoxLayout();
    buttonsHBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    mainLayout()->addLayout(buttonsHBox);

    _cancelButton = new QPushButton(this);
    _cancelButton->setObjectName("nondefaultbutton");
    _cancelButton->setFlat(true);
    _cancelButton->setText(tr("Cancel"));
    buttonsHBox->addWidget(_cancelButton);
    buttonsHBox->addStretch();

    (void) connect(_cancelButton, &QPushButton::clicked, this, &FileOpeningDialog::canceled);

    // Closing the window (system bar cross, escape key) dismisses it without cancelling the file opening.
    (void) connect(this, &CustomDialog::exit, this, &FileOpeningDialog::onExit);
    (void) connect(this, &QDialog::rejected, this, &FileOpeningDialog::onExit);

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

void FileOpeningDialog::onExit() {
    // Dismiss the window: the file opening keeps running, and the window is displayed again the next time the server
    // reports the state of the request.
    hide();
}

} // namespace KDC
