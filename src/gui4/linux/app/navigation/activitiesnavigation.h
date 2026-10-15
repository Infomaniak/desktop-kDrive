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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QObject>

#include <cstdint>

namespace KDC {

/**
 * Sub-page shown in the Activities tab.
 *
 * Role: hold the current page of a fixed chain (list, errors, conflicts) and go back to its parent.
 * Non-role: decide when a page must close; the controller owning that rule calls this navigation.
 */
class ActivitiesNavigation final : public QObject {
        Q_OBJECT
        Q_PROPERTY(Page currentPage READ currentPage NOTIFY currentPageChanged)
        Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY currentPageChanged)

    public:
        enum class Page : uint8_t {
            List = 0,
            Errors,
            ConflictsQuick,
            ConflictsList,
        };
        Q_ENUM(Page)

        explicit ActivitiesNavigation(QObject *parent = nullptr);

        [[nodiscard]] Page currentPage() const { return _currentPage; }
        [[nodiscard]] bool canGoBack() const { return _currentPage != Page::List; }

        Q_INVOKABLE void show(Page page);
        Q_INVOKABLE void back();
        void reset();

    signals:
        void currentPageChanged();

    private:
        void setCurrentPage(Page page);

        Page _currentPage{Page::List};
};

} // namespace KDC
