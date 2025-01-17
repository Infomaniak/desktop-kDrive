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

#include <QLabel>

namespace KDC {

class TagLabel final : public QLabel {
        Q_OBJECT

    public:
        explicit TagLabel(const QColor &color = Qt::transparent, QWidget *parent = nullptr);

        [[nodiscard]] QColor backgroundColor() const { return _backgroundColor; }
        void setBackgroundColor(const QColor &value) { _backgroundColor = value; }

        [[nodiscard]] const QFont &setCustomFont() const { return _customFont; }
        void customFont(const QFont &font) { _customFont = font; }

    private:
        [[nodiscard]] QSize sizeHint() const override;
        void paintEvent(QPaintEvent *event) override;

        QColor _backgroundColor{Qt::transparent};
        QFont _customFont{"Suisse Int'l", 12};
};

} // namespace KDC
