/*
Infomaniak Drive
Copyright (C) 2020 christophe.larchier@infomaniak.com

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "customradiobutton.h"

#include <QApplication>

namespace KDC {

static int defaultToolTipDuration = 3000;  // ms

CustomRadioButton::CustomRadioButton(QWidget *parent)
    : QRadioButton(parent), _toolTipText(QString()), _toolTipDuration(defaultToolTipDuration), _customToolTip(nullptr) {
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

}  // namespace KDC
