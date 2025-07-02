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

#include "taglabel.h"

#include "guiutility.h"

#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>

namespace KDC {

static constexpr int hMargin = 4;
#if defined(KD_MACOS)
static constexpr int offset = 0;
#else
static constexpr int offset = 2;
#endif

TagLabel::TagLabel(const QColor &color /*= Qt::transparent*/, QWidget *parent /*= nullptr*/) :
    QLabel(parent),
    _backgroundColor(color) {
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    setAlignment(Qt::AlignCenter);
}


QSize TagLabel::sizeHint() const {
    const QFontMetrics fm(_customFont);
    const auto textSize = fm.size(Qt::TextSingleLine, text());
    return {textSize.width() + 2 * hMargin + offset, textSize.height() + offset};
}

void TagLabel::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)

    // Update shadow color
    if (auto *effect = qobject_cast<QGraphicsDropShadowEffect *>(graphicsEffect())) {
        effect->setColor(GuiUtility::getShadowColor());
    }

    // Draw round rectangle
    QPainterPath painterPath;
    painterPath.addRoundedRect(rect(), rect().height() / 2, rect().height() / 2);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(backgroundColor());
    painter.drawPath(painterPath);

    // Draw text
    painter.setPen(QColor(255, 255, 255));
    QTextOption textOption;
    textOption.setAlignment(Qt::AlignCenter);
    painter.setFont(_customFont);
    QRect tmp = rect();
    tmp.translate(0, offset);
    painter.drawText(tmp, text(), textOption);
}

} // namespace KDC
