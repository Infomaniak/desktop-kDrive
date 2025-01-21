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

#include "customtabbar.h"

#include <QStylePainter>
#include <QPainterPath>
#include <QStyleOptionTab>
#include <QStyleOption>
#include <QStaticText>

namespace KDC {

static QColor selectLineColor(0, 152, 255);
static int selectedLineStroke = 4;
static QColor notifColor(233, 30, 99);

static int textLineHeightOffset = 15;
static int notifBoxXMargin = 10;
static int notifBoxHeight = 20;
static float notifBoxRadius = 9.5;
static int maxNbNotif = 99;
static int notifTextPadding = 15;

CustomTabBar::CustomTabBar(QWidget *parent) : QTabBar(parent), _unResolvedNotifCount(0), _autoResolvedNotifCount(0) {}

void CustomTabBar::paintEvent(QPaintEvent *) {
    QStylePainter p(this);

    for (int i = 0; i < count(); ++i) {
        int notifCount = i == 0 ? _unResolvedNotifCount : _autoResolvedNotifCount;

        QStyleOptionTab option;
        initStyleOption(&option, i);

        bool curr = i == currentIndex();
        bool notifDisplay = notifCount > 0;
        p.setPen(curr ? _tabSelectedTextColor : _tabTextColor);

        QRect textBounding = option.fontMetrics.boundingRect(option.text);

        QRect textRect(option.rect);
        textRect.moveCenter(QPoint(textRect.center().x(), textRect.center().y() - textLineHeightOffset));
        p.drawItemText(textRect, Qt::AlignBottom | Qt::AlignLeft, palette(), true, option.text);

        QString notifText = notifCount > maxNbNotif ? "+99" : QString::number(notifCount);
        QRect notifTextBoundingBox = option.fontMetrics.boundingRect(notifText);

        int textEndPointX = textBounding.bottomRight().x() + textRect.bottomLeft().x();

        int notifBoxWidth = notifTextBoundingBox.width() + notifTextPadding;

        // draw notif badge
        if (notifDisplay) {
            p.setPen(Qt::PenStyle::NoPen);

            p.setRenderHint(QPainter::Antialiasing);
            QPainterPath path;
            QRectF rect(textEndPointX + notifBoxXMargin, 5, notifBoxWidth, notifBoxHeight);
            path.addRoundedRect(rect, notifBoxRadius, notifBoxRadius);
            p.setPen(QColorConstants::Transparent);
            p.fillPath(path, notifColor);
            p.drawPath(path);

            p.setRenderHint(QPainter::TextAntialiasing);
            p.setPen(QColorConstants::White);

            p.drawText(rect, notifText, Qt::AlignVCenter | Qt::AlignHCenter);
        }

        // draw blue underline
        if (curr) {
            int margin = notifDisplay ? notifBoxWidth + notifBoxXMargin : 0;

            p.setPen(QPen(selectLineColor, selectedLineStroke));
            p.drawLine(QLineF(QPoint(textRect.bottomLeft().x(), textRect.bottomLeft().y() + textLineHeightOffset),
                              QPoint(textEndPointX + margin, textRect.bottomRight().y() + textLineHeightOffset)));
        }
    }
}


} // namespace KDC
