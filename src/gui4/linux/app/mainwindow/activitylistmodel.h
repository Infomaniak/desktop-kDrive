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
#include "app/fileiconresolver.h"

#include <QAbstractListModel>
#include <QDateTime>
#include <QFont>
#include <QFlags>
#include <QList>
#include <QString>
#include <QStringList>
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
        Q_PROPERTY(QStringList sizeTextSamples READ sizeTextSamples NOTIFY translationChanged)

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

        enum AvailableActionFlag : uint8_t {
            NoAvailableAction = 0,
            OpenLocalAction = 1 << 0,
            OpenOnlineAction = 1 << 1,
            CopyShareLinkAction = 1 << 2,
            FixErrorsAction = 1 << 3,
        };
        Q_ENUM(AvailableActionFlag)
        Q_DECLARE_FLAGS(AvailableActions, AvailableActionFlag)
        Q_FLAG(AvailableActions)

        enum Role {
            RowIdRole = Qt::UserRole + 1,
            NameRole,
            FileIconNameRole,
            SubtitleTextRole,
            FolderRole,
            SizeTextRole,
            NodeTypeRole,
            StatusRole,
            SourceRole,
            InstructionRole,
            IsDirectoryRole,
            ProgressRole,
            HasActiveErrorRole,
            ActiveErrorCountRole,
            AvailableActionsRole,
        };
        Q_ENUM(Role)

        struct ActionTarget {
                GenericId activityLocalId{0};
                SyncDbId syncDbId{0};
                SyncPath relativePath;
                NodeId remoteNodeId;
                std::vector<ErrorDbId> activeErrorDbIds;
        };

        void retranslate();

        explicit ActivityListModel(const ActivityStore &activityStore, const AppCache &appCache,
                                   MainSelectionStore &selectionStore, QObject *parent = nullptr);

        /** Returns the stable model row identifier for an activity. */
        [[nodiscard]] static QString activityRowId(GenericId localId);

        /** Widest strings the size column can render in the active locale. Notified when the application language changes. */
        [[nodiscard]] static QStringList sizeTextSamples();

        /** Widest advance width of @p texts rendered with @p font. */
        [[nodiscard]] Q_INVOKABLE static qreal maxTextWidth(const QStringList &texts, const QFont &font);

        [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        [[nodiscard]] Filter filter() const { return _filter; }
        void setFilter(Filter filter);
        [[nodiscard]] std::optional<ActionTarget> actionTarget(const QString &rowId) const;

    signals:
        void translationChanged();
        void filterChanged();
        void projectionChanged();

    private:
        using MatchScore = uint8_t;

        enum class SubtitleKind : uint8_t {
            TimeOnly,
            Updated,
            Removed,
            Renamed,
            Moved,
            Imported,
            Added,
        };

        static constexpr MatchScore noMatchScore = 0;
        static constexpr MatchScore pathMatchScore = 1;
        static constexpr MatchScore remoteNodeIdMatchScore = 2;
        static constexpr MatchScore localNodeIdMatchScore = 3;

        struct Row {
                QString rowId;
                GenericId activityLocalId{0};
                SyncDbId syncDbId{0};
                QString name;
                QString fileIconName;
                QString subtitleText;
                QString folder;
                QString sizeText;
                SubtitleKind subtitleKind{SubtitleKind::TimeOnly};
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

                friend bool operator==(const Row &lhs, const Row &rhs) = default;
        };

        [[nodiscard]] std::vector<Row> buildProjection() const;
        [[nodiscard]] std::vector<Row> activityRows(SyncDbId syncDbId) const;
        void appendActiveErrors(SyncDbId syncDbId, const std::vector<Error> &errors, std::vector<Row> &rows) const;
        void appendActiveError(SyncDbId syncDbId, const Error &error, std::vector<Row> &rows) const;
        [[nodiscard]] Row makeActivityRow(SyncDbId syncDbId, const ActivityEntry &activity) const;
        [[nodiscard]] Row makeErrorRow(SyncDbId syncDbId, const Error &error) const;
        [[nodiscard]] static Row *findMatchingActivity(std::vector<Row> &rows, const Error &error);
        [[nodiscard]] static MatchScore errorMatchScore(const Row &row, const Error &error);
        [[nodiscard]] static AvailableActions availableActions(const Row &row);
        [[nodiscard]] static SubtitleKind subtitleKind(const ActivityEntry &activity);
        [[nodiscard]] static QString formatSubtitle(SubtitleKind kind, const QDateTime &timestampUtc,
                                                    const QDateTime &nowUtc = QDateTime::currentDateTimeUtc());
        void finalizeProjection(std::vector<Row> &rows) const;
        void resetProjection();
        void scheduleProjectionReconciliation();
        void reconcileProjection();
        [[nodiscard]] bool removeStaleRows(const std::vector<Row> &nextRows);
        [[nodiscard]] bool applyProjectionRows(const std::vector<Row> &nextRows);
        [[nodiscard]] bool updateRow(qsizetype rowIndex, const Row &nextRow);
        void refreshSubtitles();

        const ActivityStore &_activityStore;
        const AppCache &_appCache;
        MainSelectionStore &_selectionStore;
        std::vector<Row> _rows;
        Filter _filter{Filter::MyActivityOnly};
        FileIconResolver _fileIconResolver;
        QTimer _projectionRefreshTimer;
        QTimer _subtitleRefreshTimer;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ActivityListModel::AvailableActions)

} // namespace KDC
