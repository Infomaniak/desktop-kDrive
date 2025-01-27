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

#include "menuitemwidget.h"
#include "guiutility.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QStyleOption>

namespace KDC {

static const int boxHMargin = 15;
static const int boxVMargin = 10;
static const int boxSpacing = 10;

MenuItemWidget::MenuItemWidget(const QString &text, QWidget *parent, QMargins margins, bool hideIconLabels /*= false*/) :
    QWidget(parent), _leftIconPath(QString()), _leftIconColor(QColor()), _leftIconSize(QSize()), _rightIconPath(QString()),
    _rightIconColor(QColor()), _rightIconSize(QSize()), _defaultIconColor(QColor()), _checkIconColor(QColor()),
    _defaultIconSize(QSize()), _submenuIconSize(QSize()), _leftIconLabel(nullptr), _rightIconLabel(nullptr), _checked(false),
    _hasSubmenu(false) {
    if (margins.isNull()) {
        margins = QMargins(boxHMargin, boxVMargin, boxHMargin, boxVMargin);
    }
    QHBoxLayout *layout = new QHBoxLayout();
    layout->setContentsMargins(margins);
    layout->setSpacing(boxSpacing);

    _leftIconLabel = new QLabel(this);
    if (!hideIconLabels) {
        layout->addWidget(_leftIconLabel);
    }

    QLabel *menuItemLabel = new QLabel(this);
    menuItemLabel->setObjectName("menuItemLabel");
    menuItemLabel->setText(text);
    layout->addWidget(menuItemLabel);

    layout->addStretch();

    _rightIconLabel = new QLabel(this);
    if (!hideIconLabels) {
        layout->addWidget(_rightIconLabel);
    }

    setLayout(layout);

    connect(this, &MenuItemWidget::defaultIconColorChanged, this, &MenuItemWidget::onDefaultIconColorChanged);
    connect(this, &MenuItemWidget::checkIconColorChanged, this, &MenuItemWidget::onCheckIconColorChanged);
    connect(this, &MenuItemWidget::defaultIconSizeChanged, this, &MenuItemWidget::onDefaultIconSizeChanged);
    connect(this, &MenuItemWidget::submenuIconSizeChanged, this, &MenuItemWidget::onSubmenuIconSizeChanged);
}

void MenuItemWidget::setLeftIcon(const QString &path, const QColor &color, const QSize &size) {
    _leftIconPath = path;
    _leftIconColor = color;
    _leftIconSize = size;

    if (color != QColor() && size != QSize()) {
        _leftIconLabel->setPixmap(KDC::GuiUtility::getIconWithColor(path, color).pixmap(size));
    }
}

void MenuItemWidget::setLeftIcon(const QIcon &icon, const QSize &size) {
    if (size != QSize()) {
        _leftIconLabel->setPixmap(icon.pixmap(size));
    }
}

void MenuItemWidget::setRightIcon(const QString &path, const QColor &color, const QSize &size) {
    _rightIconPath = path;
    _rightIconColor = color;
    _rightIconSize = size;

    if (color != QColor() && size != QSize()) {
        _rightIconLabel->setPixmap(KDC::GuiUtility::getIconWithColor(path, color).pixmap(size));
    }
}

void MenuItemWidget::setRightIcon(const QIcon &icon, const QSize &size) {
    if (size != QSize()) {
        _rightIconLabel->setPixmap(icon.pixmap(size));
    }
}

void MenuItemWidget::paintEvent(QPaintEvent *paintEvent) {
    Q_UNUSED(paintEvent)

    QStyleOption opt;
    opt.initFrom(this);
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);
}

void MenuItemWidget::setIcons() {
    if (_leftIconPath.isEmpty()) {
        if (_checked && _checkIconColor != QColor() && _defaultIconSize != QSize()) {
            _leftIconLabel->setPixmap(
                    KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/check.svg", _checkIconColor)
                            .pixmap(_defaultIconSize));
        }
    } else if (_leftIconColor != QColor() && _leftIconSize != QSize()) {
        _leftIconLabel->setPixmap(KDC::GuiUtility::getIconWithColor(_leftIconPath, _leftIconColor).pixmap(_leftIconSize));
    }

    if (_rightIconPath.isEmpty()) {
        if (_hasSubmenu && _defaultIconColor != QColor() && _submenuIconSize != QSize()) {
            _rightIconLabel->setPixmap(
                    KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/chevron-right.svg", _defaultIconColor)
                            .pixmap(_submenuIconSize));
        }
    } else if (_rightIconColor != QColor() && _rightIconSize != QSize()) {
        _rightIconLabel->setPixmap(KDC::GuiUtility::getIconWithColor(_rightIconPath, _rightIconColor).pixmap(_rightIconSize));
    }
}

void MenuItemWidget::onDefaultIconColorChanged() {
    if (_leftIconColor == QColor()) {
        _leftIconColor = _defaultIconColor;
    }

    if (_rightIconColor == QColor()) {
        _rightIconColor = _defaultIconColor;
    }

    setIcons();
}

void MenuItemWidget::onCheckIconColorChanged() {
    setIcons();
}

void MenuItemWidget::onDefaultIconSizeChanged() {
    if (_leftIconSize == QSize()) {
        _leftIconSize = _defaultIconSize;
    }

    if (_rightIconSize == QSize()) {
        _rightIconSize = _defaultIconSize;
    }

    _leftIconLabel->setFixedSize(_leftIconSize);

    setIcons();
}

void MenuItemWidget::onSubmenuIconSizeChanged() {
    setIcons();
}

} // namespace KDC
