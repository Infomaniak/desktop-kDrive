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

#pragma once

#include <QColor>
#include <QEvent>
#include <QPushButton>
#include <QString>

namespace KDC {

class CustomTogglePushButton : public QPushButton {
        Q_OBJECT

        Q_PROPERTY(QColor icon_color READ iconColor WRITE setIconColor)
        Q_PROPERTY(QColor icon_color_checked READ iconColorChecked WRITE setIconColorChecked)

    public:
        explicit CustomTogglePushButton(QWidget *parent = nullptr);
        explicit CustomTogglePushButton(const QString &text, QWidget *parent = nullptr);

        inline void setIconPath(const QString &path) { _iconPath = path; }

    signals:
        void iconColorChanged();
        void iconColorCheckedChanged();

    private:
        QString _iconPath;
        QColor _iconColor;
        QColor _iconColorChecked;

        bool event(QEvent *event);

        inline QColor iconColor() const { return _iconColor; }
        inline void setIconColor(const QColor &color) {
            _iconColor = color;
            emit iconColorChanged();
        }

        inline QColor iconColorChecked() const { return _iconColorChecked; }
        inline void setIconColorChecked(const QColor &color) {
            _iconColorChecked = color;
            emit iconColorCheckedChanged();
        }

    private slots:
        void onIconColorChanged();
        void onIconColorCheckedChanged();
        void onToggle(bool checked);
};

}  // namespace KDC
