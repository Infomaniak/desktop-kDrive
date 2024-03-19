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

#include <QBoxLayout>
#include <QDialog>
#include <QLabel>
#include <QMessageBox>
#include <QSize>

namespace KDC {

class CustomMessageBox : public CustomDialog {
        Q_OBJECT

        Q_PROPERTY(QSize icon_size READ iconSize WRITE setIconSize)

    public:
        CustomMessageBox(QMessageBox::Icon icon, const QString &text,
                         QMessageBox::StandardButtons buttons = QMessageBox::NoButton, QWidget *parent = nullptr);

        CustomMessageBox(QMessageBox::Icon icon, const QString &text, const QString &warningText, bool warning,
                         QMessageBox::StandardButtons buttons = QMessageBox::NoButton, QWidget *parent = nullptr);

        void addButton(const QString &text, QMessageBox::StandardButton button);
        void setDefaultButton(QMessageBox::StandardButton buttonType);

    private:
        QMessageBox::Icon _icon;
        QLabel *_warningLabel;
        QLabel *_textLabel;
        QLabel *_iconLabel;
        QHBoxLayout *_buttonsHBox;
        QColor _backgroundColor;
        QSize _iconSize;
        int _buttonCount;

        inline QSize iconSize() const { return _iconSize; }
        inline void setIconSize(const QSize &size) {
            _iconSize = size;
            setIcon();
        }

        void setIcon();
        QSize sizeHint() const override;

    private slots:
        void showEvent(QShowEvent *event) override;

        void onButtonClicked(bool checked = false);
        void onExit();
};

}  // namespace KDC
