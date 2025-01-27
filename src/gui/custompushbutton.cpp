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

#include "custompushbutton.h"
#include "guiutility.h"

#include <QHBoxLayout>
#include <QSizePolicy>
#include <QWidgetAction>

namespace KDC {

static const int boxHMargin = 10;
static const int boxVMargin = 5;
static const int boxSpacing = 10;

CustomPushButton::CustomPushButton(const QString &path, const QString &text, QWidget *parent) :
    QPushButton(parent), _iconPath(path), _text(text), _iconSize(QSize()), _iconColor(QColor()), _iconLabel(nullptr),
    _textLabel(nullptr) {
    setContentsMargins(0, 0, 0, 0);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    QHBoxLayout *hbox = new QHBoxLayout();
    hbox->setContentsMargins(boxHMargin, boxVMargin, boxHMargin, boxVMargin);
    hbox->setSpacing(boxSpacing);
    setLayout(hbox);

    if (!_iconPath.isEmpty()) {
        _iconLabel = new QLabel(this);
        hbox->addWidget(_iconLabel);
    }

    _textLabel = new QLabel(_text, this);
    _textLabel->setObjectName("textLabel");
    hbox->addWidget(_textLabel);
    hbox->addStretch();

    connect(this, &CustomPushButton::iconSizeChanged, this, &CustomPushButton::onIconSizeChanged);
    connect(this, &CustomPushButton::iconColorChanged, this, &CustomPushButton::onIconColorChanged);
}

CustomPushButton::CustomPushButton(const QString &text, QWidget *parent) : CustomPushButton("", text, parent) {}

CustomPushButton::CustomPushButton(QWidget *parent) : CustomPushButton("", "", parent) {}

QSize CustomPushButton::sizeHint() const {
    if (_iconPath.isEmpty()) {
        return QSize(_textLabel->sizeHint().width() + 2 * boxHMargin, QPushButton::sizeHint().height());
    }

    return QSize(_iconLabel->sizeHint().width() + _textLabel->sizeHint().width() + boxSpacing + 2 * boxHMargin,
                 QPushButton::sizeHint().height());
}

void CustomPushButton::setText(const QString &text) {
    _text = text;
    _textLabel->setText(_text);
}

void CustomPushButton::onIconSizeChanged() {
    setIcon();
}

void CustomPushButton::onIconColorChanged() {
    setIcon();
}

void CustomPushButton::setIcon() {
    if (_iconLabel && _iconSize != QSize() && _iconColor != QColor()) {
        _iconLabel->setPixmap(KDC::GuiUtility::getIconWithColor(_iconPath, _iconColor).pixmap(_iconSize));
    }
}

} // namespace KDC
