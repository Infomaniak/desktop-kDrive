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

#include "actionwidget.h"
#include "guiutility.h"

#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLoggingCategory>
#include <QPainterPath>
#include <QPainter>

namespace KDC {

static const int boxHMargin = 15;
static const int boxVMargin = 5;
static const int boxSpacing = 10;
static const int shadowBlurRadius = 20;

Q_LOGGING_CATEGORY(lcActionWidget, "gui.actionwidget", QtInfoMsg)

ActionWidget::ActionWidget(const QString &path, const QString &text, QWidget *parent)
    : ClickableWidget(parent),
      _leftIconPath(path),
      _text(text),
      _backgroundColor(QColor()),
      _leftIconColor(QColor()),
      _leftIconSize(QSize()),
      _actionIconColor(QColor()),
      _actionIconSize(QSize()),
      _leftIconLabel(nullptr),
      _actionIconLabel(nullptr),
      _leftTextLabel(nullptr) {
    setContentsMargins(0, 0, 0, 0);

    QHBoxLayout *hbox = new QHBoxLayout();
    hbox->setContentsMargins(boxHMargin, boxVMargin, boxHMargin, boxVMargin);
    hbox->setSpacing(boxSpacing);
    setLayout(hbox);

    _leftIconLabel = new QLabel(this);
    hbox->addWidget(_leftIconLabel);

    _leftTextLabel = new QLabel(_text, this);
    _leftTextLabel->setObjectName("textLabel");
    _leftTextLabel->setWordWrap(true);
    hbox->addWidget(_leftTextLabel);
    hbox->setStretchFactor(_leftTextLabel, 1);

    _actionIconLabel = new QLabel(this);
    hbox->addWidget(_actionIconLabel);

    // Shadow
    QGraphicsDropShadowEffect *effect = new QGraphicsDropShadowEffect(this);
    effect->setBlurRadius(shadowBlurRadius);
    effect->setOffset(0);
    setGraphicsEffect(effect);

    connect(this, &ActionWidget::leftIconSizeChanged, this, &ActionWidget::onLeftIconSizeChanged);
    connect(this, &ActionWidget::leftIconColorChanged, this, &ActionWidget::onLeftIconColorChanged);
    connect(this, &ActionWidget::actionIconColorChanged, this, &ActionWidget::onActionIconColorChanged);
    connect(this, &ActionWidget::actionIconSizeChanged, this, &ActionWidget::onActionIconSizeChanged);
}

ActionWidget::ActionWidget(const QString &path, QWidget *parent) : ActionWidget(path, "", parent) {}

void ActionWidget::setText(const QString &text) {
    _text = text;
    _leftTextLabel->setText(_text);
}

void ActionWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    // Update shadow color
    QGraphicsDropShadowEffect *effect = qobject_cast<QGraphicsDropShadowEffect *>(graphicsEffect());
    if (effect) {
        effect->setColor(KDC::GuiUtility::getShadowColor());
    }

    // Draw round rectangle
    QPainterPath painterPath;
    painterPath.addRoundedRect(rect(), rect().height() / 2, rect().height() / 2);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(backgroundColor());
    painter.drawPath(painterPath);
}

void ActionWidget::setLeftIcon() {
    if (_leftIconLabel && _leftIconSize != QSize() && _leftIconColor != QColor()) {
        _leftIconLabel->setPixmap(KDC::GuiUtility::getIconWithColor(_leftIconPath, _leftIconColor).pixmap(_leftIconSize));
    }
}

void ActionWidget::setActionIcon() {
    if (_actionIconLabel && _actionIconSize != QSize() && _actionIconColor != QColor()) {
        _actionIconLabel->setPixmap(
            KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/chevron-right.svg", _actionIconColor)
                .pixmap(_actionIconSize));
    }
}

void ActionWidget::onLeftIconSizeChanged() {
    setLeftIcon();
}

void ActionWidget::onLeftIconColorChanged() {
    setLeftIcon();
}

void ActionWidget::onActionIconColorChanged() {
    setActionIcon();
}

void ActionWidget::onActionIconSizeChanged() {
    setActionIcon();
}

}  // namespace KDC
