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

#include "customtogglepushbutton.h"
#include "guiutility.h"

namespace KDC {

CustomTogglePushButton::CustomTogglePushButton(QWidget *parent)
    : QPushButton(parent), _iconPath(QString()), _iconColor(QColor()), _iconColorChecked(QColor()) {
    setFlat(true);
    setCheckable(true);

    connect(this, &CustomTogglePushButton::iconColorChanged, this, &CustomTogglePushButton::onIconColorChanged);
    connect(this, &CustomTogglePushButton::iconColorCheckedChanged, this, &CustomTogglePushButton::onIconColorCheckedChanged);
    connect(this, &CustomTogglePushButton::toggled, this, &CustomTogglePushButton::onToggle);
}

CustomTogglePushButton::CustomTogglePushButton(const QString &text, QWidget *parent) : CustomTogglePushButton(parent) {
    setText(text);
}

bool CustomTogglePushButton::event(QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick ||
        event->type() == QEvent::KeyPress) {
        if (isChecked()) {
            event->ignore();
            return true;
        }
    }
    return QPushButton::event(event);
}

void CustomTogglePushButton::onIconColorChanged() {
    onToggle(isChecked());
}

void CustomTogglePushButton::onIconColorCheckedChanged() {
    onToggle(isChecked());
}

void CustomTogglePushButton::onToggle(bool checked) {
    if (!_iconPath.isEmpty()) {
        setIcon(KDC::GuiUtility::getIconWithColor(_iconPath, checked ? _iconColorChecked : _iconColor));
    }
}

}  // namespace KDC
