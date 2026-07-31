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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QObject>

namespace KDC {

/**
 * Process-long network reachability adapter for the Linux v4 presentation layer.
 *
 * Only explicit disconnected reachability is considered offline. Other reachability levels deliberately preserve the
 * cache-derived synchronization state so an inconclusive connectivity probe does not block otherwise usable controls.
 */
class NetworkStatusObserver final : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool offline READ offline NOTIFY offlineChanged)

    public:
        explicit NetworkStatusObserver(QObject *parent = nullptr);

        [[nodiscard]] bool offline() const { return _offline; }

    signals:
        void offlineChanged();

    private:
        void updateReachability();

        bool _offline{false};
};

} // namespace KDC
