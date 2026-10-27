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

#include "app/cache/cachetypes.h"
#include "app/errors/syncerrorkind.h"
#include "app/fileiconresolver.h"

#include <QAbstractListModel>
#include <QString>

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace KDC {

class MainSelectionStore;
class SyncErrorActionHandler;
class SyncErrorService;
class SyncErrorsProjection;
class SyncService;
struct ClassifiedSyncError;

/**
 * QML-facing rows of the errors page, for the selected synchronization.
 *
 * One row per error, sorted by section (`SyncErrorCategory` order) then by database id. Up to `kMaxIndividualConflicts`
 * version conflicts are listed with the files to verify; beyond it, a single aggregated row stands for all of them in
 * the conflicts section. Texts are exposed as translation ids, translated by QML. Rows are updated in place, so that the
 * view keeps its scroll position and focus.
 */
class ErrorListModel final : public QAbstractListModel {
        Q_OBJECT

    public:
        enum Role {
            RowIdRole = Qt::UserRole + 1,
            SectionTitleIdRole,
            TitleIdRole,
            TitleArgCountRole,
            TitleCountRole,
            DescriptionIdRole,
            DescriptionArgCountRole,
            DescriptionCountRole,
            NodeLabelIdRole,
            ItemNameRole,
            ItemPathRole,
            ItemIconNameRole,
            IsDirectoryRole,
            VariantRole,
            CopyablePathRole,
            TechnicalDetailsRole,
            ActionLabelIdRole,
            ActionLabelArgCountRole,
            ActionExternalRole,
            ActionPendingRole,
        };
        Q_ENUM(Role)

        ErrorListModel(const SyncErrorsProjection &projection, const MainSelectionStore &selectionStore,
                       const SyncService &syncService, const SyncErrorService &syncErrorService,
                       const SyncErrorActionHandler &actionHandler, QObject *parent = nullptr);

        [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        /** Returns the error of an error row, or `std::nullopt` for an unknown row or the aggregated conflicts row. */
        [[nodiscard]] std::optional<ErrorDbId> errorDbId(const QString &rowId) const;
        /** Returns the technical details of a row as "Name: value" lines, or an empty string. */
        [[nodiscard]] QString technicalDetailsText(const QString &rowId) const;
        /** Returns the first row, in display order, that stands for one of `errorDbIds`, or -1. */
        [[nodiscard]] int32_t firstRowOf(const std::vector<ErrorDbId> &errorDbIds) const;

    private:
        struct Row {
                QString rowId;
                SyncErrorCategory section{SyncErrorCategory::SystemAndPermissions};
                ErrorDbId errorDbId{0};
                QString titleId;
                uint8_t titleArgCount{0};
                int32_t titleCount{0};
                QString descriptionId;
                uint8_t descriptionArgCount{0};
                // Number shown in the description, such as the inotify watches limit; 0 when it has none.
                qint64 descriptionCount{0};
                QString nodeLabelId;
                QString itemName;
                QString itemPath;
                QString itemIconName;
                bool isDirectory{false};
                SyncErrorRowVariant variant{SyncErrorRowVariant::Standard};
                QString copyablePath;
                std::vector<std::pair<QString, QString>> technicalDetails;
                QString actionLabelId;
                uint8_t actionLabelArgCount{0};
                bool actionExternal{false};
                bool actionPending{false};
                // Errors the aggregated conflicts row stands for.
                std::vector<ErrorDbId> conflictDbIds;

                friend bool operator==(const Row &lhs, const Row &rhs) = default;
        };

        void reset();
        void refresh();
        [[nodiscard]] std::vector<Row> buildRows() const;
        [[nodiscard]] Row makeErrorRow(const ClassifiedSyncError &classified, SyncErrorCategory section,
                                       const std::optional<SyncContext> &context) const;
        [[nodiscard]] static Row makeConflictsRow(std::vector<ErrorDbId> conflictDbIds);
        [[nodiscard]] bool isActionPending(SyncErrorAction action) const;
        void updateRow(int32_t rowIndex, Row nextRow);

        const SyncErrorsProjection &_projection;
        const MainSelectionStore &_selectionStore;
        const SyncService &_syncService;
        const SyncErrorService &_syncErrorService;
        const SyncErrorActionHandler &_actionHandler;
        FileIconResolver _fileIconResolver;
        std::vector<Row> _rows;
};

} // namespace KDC
