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

#include "app/fileiconresolver.h"
#include "libcommon/utility/types.h"

#include <QAbstractListModel>
#include <QCollator>
#include <QString>

#include <cstdint>
#include <optional>
#include <vector>

namespace KDC {

class MainSelectionStore;
class SyncErrorService;
class SyncErrorsProjection;

/**
 * QML-facing list of the version conflicts of the selected synchronization ("Process the files individually" page).
 *
 * One row per conflict, sorted by relative path regardless of case, and filtered by `searchText` on that path. Rows are
 * updated in place while conflicts are resolved, so that the view keeps its scroll position and focus.
 */
class ConflictListModel final : public QAbstractListModel {
        Q_OBJECT
        Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
        Q_PROPERTY(int32_t count READ count NOTIFY countChanged)

    public:
        enum Role {
            RowIdRole = Qt::UserRole + 1,
            NameRole,
            PathRole,
            IconNameRole,
            IsDirectoryRole,
            ActionPendingRole,
        };
        Q_ENUM(Role)

        ConflictListModel(const SyncErrorsProjection &projection, const MainSelectionStore &selectionStore,
                          const SyncErrorService &syncErrorService, QObject *parent = nullptr);

        [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        [[nodiscard]] QString searchText() const { return _searchText; }
        void setSearchText(const QString &searchText);
        /** Number of rows shown, after the search filter. */
        [[nodiscard]] int32_t count() const { return static_cast<int32_t>(_rows.size()); }

        /** Every conflict, in display order, whatever the search filter. */
        [[nodiscard]] std::vector<ErrorDbId> errorDbIds() const;
        /** Returns the conflict of a shown row, or `std::nullopt`. */
        [[nodiscard]] std::optional<ErrorDbId> errorDbId(const QString &rowId) const;

    signals:
        void searchTextChanged();
        void countChanged();

    private:
        struct Row {
                QString rowId;
                ErrorDbId errorDbId{0};
                QString name;
                QString path;
                QString iconName;
                bool isDirectory{false};
                bool actionPending{false};

                friend bool operator==(const Row &lhs, const Row &rhs) = default;
        };

        [[nodiscard]] std::vector<Row> buildRows(bool filtered) const;
        [[nodiscard]] bool isBefore(const Row &lhs, const Row &rhs) const;
        void reset();
        void refresh();
        void updateRow(int32_t rowIndex, Row nextRow);

        const SyncErrorsProjection &_projection;
        const MainSelectionStore &_selectionStore;
        const SyncErrorService &_syncErrorService;
        FileIconResolver _fileIconResolver;
        QCollator _collator;
        QString _searchText;
        std::vector<Row> _rows;
};

} // namespace KDC
