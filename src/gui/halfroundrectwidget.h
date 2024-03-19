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
#include <QHBoxLayout>
#include <QPaintEvent>
#include <QWidget>

namespace KDC {

class HalfRoundRectWidget : public QWidget {
        Q_OBJECT

        Q_PROPERTY(QColor bottom_corners_color READ bottomCornersColor WRITE setBottomCornersColor)

    public:
        explicit HalfRoundRectWidget(QWidget *parent = nullptr);

        void setContentsMargins(int left, int top, int right, int bottom);
        void setSpacing(int spacing);
        void addWidget(QWidget *widget, int stretch = 0, Qt::Alignment alignment = Qt::Alignment());
        void addLayout(QLayout *layout, int stretch = 0);
        void addStretch(int stretch = 0);
        void addSpacing(int size);
        bool setStretchFactor(QWidget *widget, int stretch);
        bool setStretchFactor(QLayout *layout, int stretch);

    private:
        QColor _bottomCornersColor;
        QHBoxLayout *_hboxLayout;

        void paintEvent(QPaintEvent *event) override;

        inline QColor bottomCornersColor() const { return _bottomCornersColor; }
        inline void setBottomCornersColor(const QColor &value) { _bottomCornersColor = value; }
};

}  // namespace KDC
