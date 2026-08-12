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

#include "app/cache/activitystore.h"
#include "app/cache/appcache.h"
#include "app/cache/mainselectionstore.h"

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QTimer>

#include <cstdint>
#include <optional>
#include <vector>

namespace KDC {

/**
 * QML-facing projection of recent activities and active node errors for the selected synchronization.
 *
 * ActivityStore remains the bounded recent-history owner and AppCache remains the authoritative active-error owner.
 * This model joins both sources without moving either lifecycle into the presentation layer.
 */
class ActivityListModel final : public QAbstractListModel {
        Q_OBJECT

    public:
        enum class Filter : uint8_t {
            MyActivityOnly,
            AllActivities,
        };
        Q_ENUM(Filter)

        enum class Status : uint8_t {
            Synchronized,
            InProgress,
            Failed,
        };
        Q_ENUM(Status)

        enum class Source : uint8_t {
            Unknown,
            Computer,
            Web,
        };
        Q_ENUM(Source)

        enum Role {
            RowIdRole = Qt::UserRole + 1,
            NameRole,
            FolderRole,
            TimeTextRole,
            SizeTextRole,
            NodeTypeRole,
            StatusRole,
            SourceRole,
            InstructionRole,
            ProgressRole,
            HasActiveErrorRole,
            ActiveErrorCountRole,
            HasOptionsRole,
            CanOpenLocalRole,
            CanOpenOnlineRole,
            CanCopyShareLinkRole,
            CanFixErrorsRole,
        };
        Q_ENUM(Role)

        struct ActionTarget {
                GenericId activityLocalId{0};
                SyncDbId syncDbId{0};
                SyncPath relativePath;
                NodeType nodeType{NodeType::Unknown};
                NodeId remoteNodeId;
                std::vector<ErrorDbId> activeErrorDbIds;
                bool canOpenLocal{false};
                bool canOpenOnline{false};
                bool canCopyShareLink{false};
                bool canFixErrors{false};
        };

        explicit ActivityListModel(const ActivityStore &activityStore, const AppCache &appCache,
                                   MainSelectionStore &selectionStore, QObject *parent = nullptr);

        /** Returns the stable model row identifier for an activity. */
        [[nodiscard]] static QString activityRowId(GenericId localId);

        [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        [[nodiscard]] Filter filter() const { return _filter; }
        void setFilter(Filter filter);
        [[nodiscard]] std::optional<ActionTarget> actionTarget(const QString &rowId) const;

    signals:
        void filterChanged();
        void projectionChanged();

    private:
        struct Row {
                QString rowId;
                GenericId activityLocalId{0};
                SyncDbId syncDbId{0};
                QString name;
                QString folder;
                QString timeText;
                QString sizeText;
                NodeType nodeType{NodeType::Unknown};
                Status status{Status::Synchronized};
                Source source{Source::Unknown};
                SyncFileInstruction instruction{SyncFileInstruction::None};
                int32_t progress{0};
                QDateTime timestampUtc;
                Count receivedSequence{0};
                SyncPath relativePath;
                SyncPath sourcePath;
                SyncPath destinationPath;
                NodeId localNodeId;
                NodeId remoteNodeId;
                std::vector<ErrorDbId> activeErrorDbIds;
                bool canOpenLocal{false};
                bool canOpenOnline{false};
                bool canCopyShareLink{false};
                bool canFixErrors{false};

                friend bool operator==(const Row &lhs, const Row &rhs) = default;
        };

        [[nodiscard]] std::vector<Row> buildProjection() const;
        [[nodiscard]] std::vector<Row> activityRows(SyncDbId syncDbId) const;
        static void appendActiveErrors(SyncDbId syncDbId, const std::vector<Error> &errors, std::vector<Row> &rows);
        static void appendActiveError(SyncDbId syncDbId, const Error &error, std::vector<Row> &rows);
        [[nodiscard]] static Row makeActivityRow(SyncDbId syncDbId, const ActivityEntry &activity);
        [[nodiscard]] static Row makeErrorRow(SyncDbId syncDbId, const Error &error);
        [[nodiscard]] static Row *findMatchingActivity(std::vector<Row> &rows, const Error &error);
        [[nodiscard]] static int32_t errorMatchScore(const Row &row, const Error &error);
        void finalizeProjection(std::vector<Row> &rows) const;
        void resetProjection();
        void reconcileProjection();
        [[nodiscard]] bool removeMissingRows(const std::vector<Row> &nextRows);
        [[nodiscard]] bool alignRows(const std::vector<Row> &nextRows);
        [[nodiscard]] bool updateRow(qsizetype rowIndex, const Row &nextRow);
        void refreshRelativeTimes();

        const ActivityStore &_activityStore;
        const AppCache &_appCache;
        MainSelectionStore &_selectionStore;
        std::vector<Row> _rows;
        Filter _filter{Filter::MyActivityOnly};
        QTimer _relativeTimeTimer;
};

} // namespace KDC
