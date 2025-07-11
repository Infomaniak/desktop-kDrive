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

#include "menuwidgetlite.h"

#include <QPushButton>

#include <iostream>

namespace KDC {

MenuWidgetLite::MenuWidgetLite(QPushButton *button, QWidget *parent) :
    QMenu(parent),
    b(button) {}

void MenuWidgetLite::showEvent(QShowEvent *) {
#ifndef KD_WINDOWS
    QPoint bp = b->mapToGlobal(b->pos());
    QPoint p(pos().x(), bp.y() + b->height() / 2); // I do not understand why b->pos().y() seems to return the vertical center
                                                   // instead of the y position of the top left corner...
    move(p);
#endif
}

} // namespace KDC
