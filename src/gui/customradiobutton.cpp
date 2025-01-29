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

#include "customradiobutton.h"

#include <QApplication>

namespace KDC {

static int defaultToolTipDuration = 3000; // ms

CustomRadioButton::CustomRadioButton(QWidget *parent) :
    QRadioButton(parent), _toolTipText(QString()), _toolTipDuration(defaultToolTipDuration), _customToolTip(nullptr) {
    connect(this, &QRadioButton::clicked, this, &CustomRadioButton::onClicked);
}

bool CustomRadioButton::event(QEvent *event) {
    if (event->type() == QEvent::ToolTip) {
        if (!_toolTipText.isEmpty()) {
            if (!_customToolTip) {
                QRect widgetRect = geometry();
                QPoint position = parentWidget()->mapToGlobal((widgetRect.bottomLeft() + widgetRect.bottomRight()) / 2.0);
                _customToolTip = new CustomToolTip(_toolTipText, position, _toolTipDuration);
                _customToolTip->show();
                event->ignore();
                return true;
            }
        }
    }

    return QRadioButton::event(event);
}

void CustomRadioButton::leaveEvent(QEvent *event) {
    if (_customToolTip) {
        _customToolTip->close();
        _customToolTip = nullptr;
    }

    QRadioButton::leaveEvent(event);
}

void CustomRadioButton::onClicked(bool checked) {
    Q_UNUSED(checked)

    // Remove hover
    QApplication::sendEvent(this, new QEvent(QEvent::Leave));
}

} // namespace KDC
