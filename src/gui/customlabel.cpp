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

#include "customlabel.h"

namespace KDC {

static int defaultToolTipDuration = 3000; // ms

CustomLabel::CustomLabel(QWidget *parent) :
    QLabel(parent), _toolTipText(QString()), _toolTipDuration(defaultToolTipDuration), _customToolTip(nullptr) {}


bool CustomLabel::event(QEvent *event) {
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

    return QLabel::event(event);
}

void CustomLabel::leaveEvent(QEvent *event) {
    if (_customToolTip) {
        _customToolTip->close();
        _customToolTip = nullptr;
    }

    QLabel::leaveEvent(event);
}

} // namespace KDC
