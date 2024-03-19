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
#include <QDialog>
#include <QPaintEvent>
#include <QPoint>
#include <QString>

namespace KDC {

class CustomToolTip : public QDialog {
        Q_OBJECT

        Q_PROPERTY(QColor background_color READ backgroundColor WRITE setBackgroundColor)

    public:
        CustomToolTip(const QString &text, const QPoint &position, int toolTipDuration, QWidget *parent = nullptr);

    private:
        QPoint _cursorPosition;
        QColor _backgroundColor;

        void paintEvent(QPaintEvent *event) override;

        inline QColor backgroundColor() const { return _backgroundColor; }
        inline void setBackgroundColor(const QColor &color) { _backgroundColor = color; }
};

}  // namespace KDC
