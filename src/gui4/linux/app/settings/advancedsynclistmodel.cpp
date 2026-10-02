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

#include "advancedsynclistmodel.h"

#include <algorithm>
#include <unordered_set>

namespace KDC {

AdvancedSyncListModel::AdvancedSyncListModel(QObject *const parent) :
    QAbstractListModel(parent) {}

int AdvancedSyncListModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(_rows.size());
}

QVariant AdvancedSyncListModel::data(const QModelIndex &index, const int role) const {
    if (!index.isValid() || index.row() < 0 || static_cast<std::size_t>(index.row()) >= _rows.size()) {
        return {};
    }

    const Row &row = _rows[static_cast<std::size_t>(index.row())];
    switch (role) {
        case SyncDbIdRole:
            return QVariant::fromValue<qint64>(row.syncDbId);
        case LocalFolderNameRole:
            return row.localFolderName;
        case LocalPathRole:
            return row.localPath;
        case RemoteFolderNameRole:
            return row.remoteFolderName;
        case RemotePathRole:
            return row.remotePath;
        case CustomSelectionRole:
            return row.customSelection;
        case BlackListLoadingRole:
            return row.blackListState == BlackListState::Loading;
        case BlackListLoadFailedRole:
            return row.blackListState == BlackListState::Failed;
        case DeletePendingRole:
            return row.deletePending;
        default:
            return {};
    }
}

QHash<int, QByteArray> AdvancedSyncListModel::roleNames() const {
    return {{SyncDbIdRole, "syncDbId"},
            {LocalFolderNameRole, "localFolderName"},
            {LocalPathRole, "localPath"},
            {RemoteFolderNameRole, "remoteFolderName"},
            {RemotePathRole, "remotePath"},
            {CustomSelectionRole, "customSelection"},
            {BlackListLoadingRole, "blackListLoading"},
            {BlackListLoadFailedRole, "blackListLoadFailed"},
            {DeletePendingRole, "deletePending"}};
}

const AdvancedSyncListModel::Row *AdvancedSyncListModel::row(const SyncDbId syncDbId) const {
    const auto rowIndex = indexOf(syncDbId);
    return rowIndex ? &_rows[*rowIndex] : nullptr;
}

/**
 * Removes, updates, and inserts rows individually rather than resetting the model, so a card that stays in the list keeps
 * its delegate, and therefore its collapsed state and keyboard focus, when another synchronization is added or removed.
 */
std::vector<SyncDbId> AdvancedSyncListModel::replaceRows(std::vector<Row> rows) {
    std::unordered_set<SyncDbId> retainedIds;
    for (const auto &row: rows) {
        (void) retainedIds.insert(row.syncDbId);
    }

    // Walks backwards so a removal never shifts the rows still to visit.
    for (std::size_t rowIndex = _rows.size(); rowIndex-- > 0;) {
        if (retainedIds.contains(_rows[rowIndex].syncDbId)) {
            continue;
        }

        beginRemoveRows({}, toModelRow(rowIndex), toModelRow(rowIndex));
        (void) _rows.erase(_rows.begin() + static_cast<std::ptrdiff_t>(rowIndex));
        endRemoveRows();
    }

    std::vector<SyncDbId> addedIds;
    for (std::size_t position = 0; position < rows.size(); ++position) {
        Row &incoming = rows[position];
        const auto existingIndex = indexOf(incoming.syncDbId);
        if (!existingIndex) {
            beginInsertRows({}, toModelRow(position), toModelRow(position));
            (void) _rows.insert(_rows.begin() + static_cast<std::ptrdiff_t>(position), std::move(incoming));
            endInsertRows();
            addedIds.push_back(_rows[position].syncDbId);
            continue;
        }

        if (*existingIndex != position) {
            // Earlier positions are already final, so an existing row only ever moves up: the move is always valid.
            [[maybe_unused]] const bool moveAccepted =
                    beginMoveRows({}, toModelRow(*existingIndex), toModelRow(*existingIndex), {}, toModelRow(position));
            Q_ASSERT(moveAccepted);
            Row moved = std::move(_rows[*existingIndex]);
            (void) _rows.erase(_rows.begin() + static_cast<std::ptrdiff_t>(*existingIndex));
            (void) _rows.insert(_rows.begin() + static_cast<std::ptrdiff_t>(position), std::move(moved));
            endMoveRows();
        }

        Row &current = _rows[position];
        incoming.blackListState = current.blackListState;
        incoming.customSelection = current.customSelection;
        incoming.deletePending = current.deletePending;
        if (current.localFolderName != incoming.localFolderName || current.localPath != incoming.localPath ||
            current.remoteFolderName != incoming.remoteFolderName || current.remotePath != incoming.remotePath) {
            current = std::move(incoming);
            notifyRow(position, {LocalFolderNameRole, LocalPathRole, RemoteFolderNameRole, RemotePathRole});
        }
    }

    return addedIds;
}

void AdvancedSyncListModel::setBlackListState(const SyncDbId syncDbId, const BlackListState state) {
    const auto rowIndex = indexOf(syncDbId);
    if (!rowIndex || _rows[*rowIndex].blackListState == state) {
        return;
    }

    _rows[*rowIndex].blackListState = state;
    notifyRow(*rowIndex, {BlackListLoadingRole, BlackListLoadFailedRole});
}

void AdvancedSyncListModel::setCustomSelection(const SyncDbId syncDbId, const bool customSelection) {
    const auto rowIndex = indexOf(syncDbId);
    if (!rowIndex || _rows[*rowIndex].customSelection == customSelection) {
        return;
    }

    _rows[*rowIndex].customSelection = customSelection;
    notifyRow(*rowIndex, {CustomSelectionRole});
}

void AdvancedSyncListModel::setDeletePending(const SyncDbId syncDbId, const bool deletePending) {
    const auto rowIndex = indexOf(syncDbId);
    if (!rowIndex || _rows[*rowIndex].deletePending == deletePending) {
        return;
    }

    _rows[*rowIndex].deletePending = deletePending;
    notifyRow(*rowIndex, {DeletePendingRole});
}

std::optional<std::size_t> AdvancedSyncListModel::indexOf(const SyncDbId syncDbId) const {
    const auto it = std::ranges::find(_rows, syncDbId, &Row::syncDbId);
    if (it == _rows.end()) {
        return std::nullopt;
    }

    return static_cast<std::size_t>(std::distance(_rows.begin(), it));
}

// Qt addresses rows with an int; a settings list never comes close to its range.
int32_t AdvancedSyncListModel::toModelRow(const std::size_t rowIndex) {
    return static_cast<int32_t>(rowIndex);
}

void AdvancedSyncListModel::notifyRow(const std::size_t rowIndex, const QList<int> &roles) {
    const QModelIndex modelIndex = index(toModelRow(rowIndex));
    emit dataChanged(modelIndex, modelIndex, roles);
}

} // namespace KDC
