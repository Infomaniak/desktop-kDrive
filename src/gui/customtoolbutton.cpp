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

#include "customtoolbutton.h"
#include "guiutility.h"

#include <QApplication>
#include <QEvent>

namespace KDC {

static int defaultToolTipDuration = 3000; // ms

CustomToolButton::CustomToolButton(QWidget *parent) :
    QToolButton(parent), _withMenu(false), _baseIconSize(QSize()), _iconPath(QString()), _iconColor(QColor()),
    _iconColorHover(QColor()), _toolTipText(QString()), _toolTipDuration(defaultToolTipDuration), _hover(false),
    _customToolTip(nullptr) {
    connect(this, &CustomToolButton::baseIconSizeChanged, this, &CustomToolButton::onBaseIconSizeChanged);
    connect(this, &CustomToolButton::iconColorChanged, this, &CustomToolButton::onIconColorChanged);
    connect(this, &CustomToolButton::clicked, this, &CustomToolButton::onClicked);
}

void CustomToolButton::setWithMenu(bool withMenu) {
    _withMenu = withMenu;
    applyIconSizeAndColor();
}


void CustomToolButton::onBaseIconSizeChanged() {
    applyIconSizeAndColor();
}

void CustomToolButton::onIconColorChanged() {
    applyIconSizeAndColor();
}

void CustomToolButton::onClicked(bool checked) {
    Q_UNUSED(checked)

    // Remove hover
    QApplication::sendEvent(this, new QEvent(QEvent::Leave));
}

bool CustomToolButton::event(QEvent *event) {
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

    return QToolButton::event(event);
}

void CustomToolButton::enterEvent(QEnterEvent *event) {
    setHover(true);

    QToolButton::enterEvent(event);
}

void CustomToolButton::leaveEvent(QEvent *event) {
    setHover(false);

    if (_customToolTip) {
        _customToolTip->close();
        _customToolTip = nullptr;
    }

    QToolButton::leaveEvent(event);
}

void CustomToolButton::applyIconSizeAndColor() {
    QColor color = _hover ? _iconColorHover : _iconColor;

    if (_baseIconSize != QSize()) {
        setIconSize(QSize(_withMenu ? 2 * _baseIconSize.width() : _baseIconSize.width(), _baseIconSize.height()));
    }

    if (!_iconPath.isEmpty() && color.isValid()) {
        if (_withMenu) {
            setIcon(KDC::GuiUtility::getIconMenuWithColor(_iconPath, color));
        } else {
            setIcon(KDC::GuiUtility::getIconWithColor(_iconPath, color));
        }
    }
}

void CustomToolButton::setHover(bool hover) {
    if (isEnabled()) {
        _hover = hover;
        applyIconSizeAndColor();
    }
}

} // namespace KDC
