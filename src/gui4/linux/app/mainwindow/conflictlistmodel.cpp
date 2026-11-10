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

#include "app/mainwindow/conflictlistmodel.h"

#include "app/cache/mainselectionstore.h"
#include "app/cache/syncerrorsprojection.h"
#include "app/services/syncerrorservice.h"


#include <algorithm>
#include <iterator>

using namespace Qt::StringLiterals;

namespace KDC {

namespace {

QString conflictRowId(const ErrorDbId errorDbId) {
    return u"conflict:%1"_s.arg(static_cast<qlonglong>(errorDbId));
}

} // namespace

ConflictListModel::ConflictListModel(const SyncErrorsProjection &projection, const MainSelectionStore &selectionStore,
                                     const SyncErrorService &syncErrorService, QObject *const parent) :
    QAbstractListModel(parent),
    _projection(projection),
    _selectionStore(selectionStore),
    _syncErrorService(syncErrorService) {
    _collator.setCaseSensitivity(Qt::CaseInsensitive);
    _collator.setNumericMode(true);
    (void) connect(&_projection, &SyncErrorsProjection::currentErrorsChanged, this, &ConflictListModel::refresh);
    (void) connect(&_selectionStore, &MainSelectionStore::currentSyncDbIdChanged, this, &ConflictListModel::reset);
    (void) connect(&_syncErrorService, &SyncErrorService::conflictResolutionPendingChanged, this, [this](const qint64 syncDbId) {
        if (syncDbId == _selectionStore.currentSyncDbId()) {
            refresh();
        }
    });
    _rows = buildRows(true);
}

int ConflictListModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(_rows.size());
}

QVariant ConflictListModel::data(const QModelIndex &index, const int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(_rows.size())) {
        return {};
    }

    const auto &row = _rows[static_cast<std::size_t>(index.row())];
    switch (role) {
        case RowIdRole:
            return row.rowId;
        case NameRole:
            return row.name;
        case PathRole:
            return row.path;
        case IconNameRole:
            return row.iconName;
        case IsDirectoryRole:
            return row.isDirectory;
        case ActionPendingRole:
            return row.actionPending;
        default:
            return {};
    }
}

QHash<int, QByteArray> ConflictListModel::roleNames() const {
    return {
            {RowIdRole, "rowId"},
            {NameRole, "name"},
            {PathRole, "path"},
            {IconNameRole, "iconName"},
            {IsDirectoryRole, "isDirectory"},
            {ActionPendingRole, "actionPending"},
    };
}

void ConflictListModel::setSearchText(const QString &searchText) {
    if (_searchText == searchText) {
        return;
    }
    _searchText = searchText;
    emit searchTextChanged();
    reset();
}

std::vector<ErrorDbId> ConflictListModel::errorDbIds() const {
    std::vector<ErrorDbId> errorDbIds;
    for (const auto &row: buildRows(false)) {
        errorDbIds.push_back(row.errorDbId);
    }
    return errorDbIds;
}

std::optional<ErrorDbId> ConflictListModel::errorDbId(const QString &rowId) const {
    const auto rowIt = std::ranges::find(_rows, rowId, &Row::rowId);
    if (rowIt == _rows.end()) {
        return std::nullopt;
    }
    return rowIt->errorDbId;
}

std::vector<ConflictListModel::Row> ConflictListModel::buildRows(const bool filtered) const {
    const auto search = _searchText.trimmed();
    const bool actionPending = _syncErrorService.isConflictResolutionPending(_selectionStore.currentSyncDbId());
    std::vector<Row> rows;
    for (const auto &classified: _projection.currentErrors()) {
        if (classified.category != SyncErrorCategory::Conflicts) {
            continue;
        }
        const auto &error = classified.error;
        Row row;
        row.path = Path2QStr(error.path().relative_path().lexically_normal());
        if (filtered && !search.isEmpty() && !row.path.contains(search, Qt::CaseInsensitive)) {
            continue;
        }
        row.rowId = conflictRowId(error.dbId());
        row.errorDbId = error.dbId();
        row.name = Path2QStr(error.path().filename());
        row.isDirectory = error.nodeType() == NodeType::Directory;
        row.iconName = _fileIconResolver.iconName(row.name, error.nodeType());
        row.actionPending = actionPending;
        rows.push_back(std::move(row));
    }
    (void) std::ranges::sort(rows, [this](const Row &lhs, const Row &rhs) { return isBefore(lhs, rhs); });
    return rows;
}

// By path regardless of case, then by database id: two conflicts never compare equal.
bool ConflictListModel::isBefore(const Row &lhs, const Row &rhs) const {
    if (const auto order = _collator.compare(lhs.path, rhs.path); order != 0) {
        return order < 0;
    }
    return lhs.errorDbId < rhs.errorDbId;
}

// Another synchronization or another search: the rows are replaced wholesale.
void ConflictListModel::reset() {
    const auto previousCount = count();
    beginResetModel();
    _fileIconResolver.clear();
    _rows = buildRows(true);
    endResetModel();
    if (count() != previousCount) {
        emit countChanged();
    }
}

// Applies the next rows in place. Both lists are sorted the same way, so a single merge pass removes, inserts and updates
// rows, one range per run of consecutive removed or inserted rows.
void ConflictListModel::refresh() {
    const auto previousCount = count();
    auto nextRows = buildRows(true);
    std::size_t rowIndex = 0;
    std::size_t nextIndex = 0;
    while (rowIndex < _rows.size() || nextIndex < nextRows.size()) {
        if (nextIndex < nextRows.size() && rowIndex < _rows.size() &&
            _rows[rowIndex].errorDbId == nextRows[nextIndex].errorDbId) {
            updateRow(static_cast<int32_t>(rowIndex), std::move(nextRows[nextIndex]));
            ++rowIndex;
            ++nextIndex;
            continue;
        }

        auto removedEnd = rowIndex;
        while (removedEnd < _rows.size() && (nextIndex == nextRows.size() || isBefore(_rows[removedEnd], nextRows[nextIndex]))) {
            ++removedEnd;
        }
        if (removedEnd > rowIndex) {
            beginRemoveRows({}, static_cast<int>(rowIndex), static_cast<int>(removedEnd) - 1);
            (void) _rows.erase(_rows.begin() + static_cast<std::ptrdiff_t>(rowIndex),
                               _rows.begin() + static_cast<std::ptrdiff_t>(removedEnd));
            endRemoveRows();
            continue;
        }

        auto insertedEnd = nextIndex;
        while (insertedEnd < nextRows.size() && (rowIndex == _rows.size() || isBefore(nextRows[insertedEnd], _rows[rowIndex]))) {
            ++insertedEnd;
        }
        beginInsertRows({}, static_cast<int>(rowIndex), static_cast<int>(rowIndex + insertedEnd - nextIndex) - 1);
        (void) _rows.insert(_rows.begin() + static_cast<std::ptrdiff_t>(rowIndex),
                            std::make_move_iterator(nextRows.begin() + static_cast<std::ptrdiff_t>(nextIndex)),
                            std::make_move_iterator(nextRows.begin() + static_cast<std::ptrdiff_t>(insertedEnd)));
        endInsertRows();
        rowIndex += insertedEnd - nextIndex;
        nextIndex = insertedEnd;
    }
    if (count() != previousCount) {
        emit countChanged();
    }
}

void ConflictListModel::updateRow(const int32_t rowIndex, Row nextRow) {
    auto &row = _rows[static_cast<std::size_t>(rowIndex)];
    if (row == nextRow) {
        return;
    }

    QList<int> changedRoles;
    const auto addRoleIf = [&changedRoles](const bool changed, const Role role) {
        if (changed) {
            changedRoles.push_back(role);
        }
    };
    addRoleIf(row.name != nextRow.name, NameRole);
    addRoleIf(row.path != nextRow.path, PathRole);
    addRoleIf(row.iconName != nextRow.iconName, IconNameRole);
    addRoleIf(row.isDirectory != nextRow.isDirectory, IsDirectoryRole);
    addRoleIf(row.actionPending != nextRow.actionPending, ActionPendingRole);

    row = std::move(nextRow);
    if (!changedRoles.empty()) {
        emit dataChanged(index(rowIndex, 0), index(rowIndex, 0), changedRoles);
    }
}

} // namespace KDC
