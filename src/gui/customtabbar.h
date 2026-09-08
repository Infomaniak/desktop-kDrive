/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include <QTabBar>

namespace KDC {

class CustomTabBar : public QTabBar {
        Q_OBJECT

        Q_PROPERTY(QColor tab_selected_text_color MEMBER _tabSelectedTextColor)
        Q_PROPERTY(QColor tab_text_color MEMBER _tabTextColor)

    public:
        CustomTabBar(QWidget *parent = nullptr);

        inline void setUnResolvedNotifCount(const Count nbNotif) { _unResolvedNotifCount = nbNotif; }
        inline void setAutoResolvedNotifCount(const Count nbNotif) { _autoResolvedNotifCount = nbNotif; }

    protected:
        void paintEvent(QPaintEvent *) override;

    private:
        Count _unResolvedNotifCount{0};
        Count _autoResolvedNotifCount{0};

        QColor _tabTextColor;
        QColor _tabSelectedTextColor;
};

} // namespace KDC
