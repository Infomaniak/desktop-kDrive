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

#pragma once

#include <QColor>
#include <QLabel>
#include <QPushButton>
#include <QSize>

namespace KDC {

class CustomPushButton : public QPushButton {
        Q_OBJECT

        Q_PROPERTY(QSize icon_size READ iconSize WRITE setIconSize)
        Q_PROPERTY(QColor icon_color READ iconColor WRITE setIconColor)

    public:
        explicit CustomPushButton(const QString &path, const QString &text, QWidget *parent = nullptr);
        explicit CustomPushButton(const QString &text, QWidget *parent = nullptr);
        explicit CustomPushButton(QWidget *parent = nullptr);
        QSize sizeHint() const override;
        void setText(const QString &text);

    signals:
        void iconSizeChanged();
        void iconColorChanged();

    private:
        QString _iconPath;
        QString _text;
        QSize _iconSize;
        QColor _iconColor;
        QLabel *_iconLabel;
        QLabel *_textLabel;

        inline QSize iconSize() const { return _iconSize; }
        inline void setIconSize(QSize size) {
            _iconSize = size;
            emit iconSizeChanged();
        }

        inline QColor iconColor() const { return _iconColor; }
        inline void setIconColor(QColor color) {
            _iconColor = color;
            emit iconColorChanged();
        }

        void setIcon();

    private slots:
        void onIconSizeChanged();
        void onIconColorChanged();
};

} // namespace KDC
