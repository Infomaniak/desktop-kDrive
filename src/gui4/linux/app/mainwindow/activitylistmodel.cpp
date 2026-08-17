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

#include "app/mainwindow/activitylistmodel.h"

#include "libcommon/utility/types.h"

#include <QCoreApplication>
#include <QLoggingCategory>
#include <QLocale>
#include <QSet>

#include <algorithm>
#include <chrono>
#include <qtimezone.h>
#include <ranges>

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcActivityListModel, "gui.v4.activitylistmodel", QtInfoMsg)

constexpr auto relativeTimeRefreshInterval = std::chrono::minutes{1};
constexpr auto projectionRefreshInterval = std::chrono::milliseconds{100};
constexpr auto justNowThreshold = std::chrono::seconds{30};
constexpr auto second = std::chrono::seconds{1};
constexpr auto minute = std::chrono::minutes{1};
constexpr auto hour = std::chrono::hours{1};
constexpr auto day = std::chrono::days{1};
constexpr auto relativeDateThreshold = std::chrono::days{4};

QString errorRowId(const ErrorDbId errorDbId) {
    return QStringLiteral("error:%1").arg(static_cast<qlonglong>(errorDbId));
}

SyncPath normalizedRelativePath(const SyncPath &path) {
    return path.relative_path().lexically_normal();
}

QString itemName(const SyncPath &path) {
    return Path2QStr(path.filename());
}

QString parentFolder(const SyncPath &path) {
    const auto parent = path.parent_path();
    return parent.empty() || parent == SyncPath{"."} ? QString{} : Path2QStr(parent);
}

QString formatSize(const NodeType nodeType, const int64_t size) {
    if (nodeType == NodeType::Directory || size < 0) {
        return {};
    }
    return QLocale().formattedDataSize(size);
}

QString formatAgo(const std::chrono::seconds elapsed, const std::chrono::seconds unit, const char *const unitTranslationId) {
    const auto unitCount = elapsed / unit;
    const QString duration = QStringLiteral("%1 %2").arg(unitCount).arg(qtTrId(unitTranslationId));
    return qtTrId("labelAgo").arg(duration);
}

QString formatRelativeTime(const QDateTime &timestampUtc, const QDateTime &nowUtc = QDateTime::currentDateTimeUtc()) {
    if (!timestampUtc.isValid()) {
        return {};
    }

    const auto elapsed = std::chrono::seconds{std::max<qint64>(0, timestampUtc.secsTo(nowUtc))};
    if (elapsed < justNowThreshold) {
        return qtTrId("labelJustNow");
    }

    if (elapsed < minute) {
        return formatAgo(elapsed, second, "labelShortSecond");
    }
    if (elapsed < hour) {
        return formatAgo(elapsed, minute, "labelShortMinute");
    }
    if (elapsed < day) {
        return formatAgo(elapsed, hour, "labelShortHour");
    }
    if (elapsed < relativeDateThreshold) {
        return formatAgo(elapsed, day, "labelShortDay");
    }
    return QLocale().toString(timestampUtc.toLocalTime().date(), QLocale::ShortFormat);
}

ActivityListModel::Status toModelStatus(const SyncFileStatus status) {
    switch (status) {
        case SyncFileStatus::Success:
            return ActivityListModel::Status::Synchronized;
        case SyncFileStatus::Syncing:
            return ActivityListModel::Status::InProgress;
        case SyncFileStatus::Unknown:
        case SyncFileStatus::Error:
        case SyncFileStatus::Conflict:
        case SyncFileStatus::Inconsistency:
        case SyncFileStatus::Ignored:
        case SyncFileStatus::EnumEnd:
            return ActivityListModel::Status::Failed;
    }
    return ActivityListModel::Status::Failed;
}

ActivityListModel::Source toModelSource(const SyncDirection direction) {
    switch (direction) {
        case SyncDirection::Up:
            return ActivityListModel::Source::Computer;
        case SyncDirection::Down:
            return ActivityListModel::Source::Web;
        case SyncDirection::Unknown:
        case SyncDirection::EnumEnd:
            return ActivityListModel::Source::Unknown;
    }
    return ActivityListModel::Source::Unknown;
}

QString activityActionText(const ActivityEntry &activity) {
    switch (activity.instruction) {
        using enum SyncFileInstruction;

        case UpdateMetadata: // We shouldn't receive an UpdateMetadata on linux, reserved instruction for macOS and Windows for
                             // the litesync. Here for possible future litesync linux implem.
        case Update:
            return qtTrId("activityInstructionUpdateLabel");
        case Remove:
            return qtTrId("activityInstructionRemoveLabel");
        case Move: {
            if (!activity.path.empty() && !activity.newPath.empty() &&
                normalizedRelativePath(activity.path).parent_path() == normalizedRelativePath(activity.newPath).parent_path()) {
                return qtTrId("activityInstructionRenameLabel");
            }
            return qtTrId("activityInstructionMoveLabel");
        }
        case Get:
            return qtTrId("activityInstructionGetLabel");
        case Put:
            return qtTrId("activityInstructionPutLabel");
        case Ignore:
            return qtTrId("activityInstructionIgnoreLabel");
        case None:
        case EnumEnd:
            return {};
    }
    return {};
}

} // namespace

QString ActivityListModel::activityRowId(const GenericId localId) {
    return QStringLiteral("activity:%1").arg(static_cast<qlonglong>(localId));
}

ActivityListModel::ActivityListModel(const ActivityStore &activityStore, const AppCache &appCache,
                                     MainSelectionStore &selectionStore, QObject *const parent) :
    QAbstractListModel(parent),
    _activityStore(activityStore),
    _appCache(appCache),
    _selectionStore(selectionStore) {
    (void) connect(&_activityStore, &ActivityStore::activitiesChanged, this, [this](const SyncDbId syncDbId) {
        if (syncDbId == static_cast<SyncDbId>(_selectionStore.currentSyncDbId())) {
            scheduleProjectionReconciliation();
        }
    });
    (void) connect(&_appCache, &AppCache::syncErrorsChanged, this, &ActivityListModel::scheduleProjectionReconciliation);
    (void) connect(&_selectionStore, &MainSelectionStore::currentSyncDbIdChanged, this, [this] {
        // Keep the bounded icon cache scoped to the selected synchronization.
        _fileIconResolver.clear();
        resetProjection();
    });

    _projectionRefreshTimer.setInterval(projectionRefreshInterval);
    _projectionRefreshTimer.setSingleShot(true);
    (void) connect(&_projectionRefreshTimer, &QTimer::timeout, this, &ActivityListModel::reconcileProjection);

    _relativeTimeTimer.setInterval(relativeTimeRefreshInterval);
    (void) connect(&_relativeTimeTimer, &QTimer::timeout, this, &ActivityListModel::refreshRelativeTimes);
    _relativeTimeTimer.start();
    resetProjection();
}

int ActivityListModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(_rows.size());
}

QVariant ActivityListModel::data(const QModelIndex &index, const int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    const auto &row = _rows[static_cast<std::size_t>(index.row())];
    switch (role) {
        case RowIdRole:
            return row.rowId;
        case NameRole:
        case Qt::DisplayRole:
            return row.name;
        case FileIconNameRole:
            return row.fileIconName;
        case ActionTextRole:
            return row.actionText;
        case FolderRole:
            return row.folder;
        case TimeTextRole:
            return row.timeText;
        case SizeTextRole:
            return row.sizeText;
        case NodeTypeRole:
            return static_cast<int32_t>(row.nodeType);
        case StatusRole:
            return QVariant::fromValue(row.status);
        case SourceRole:
            return QVariant::fromValue(row.source);
        case InstructionRole:
            return static_cast<int32_t>(row.instruction);
        case IsDirectoryRole:
            return row.nodeType == NodeType::Directory;
        case ProgressRole:
            return row.progress;
        case HasActiveErrorRole:
            return !row.activeErrorDbIds.empty();
        case ActiveErrorCountRole:
            return static_cast<qint32>(row.activeErrorDbIds.size());
        default:
            return {};
    }
}

QHash<int, QByteArray> ActivityListModel::roleNames() const {
    return {
            {RowIdRole, "rowId"},
            {NameRole, "name"},
            {FileIconNameRole, "fileIconName"},
            {ActionTextRole, "actionText"},
            {FolderRole, "folder"},
            {TimeTextRole, "timeText"},
            {SizeTextRole, "sizeText"},
            {NodeTypeRole, "nodeType"},
            {StatusRole, "status"},
            {SourceRole, "source"},
            {InstructionRole, "instruction"},
            {IsDirectoryRole, "isDirectory"},
            {ProgressRole, "progress"},
            {HasActiveErrorRole, "hasActiveError"},
            {ActiveErrorCountRole, "activeErrorCount"},
    };
}

void ActivityListModel::setFilter(const Filter filter) {
    switch (filter) {
        case Filter::MyActivityOnly:
        case Filter::AllActivities:
            break;
        default:
            qCWarning(lcActivityListModel) << "Invalid activity filter ignored | filter:" << static_cast<int32_t>(filter);
            return;
    }
    if (_filter == filter) {
        return;
    }
    _filter = filter;
    emit filterChanged();
    resetProjection();
}

std::optional<ActivityListModel::ActionTarget> ActivityListModel::actionTarget(const QString &rowId) const {
    const auto rowIt = std::ranges::find(_rows, rowId, &Row::rowId);
    if (rowIt == _rows.end()) {
        return std::nullopt;
    }
    return ActionTarget{
            .activityLocalId = rowIt->activityLocalId,
            .syncDbId = rowIt->syncDbId,
            .relativePath = rowIt->relativePath,
            .remoteNodeId = rowIt->remoteNodeId,
            .activeErrorDbIds = rowIt->activeErrorDbIds,
    };
}

std::vector<ActivityListModel::Row> ActivityListModel::buildProjection() const {
    const auto syncDbId = static_cast<SyncDbId>(_selectionStore.currentSyncDbId());
    if (syncDbId <= 0) {
        return {};
    }

    auto rows = activityRows(syncDbId);
    appendActiveErrors(syncDbId, _appCache.errorsForSync(syncDbId), rows);
    finalizeProjection(rows);
    return rows;
}

std::vector<ActivityListModel::Row> ActivityListModel::activityRows(const SyncDbId syncDbId) const {
    std::vector<Row> rows;
    const auto activities = _activityStore.activities(syncDbId);
    rows.reserve(activities.size());
    for (const auto &activity: activities) {
        rows.push_back(makeActivityRow(syncDbId, activity));
    }
    return rows;
}

void ActivityListModel::appendActiveErrors(const SyncDbId syncDbId, const std::vector<Error> &errors,
                                           std::vector<Row> &rows) const {
    for (const auto &error: errors) {
        if (error.level() != ErrorLevel::Node) {
            continue;
        }
        appendActiveError(syncDbId, error, rows);
    }
}

void ActivityListModel::appendActiveError(const SyncDbId syncDbId, const Error &error, std::vector<Row> &rows) const {
    if (auto *const matchingRow = findMatchingActivity(rows, error); matchingRow != nullptr) {
        matchingRow->activeErrorDbIds.push_back(error.dbId());
        return;
    }
    rows.push_back(makeErrorRow(syncDbId, error));
}

ActivityListModel::Row ActivityListModel::makeActivityRow(const SyncDbId syncDbId, const ActivityEntry &activity) const {
    const SyncPath &currentPath =
            activity.instruction == SyncFileInstruction::Move && !activity.newPath.empty() ? activity.newPath : activity.path;
    const SyncPath relativePath = normalizedRelativePath(currentPath);
    const auto status = toModelStatus(activity.status);

    Row row;
    row.rowId = activityRowId(activity.localId);
    row.activityLocalId = activity.localId;
    row.syncDbId = syncDbId;
    row.name = itemName(relativePath);
    row.fileIconName = _fileIconResolver.iconName(row.name, activity.nodeType);
    row.actionText = activityActionText(activity);
    row.folder = parentFolder(relativePath);
    row.timeText = formatRelativeTime(activity.receivedAtUtc);
    row.sizeText = formatSize(activity.nodeType, activity.size);
    row.nodeType = activity.nodeType;
    row.status = status;
    row.source = toModelSource(activity.direction);
    row.instruction = activity.instruction;
    row.progress = activity.progress;
    row.timestampUtc = activity.receivedAtUtc;
    row.receivedSequence = activity.receivedSequence;
    row.relativePath = relativePath;
    row.sourcePath = normalizedRelativePath(activity.path);
    row.destinationPath = normalizedRelativePath(activity.newPath);
    row.localNodeId = activity.localNodeId.toStdString();
    row.remoteNodeId = activity.remoteNodeId.toStdString();
    return row;
}

ActivityListModel::Row ActivityListModel::makeErrorRow(const SyncDbId syncDbId, const Error &error) const {
    const SyncPath relativePath =
            normalizedRelativePath(error.destinationPath().empty() ? error.path() : error.destinationPath());
    const QDateTime timestampUtc = error.time() > 0 ? QDateTime::fromSecsSinceEpoch(error.time(), QTimeZone::UTC) : QDateTime{};

    Row row;
    row.rowId = errorRowId(error.dbId());
    row.syncDbId = syncDbId;
    row.name = itemName(relativePath);
    row.fileIconName = _fileIconResolver.iconName(row.name, error.nodeType());
    row.folder = parentFolder(relativePath);
    row.timeText = formatRelativeTime(timestampUtc);
    row.nodeType = error.nodeType();
    row.status = Status::Failed;
    row.timestampUtc = timestampUtc;
    row.relativePath = relativePath;
    row.localNodeId = error.localNodeId();
    row.remoteNodeId = error.remoteNodeId();
    row.activeErrorDbIds.push_back(error.dbId());
    return row;
}

ActivityListModel::Row *ActivityListModel::findMatchingActivity(std::vector<Row> &rows, const Error &error) {
    Row *matchingRow = nullptr;
    MatchScore bestScore = noMatchScore;
    for (auto &row: rows) {
        if (row.status != Status::Failed && row.status != Status::InProgress) {
            continue;
        }
        if (const MatchScore score = errorMatchScore(row, error);
            score > bestScore || (score == bestScore && score > noMatchScore && matchingRow != nullptr &&
                                  row.receivedSequence > matchingRow->receivedSequence)) {
            matchingRow = &row;
            bestScore = score;
        }
    }
    return matchingRow;
}

void ActivityListModel::finalizeProjection(std::vector<Row> &rows) const {
    (void) std::erase_if(rows, [this](const Row &row) {
        const bool resolvedFailure = row.status == Status::Failed && row.activeErrorDbIds.empty();
        const bool filteredRemoteActivity =
                row.activeErrorDbIds.empty() && _filter == Filter::MyActivityOnly && row.source != Source::Computer;
        return resolvedFailure || filteredRemoteActivity;
    });
    (void) std::ranges::sort(rows, [](const Row &lhs, const Row &rhs) {
        const bool lhsInProgress = lhs.status == Status::InProgress;
        const bool rhsInProgress = rhs.status == Status::InProgress;
        if (lhsInProgress != rhsInProgress) {
            return lhsInProgress;
        }
        const bool lhsHasActiveError = !lhs.activeErrorDbIds.empty();
        const bool rhsHasActiveError = !rhs.activeErrorDbIds.empty();
        if (lhsHasActiveError != rhsHasActiveError) {
            return lhsHasActiveError;
        }
        if (lhs.timestampUtc != rhs.timestampUtc) {
            return lhs.timestampUtc > rhs.timestampUtc;
        }
        if (lhs.receivedSequence != rhs.receivedSequence) {
            return lhs.receivedSequence > rhs.receivedSequence;
        }
        return lhs.rowId < rhs.rowId;
    });
}

/**
 * Returns the strongest non-empty node identity shared by an activity row and an active node error.
 *
 * The comparisons mirror the server's `selectErrorByNodeInfo` lookup: local node id, remote node id, source path, then
 * destination path. A stronger match wins when several recent failed or in-progress activities could represent the same
 * error.
 */
ActivityListModel::MatchScore ActivityListModel::errorMatchScore(const Row &row, const Error &error) {
    if (!row.localNodeId.empty() && row.localNodeId == error.localNodeId()) {
        return localNodeIdMatchScore;
    }
    if (!row.remoteNodeId.empty() && row.remoteNodeId == error.remoteNodeId()) {
        return remoteNodeIdMatchScore;
    }
    if (!error.path().empty() && row.sourcePath == normalizedRelativePath(error.path())) {
        return pathMatchScore;
    }
    if (!error.destinationPath().empty() && row.destinationPath == normalizedRelativePath(error.destinationPath())) {
        return pathMatchScore;
    }
    return noMatchScore;
}

void ActivityListModel::resetProjection() {
    _projectionRefreshTimer.stop();
    auto nextRows = buildProjection();
    if (_rows == nextRows) {
        return;
    }
    beginResetModel();
    _rows = std::move(nextRows);
    endResetModel();
    emit projectionChanged();
}

void ActivityListModel::scheduleProjectionReconciliation() {
    if (!_projectionRefreshTimer.isActive()) {
        _projectionRefreshTimer.start();
    }
}

void ActivityListModel::reconcileProjection() {
    const auto nextRows = buildProjection();
    bool changed = removeMissingRows(nextRows);
    changed = applyProjectionRows(nextRows) || changed;
    if (changed) {
        emit projectionChanged();
    }
}

bool ActivityListModel::removeMissingRows(const std::vector<Row> &nextRows) {
    QSet<QString> nextRowIds;
    nextRowIds.reserve(static_cast<qsizetype>(nextRows.size()));
    for (const auto &row: nextRows) {
        (void) nextRowIds.insert(row.rowId);
    }

    bool changed = false;
    for (qsizetype rowIndex = static_cast<qsizetype>(_rows.size()) - 1; rowIndex >= 0; --rowIndex) {
        if (nextRowIds.contains(_rows[static_cast<std::size_t>(rowIndex)].rowId)) {
            continue;
        }
        beginRemoveRows({}, rowIndex, rowIndex);
        (void) _rows.erase(_rows.begin() + rowIndex);
        endRemoveRows();
        changed = true;
    }
    return changed;
}

bool ActivityListModel::applyProjectionRows(const std::vector<Row> &nextRows) {
    bool changed = false;
    for (qsizetype targetIndex = 0; targetIndex < static_cast<qsizetype>(nextRows.size()); ++targetIndex) {
        const auto &nextRow = nextRows[static_cast<std::size_t>(targetIndex)];
        if (targetIndex >= static_cast<qsizetype>(_rows.size())) {
            beginInsertRows({}, targetIndex, targetIndex);
            _rows.push_back(nextRow);
            endInsertRows();
            changed = true;
            continue;
        }

        if (_rows[static_cast<std::size_t>(targetIndex)].rowId != nextRow.rowId) {
            const auto matchingIt = std::ranges::find(_rows.begin() + targetIndex + 1, _rows.end(), nextRow.rowId, &Row::rowId);
            if (matchingIt == _rows.end()) {
                beginInsertRows({}, targetIndex, targetIndex);
                (void) _rows.insert(_rows.begin() + targetIndex, nextRow);
                endInsertRows();
                changed = true;
                continue;
            }

            if (const qsizetype sourceIndex = std::distance(_rows.begin(), matchingIt);
                !beginMoveRows({}, sourceIndex, sourceIndex, {}, targetIndex)) {
                qCWarning(lcActivityListModel) << "Incremental row move rejected; resetting activity projection"
                                               << "| sourceIndex:" << sourceIndex << "| targetIndex:" << targetIndex;
                beginResetModel();
                _rows = nextRows;
                endResetModel();
                return true;
            }
            auto movedRow = std::move(*matchingIt);
            (void) _rows.erase(matchingIt);
            (void) _rows.insert(_rows.begin() + targetIndex, std::move(movedRow));
            endMoveRows();
            changed = true;
        }
        changed = updateRow(targetIndex, nextRow) || changed;
    }
    return changed;
}

bool ActivityListModel::updateRow(const qsizetype rowIndex, const Row &nextRow) {
    auto &row = _rows[static_cast<std::size_t>(rowIndex)];
    if (row == nextRow) {
        return false;
    }

    QList<int> changedRoles;
    const auto addRoleIf = [&changedRoles](const bool changed, const Role role) {
        if (changed) {
            changedRoles.push_back(role);
        }
    };
    addRoleIf(row.name != nextRow.name, NameRole);
    addRoleIf(row.fileIconName != nextRow.fileIconName, FileIconNameRole);
    addRoleIf(row.actionText != nextRow.actionText, ActionTextRole);
    addRoleIf(row.folder != nextRow.folder, FolderRole);
    addRoleIf(row.timeText != nextRow.timeText, TimeTextRole);
    addRoleIf(row.sizeText != nextRow.sizeText, SizeTextRole);
    addRoleIf(row.nodeType != nextRow.nodeType, NodeTypeRole);
    addRoleIf(row.nodeType != nextRow.nodeType, IsDirectoryRole);
    addRoleIf(row.status != nextRow.status, StatusRole);
    addRoleIf(row.source != nextRow.source, SourceRole);
    addRoleIf(row.instruction != nextRow.instruction, InstructionRole);
    addRoleIf(row.progress != nextRow.progress, ProgressRole);
    addRoleIf(row.activeErrorDbIds.empty() != nextRow.activeErrorDbIds.empty(), HasActiveErrorRole);
    addRoleIf(row.activeErrorDbIds.size() != nextRow.activeErrorDbIds.size(), ActiveErrorCountRole);

    row = nextRow;
    if (!changedRoles.empty()) {
        emit dataChanged(index(rowIndex, 0), index(rowIndex, 0), changedRoles);
    }
    return true;
}

void ActivityListModel::refreshRelativeTimes() {
    if (_rows.empty()) {
        return;
    }
    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    for (qsizetype rowIndex = 0; rowIndex < static_cast<qsizetype>(_rows.size()); ++rowIndex) {
        auto &row = _rows[static_cast<std::size_t>(rowIndex)];
        const QString nextTimeText = formatRelativeTime(row.timestampUtc, nowUtc);
        if (row.timeText == nextTimeText) {
            continue;
        }
        row.timeText = nextTimeText;
        emit dataChanged(index(rowIndex, 0), index(rowIndex, 0), {TimeTextRole});
    }
}

} // namespace KDC
