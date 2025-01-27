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

#include "customtooltip.h"

#include <QColor>
#include <QPaintEvent>
#include <QToolButton>
#include <QSize>
#include <QString>

namespace KDC {

class CustomToolButton : public QToolButton {
        Q_OBJECT

        Q_PROPERTY(QSize base_icon_size READ baseIconSize WRITE setBaseIconSize)
        Q_PROPERTY(QColor icon_color READ iconColor WRITE setIconColor)
        Q_PROPERTY(QColor icon_color_hover READ iconColorHover WRITE setIconColorHover)

    public:
        explicit CustomToolButton(QWidget *parent = nullptr);

        inline void setIconPath(const QString &path) { _iconPath = path; }
        void setWithMenu(bool withMenu);
        inline void setToolTip(const QString &text) { _toolTipText = text; }

    signals:
        void baseIconSizeChanged();
        void iconColorChanged();

    private:
        bool _withMenu;
        QSize _baseIconSize;
        QString _iconPath;
        QColor _iconColor;
        QColor _iconColorHover;
        QString _toolTipText;
        int _toolTipDuration;
        bool _hover;
        CustomToolTip *_customToolTip;

        bool event(QEvent *event) override;
        void enterEvent(QEnterEvent *event) override;
        void leaveEvent(QEvent *event) override;

        inline QSize baseIconSize() const { return _baseIconSize; }
        inline void setBaseIconSize(const QSize &size) {
            _baseIconSize = size;
            emit baseIconSizeChanged();
        }

        inline QColor iconColor() const { return _iconColor; }
        inline void setIconColor(const QColor &color) {
            _iconColor = color;
            emit iconColorChanged();
        }

        inline QColor iconColorHover() const { return _iconColorHover; }
        inline void setIconColorHover(const QColor &color) { _iconColorHover = color; }

        void applyIconSizeAndColor();
        void setHover(bool hover);

    private slots:
        void onBaseIconSizeChanged();
        void onIconColorChanged();
        void onClicked(bool checked = false);
};

} // namespace KDC
