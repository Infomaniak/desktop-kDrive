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

#include "disabledoverlay.h"

#include "guiutility.h"

#include <QPainter>

namespace KDC {

static const QColor disabledOverlayColorLight = {182, 182, 182, 128};
static const QColor disabledOverlayColorDark = {24, 24, 24, 128};
static const int cornerRadius = 5;
static const int hMargin = 20;
static const int vMargin = 20;

DisabledOverlay::DisabledOverlay(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
}

void DisabledOverlay::paintEvent(QPaintEvent *e) {
    Q_UNUSED(e)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(KDC::GuiUtility::isDarkTheme() ? disabledOverlayColorDark : disabledOverlayColorLight);
    painter.drawRoundedRect(rect().marginsRemoved(QMargins(hMargin, vMargin, hMargin, vMargin)), cornerRadius, cornerRadius);
}

}  // namespace KDC
