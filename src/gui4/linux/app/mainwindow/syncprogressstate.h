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

#include <cstdint>

namespace KDC {

class MainSelectionStore;

/**
 * QML-facing progress of the selected synchronization, shared by Home and Activities.
 *
 * It exposes a phase rather than a text, so that QML owns the translation. Observers are only notified when a displayed value
 * changes, not on every progress tick.
 */
class SyncProgressState final : public QObject {
        Q_OBJECT
        Q_PROPERTY(Phase phase READ phase NOTIFY progressChanged)
        Q_PROPERTY(qint32 completedItems READ completedItems NOTIFY progressChanged)
        Q_PROPERTY(qint32 totalItems READ totalItems NOTIFY progressChanged)

    public:
        enum class Phase : uint8_t {
            None = 0,
            Exploring,
            Analysing,
            Propagating,
        };
        Q_ENUM(Phase)

        explicit SyncProgressState(const MainSelectionStore &mainSelectionStore, QObject *parent = nullptr);

        [[nodiscard]] Phase phase() const { return _phase; }
        [[nodiscard]] qint32 completedItems() const { return _completedItems; }
        [[nodiscard]] qint32 totalItems() const { return _totalItems; }

    signals:
        void progressChanged();

    private:
        void refresh();

        const MainSelectionStore &_mainSelectionStore;
        Phase _phase{Phase::None};
        qint32 _completedItems{0};
        qint32 _totalItems{0};
};

} // namespace KDC
