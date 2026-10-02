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

#include "libcommon/utility/types.h"

#include <QAbstractListModel>
#include <QString>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace KDC {

/**
 * Rows of the Settings "Advanced sync" page: one per advanced synchronization of the managed drive.
 *
 * Role: present display values only. `AdvancedSyncsController` resolves the synchronizations from AppCache and owns the
 * requests; the model keeps each row's blacklist summary across refreshes of the same synchronization.
 */
class AdvancedSyncListModel final : public QAbstractListModel {
        Q_OBJECT

    public:
        enum Role : int32_t {
            SyncDbIdRole = Qt::UserRole + 1,
            LocalFolderNameRole,
            LocalPathRole,
            RemoteFolderNameRole,
            RemotePathRole,
            CustomSelectionRole,
            BlackListLoadingRole,
            BlackListLoadFailedRole,
            DeletePendingRole,
        };
        Q_ENUM(Role)

        enum class BlackListState : uint8_t {
            Idle,
            Loading,
            Loaded,
            Failed,
        };

        struct Row {
                SyncDbId syncDbId{0};
                QString localFolderName;
                QString localPath;
                QString remoteFolderName;
                QString remotePath;
                BlackListState blackListState{BlackListState::Idle};
                // Confirmed non-empty blacklist; kept while a reload is in flight.
                bool customSelection{false};
                bool deletePending{false};
        };

        explicit AdvancedSyncListModel(QObject *parent = nullptr);

        [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        [[nodiscard]] const std::vector<Row> &rows() const { return _rows; }
        [[nodiscard]] const Row *row(SyncDbId syncDbId) const;

        /**
         * Replaces the rows with the given synchronizations, in order. A synchronization already present keeps its
         * blacklist summary; the returned ids are the new ones, whose summary still has to be loaded.
         */
        std::vector<SyncDbId> replaceRows(std::vector<Row> rows);
        void setBlackListState(SyncDbId syncDbId, BlackListState state);
        void setCustomSelection(SyncDbId syncDbId, bool customSelection);
        void setDeletePending(SyncDbId syncDbId, bool deletePending);

    private:
        [[nodiscard]] std::optional<std::size_t> indexOf(SyncDbId syncDbId) const;
        [[nodiscard]] static int32_t toModelRow(std::size_t rowIndex);
        // `roles` is a QList<int> because dataChanged() takes one.
        void notifyRow(std::size_t rowIndex, const QList<int> &roles);

        std::vector<Row> _rows;
};

} // namespace KDC
