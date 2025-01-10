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

#include "customproxystyle.h"

#include <QVariant>

namespace KDC {

static const int tooltipWakeUpDelay = 200; // ms

const char CustomProxyStyle::focusRectangleProperty[] = "focusRectangle";

int CustomProxyStyle::styleHint(QStyle::StyleHint hint, const QStyleOption *option, const QWidget *widget,
                                QStyleHintReturn *returnData) const {
    if (hint == QStyle::SH_ToolTip_WakeUpDelay) {
        return tooltipWakeUpDelay;
    }
    return QProxyStyle::styleHint(hint, option, widget, returnData);
}

void CustomProxyStyle::drawPrimitive(QStyle::PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                                     const QWidget *widget) const {
    // No focus rectangle
    if (element == QStyle::PE_FrameFocusRect && !widget->property(focusRectangleProperty).toBool()) {
        return;
    }

    QProxyStyle::drawPrimitive(element, option, painter, widget);
}

} // namespace KDC
