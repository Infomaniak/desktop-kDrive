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

#include "app/cache/mainselectionstore.h"

#include <QAbstractListModel>
#include <QColor>
#include <QString>

#include <vector>

namespace KDC {

/**
 * QML list adapter for the main-sidebar synchronization selector.
 *
 * Rows contain only selector presentation data. AppCache owns entities and MainSelectionStore remains the selection
 * authority; this model does not own navigation, desktop actions, or screen state.
 */
class SyncSelectorModel final : public QAbstractListModel {
        Q_OBJECT
        Q_PROPERTY(qint32 selectedRow READ selectedRow NOTIFY selectedRowChanged)

    public:
        enum class EntryType : uint8_t {
            ClassicSync,
            AdvancedSync,
        };
        Q_ENUM(EntryType)

        enum Role {
            EntryTypeRole = Qt::UserRole + 1,
            SyncDbIdRole,
            TitleRole,
            SubtitleRole,
            DriveColorRole,
            ErrorCountRole,
            WarningRole,
            SelectedRole,
        };
        Q_ENUM(Role)

        explicit SyncSelectorModel(const AppCache &cache, MainSelectionStore &selectionStore, QObject *parent = nullptr);

        [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
        [[nodiscard]] qint32 selectedRow() const;

    signals:
        void selectedRowChanged();

    private:
        struct Entry {
                EntryType type{EntryType::ClassicSync};
                SyncDbId syncDbId{0};
                QString title;
                QString subtitle;
                QColor driveColor;
                qint32 errorCount{0};
                bool warning{false};
        };

        void rebuild();
        void appendDriveEntries(std::vector<Entry> &entries, const DriveContext &driveContext) const;
        void handleSelectionChanged();
        [[nodiscard]] qint32 rowForSyncDbId(SyncDbId syncDbId) const;

        const AppCache &_cache;
        MainSelectionStore &_selectionStore;
        std::vector<Entry> _entries;
        SyncDbId _selectedSyncDbId{0};
};

} // namespace KDC
