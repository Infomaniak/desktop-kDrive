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

#include "menuwidget.h"
#include "guiutility.h"

#include <QAction>
#include <QGraphicsDropShadowEffect>
#include <QLoggingCategory>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QPainterPath>

namespace KDC {

const std::string MenuWidget::actionTypeProperty = "actionType";

static const int contentMargin = 10;
static const int cornerRadius = 5;
static const int shadowBlurRadius = 10;
static const int menuOffsetX = -30;
static const int menuOffsetY = 10;

Q_LOGGING_CATEGORY(lcMenuWidget, "gui.menuwidget", QtInfoMsg)

MenuWidget::MenuWidget(Type type, QWidget *parent) :
    QMenu(parent),
    _type(type),
    _backgroundColor(QColor()),
    _moved(false) {
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint | Qt::X11BypassWindowManagerHint);
    setAttribute(Qt::WA_TranslucentBackground);

    setContentsMargins(contentMargin, contentMargin, contentMargin, contentMargin);

    // Shadow
    QGraphicsDropShadowEffect *effect = new QGraphicsDropShadowEffect(this);
    effect->setBlurRadius(shadowBlurRadius);
    effect->setOffset(0);
    setGraphicsEffect(effect);

    connect(this, &MenuWidget::aboutToShow, this, &MenuWidget::onAboutToShow);
}

MenuWidget::MenuWidget(Type type, const QString &title, QWidget *parent) :
    MenuWidget(type, parent) {
    setTitle(title);
}

void MenuWidget::paintEvent(QPaintEvent *event) {
    if (!_moved) {
        // Move menu
        _moved = true;
        QPoint offset;
        switch (_type) {
            case Menu:
                offset = QPoint(menuOffsetX, menuOffsetY);
                break;
            case Submenu:
                offset = QPoint(pos().x() < parentWidget()->pos().x() ? contentMargin - 1 // Sub menu is on left
                                                                      : -contentMargin // Sub menu is on right
                                ,
                                0);
                break;
            case List:
#if defined(KD_WINDOWS)
                offset = QPoint(-10, -10);
#else
                offset = QPoint(-10, -2);
#endif
                break;
        }

        move(pos() + offset);
    }

    // Update shadow color
    QGraphicsDropShadowEffect *effect = qobject_cast<QGraphicsDropShadowEffect *>(graphicsEffect());
    if (effect) {
        effect->setColor(KDC::GuiUtility::getShadowColor(true));
    }

    QRect intRect = rect().marginsRemoved(QMargins(contentMargin, contentMargin, contentMargin - 1, contentMargin - 1));

    QPainterPath painterPath;
    painterPath.addRoundedRect(intRect, cornerRadius, cornerRadius);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(backgroundColor());
    painter.setPen(Qt::NoPen);
    painter.drawPath(painterPath);

    QMenu::paintEvent(event);
}

void MenuWidget::onAboutToShow() {
    _moved = false;
}

} // namespace KDC
