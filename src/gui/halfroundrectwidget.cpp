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

#include "halfroundrectwidget.h"
#include "guiutility.h"

#include <QBrush>
#include <QLinearGradient>
#include <QLoggingCategory>
#include <QPainter>
#include <QPainterPath>
#include <QPoint>
#include <QRect>
#include <QRegion>

namespace KDC {

static const int cornerRadius = 20;
static const int shadowWidth = 5;

Q_LOGGING_CATEGORY(lcHalfRoundRectWidget, "gui.halfroundwidget", QtInfoMsg)

HalfRoundRectWidget::HalfRoundRectWidget(QWidget *parent) : QWidget(parent), _bottomCornersColor(QColor()), _hboxLayout(nullptr) {
    _hboxLayout = new QHBoxLayout();
    setLayout(_hboxLayout);
}

void HalfRoundRectWidget::setContentsMargins(int left, int top, int right, int bottom) {
    _hboxLayout->setContentsMargins(left, top, right, bottom);
}

void HalfRoundRectWidget::setSpacing(int spacing) {
    _hboxLayout->setSpacing(spacing);
}

void HalfRoundRectWidget::addWidget(QWidget *widget, int stretch, Qt::Alignment alignment) {
    _hboxLayout->addWidget(widget, stretch, alignment);
}

void HalfRoundRectWidget::addLayout(QLayout *layout, int stretch) {
    _hboxLayout->addLayout(layout, stretch);
}

void HalfRoundRectWidget::addStretch(int stretch) {
    _hboxLayout->addStretch(stretch);
}

void HalfRoundRectWidget::addSpacing(int size) {
    _hboxLayout->addSpacing(size);
}

bool HalfRoundRectWidget::setStretchFactor(QWidget *widget, int stretch) {
    return _hboxLayout->setStretchFactor(widget, stretch);
}

bool HalfRoundRectWidget::setStretchFactor(QLayout *layout, int stretch) {
    return _hboxLayout->setStretchFactor(layout, stretch);
}

void HalfRoundRectWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    int diameter = 2 * cornerRadius;
    QRect rectangleLeft = QRect(0, rect().height() - diameter - shadowWidth, diameter, diameter);
    QRect rectangleRight = QRect(rect().width() - diameter, rect().height() - diameter - shadowWidth, diameter, diameter);
    QPoint bottomLeft(0, rect().height());
    QPoint bottomRight(rect().width(), rect().height());
    QPoint leftArcCenter(cornerRadius, rect().height() - cornerRadius - shadowWidth);
    QPoint leftArcStart(0, rect().height() - cornerRadius - shadowWidth);
    QPoint leftArcEnd(cornerRadius, rect().height() - shadowWidth);
    QPoint rightArcCenter(rect().width() - cornerRadius, rect().height() - cornerRadius - shadowWidth);
    QPoint rightArcStart(rect().width() - cornerRadius, rect().height() - shadowWidth);
    QPoint rightArcEnd(rect().width(), rect().height() - cornerRadius - shadowWidth);
    QPoint translation(0, shadowWidth);

    // Draw bottom
    QPainterPath painterPath(bottomLeft);
    painterPath.lineTo(leftArcStart);
    painterPath.arcTo(rectangleLeft, 180, 90);
    painterPath.lineTo(rightArcStart);
    painterPath.arcTo(rectangleRight, 270, 90);
    painterPath.lineTo(bottomRight);
    painterPath.closeSubpath();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(bottomCornersColor());
    painter.setPen(Qt::NoPen);
    painter.drawPath(painterPath);

    // Draw shadow - left arc
    QPainterPath painterPathLeftArc(leftArcStart);
    painterPathLeftArc.arcTo(rectangleLeft, 180, 90);
    painterPathLeftArc.lineTo(leftArcEnd + translation);
    rectangleLeft.translate(translation);
    painterPathLeftArc.arcTo(rectangleLeft, 270, -90);
    painterPathLeftArc.closeSubpath();

    QRadialGradient gradientLeftArc(leftArcCenter, cornerRadius + shadowWidth);
    gradientLeftArc.setColorAt(0, KDC::GuiUtility::getShadowColor());
    gradientLeftArc.setColorAt(1, bottomCornersColor());
    painter.setBrush(gradientLeftArc);
    painter.drawPath(painterPathLeftArc);

    // Draw shadow - bottom
    QPainterPath painterPathBottom(leftArcEnd + translation);
    painterPathBottom.lineTo(leftArcEnd);
    painterPathBottom.lineTo(rightArcStart);
    painterPathBottom.lineTo(rightArcStart + translation);
    painterPathBottom.closeSubpath();

    QLinearGradient gradientBottom(leftArcCenter, leftArcEnd + translation);
    gradientBottom.setColorAt(0, KDC::GuiUtility::getShadowColor());
    gradientBottom.setColorAt(1, bottomCornersColor());
    painter.setBrush(gradientBottom);
    painter.drawPath(painterPathBottom);

    // Draw shadow - right arc
    QPainterPath painterPathRightArc;
    painterPathRightArc.moveTo(rightArcStart);
    painterPathRightArc.arcTo(rectangleRight, 270, 90);
    painterPathLeftArc.lineTo(rightArcEnd + translation);
    rectangleRight.translate(translation);
    painterPathRightArc.arcTo(rectangleRight, 0, -90);
    painterPathRightArc.closeSubpath();

    QRadialGradient gradientRightArc(rightArcCenter, cornerRadius + shadowWidth);
    gradientRightArc.setColorAt(0, KDC::GuiUtility::getShadowColor());
    gradientRightArc.setColorAt(1, bottomCornersColor());
    painter.setBrush(gradientRightArc);
    painter.drawPath(painterPathRightArc);
}

}  // namespace KDC
