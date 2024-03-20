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

#include "customtooltip.h"
#include "widgetwithcustomtooltip.h"

namespace KDC {

WidgetWithCustomToolTip::WidgetWithCustomToolTip(QWidget *parent) : QWidget(parent), _customToolTip{nullptr} {}

// Place the tooltip at the bottom middle of the widget.
QPoint WidgetWithCustomToolTip::customToolTipPosition(QHelpEvent *event) {
    Q_UNUSED(event);
    const QRect widgetRect = geometry();

    return parentWidget()->mapToGlobal((widgetRect.bottomLeft() + widgetRect.bottomRight()) / 2.0);
}

bool WidgetWithCustomToolTip::event(QEvent *event) {
    static const int defaultToolTipDuration = 3000;  // ms

    if (event->type() == QEvent::ToolTip) {
        if (!_customToolTipText.isEmpty()) {
            const QPoint position = customToolTipPosition(static_cast<QHelpEvent *>(event));
            delete _customToolTip;
            _customToolTip = new CustomToolTip(_customToolTipText, position, defaultToolTipDuration, this);
            _customToolTip->show();
            event->ignore();

            return true;
        }
    }

    return QWidget::event(event);
}

void WidgetWithCustomToolTip::leaveEvent(QEvent *event) {
    delete _customToolTip;
    _customToolTip = nullptr;

    QWidget::leaveEvent(event);
}

LargeWidgetWithCustomToolTip::LargeWidgetWithCustomToolTip(QWidget *parent) : WidgetWithCustomToolTip(parent) {}

// Place the tooltip at mouse pointer position.
QPoint LargeWidgetWithCustomToolTip::customToolTipPosition(QHelpEvent *event) {
    return mapToGlobal(event->pos());
}

}  // namespace KDC
