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

#include "buttonsbarwidget.h"

#include <QBrush>
#include <QLoggingCategory>
#include <QPainter>
#include <QRect>

namespace KDC {

static const int hMargin = 30;
static const int vMargin = 20;

Q_LOGGING_CATEGORY(lcButtonsBarWidget, "gui.buttonsbarwidget", QtInfoMsg)

ButtonsBarWidget::ButtonsBarWidget(QWidget *parent) :
    QWidget(parent), _position(0), _backgroundColor(QColor()), _hboxLayout(nullptr) {
    _hboxLayout = new QHBoxLayout();
    _hboxLayout->setContentsMargins(hMargin, vMargin, hMargin, vMargin);
    setLayout(_hboxLayout);
}

void ButtonsBarWidget::insertButton(int position, CustomTogglePushButton *button) {
    if (button) {
        _hboxLayout->insertWidget(position, button);
        connect(button, &CustomTogglePushButton::toggled, this, &ButtonsBarWidget::onToggle);
        buttonsList.insert(position, button);
        if (buttonsList.size() == 1) {
            button->setChecked(true);
        }
    }
}

void ButtonsBarWidget::selectButton(int position) {
    int i = 0;
    for (auto btn: qAsConst(buttonsList)) {
        if (i == position) {
            btn->setChecked(true);
            emit buttonToggled(position);
            break;
        }
        i++;
    }
}

void ButtonsBarWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.fillRect(rect(), backgroundColor());
}

void ButtonsBarWidget::onToggle(bool checked) {
    if (checked) {
        int position = 0;
        for (auto btn: qAsConst(buttonsList)) {
            if (btn == sender()) {
                _position = position;
                emit buttonToggled(position);
            } else {
                btn->setChecked(false);
            }
            position++;
        }
    }
}

} // namespace KDC
