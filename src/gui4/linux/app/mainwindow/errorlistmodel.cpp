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

#include "app/mainwindow/errorlistmodel.h"

#include "app/cache/mainselectionstore.h"
#include "app/cache/syncerrorsprojection.h"
#include "app/errors/syncerrorcatalog.h"
#include "app/services/syncerrorservice.h"
#include "app/services/syncservice.h"
#include "app/syncconfiguration/localpaths.h"
#include "libcommon/utility/types.h"

#include <algorithm>
#include <iterator>
#include <tuple>

namespace KDC {

using namespace Qt::StringLiterals;

namespace {

const auto conflictsRowId = u"conflicts"_s;

QString errorRowId(const ErrorDbId errorDbId) {
    return u"error:%1"_s.arg(static_cast<qlonglong>(errorDbId));
}

QString textId(const char *const id) {
    return id ? QString::fromLatin1(id) : QString{};
}

// Actions whose effect lands in the next PRs: their rows are shown without a button until then.
bool isActionShown(const SyncErrorAction action) {
    switch (action) {
        case SyncErrorAction::None:
        case SyncErrorAction::ChooseVersion:
        case SyncErrorAction::Explain:
        case SyncErrorAction::EnumEnd:
            return false;
        default:
            return true;
    }
}

// Whether the action opens the web browser. A rename opens the item online only when the server knows it.
bool opensBrowser(const SyncErrorAction action, const Error &error) {
    switch (action) {
        case SyncErrorAction::Rename:
            return !error.remoteNodeId().empty();
        case SyncErrorAction::ManageStorage:
        case SyncErrorAction::RenewSubscription:
        case SyncErrorAction::WakeUp:
        case SyncErrorAction::ContactSupport:
            return true;
        default:
            return false;
    }
}

// Rows are sorted by section, then by database id: an error the server detects again keeps its place.
bool isBefore(const auto &lhs, const auto &rhs) {
    return std::tie(lhs.section, lhs.errorDbId) < std::tie(rhs.section, rhs.errorDbId);
}

} // namespace

ErrorListModel::ErrorListModel(const SyncErrorsProjection &projection, const MainSelectionStore &selectionStore,
                               const SyncService &syncService, const SyncErrorService &syncErrorService, QObject *const parent) :
    QAbstractListModel(parent),
    _projection(projection),
    _selectionStore(selectionStore),
    _syncService(syncService),
    _syncErrorService(syncErrorService) {
    (void) connect(&_projection, &SyncErrorsProjection::currentErrorsChanged, this, &ErrorListModel::refresh);
    // The admin rights and the synchronization root change the texts, actions and item chips of some rows.
    (void) connect(&_selectionStore, &MainSelectionStore::currentContextChanged, this, &ErrorListModel::refresh);
    (void) connect(&_selectionStore, &MainSelectionStore::currentSyncDbIdChanged, this, &ErrorListModel::reset);
    const auto refreshPendingRows = [this](const qint64 syncDbId) {
        if (syncDbId == _selectionStore.currentSyncDbId()) {
            refresh();
        }
    };
    (void) connect(&_syncService, &SyncService::syncActionPendingChanged, this, refreshPendingRows);
    (void) connect(&_syncErrorService, &SyncErrorService::refreshPendingChanged, this, refreshPendingRows);
    _rows = buildRows();
}

int ErrorListModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(_rows.size());
}

QVariant ErrorListModel::data(const QModelIndex &index, const int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(_rows.size())) {
        return {};
    }

    const auto &row = _rows[static_cast<std::size_t>(index.row())];
    switch (role) {
        case RowIdRole:
            return row.rowId;
        case SectionTitleIdRole:
            return textId(syncErrorCategoryTitleId(row.section));
        case TitleIdRole:
            return row.titleId;
        // Counts are stored on one byte: as numbers for QML, not as characters.
        case TitleArgCountRole:
            return static_cast<int32_t>(row.titleArgCount);
        case TitleCountRole:
            return row.titleCount;
        case DescriptionIdRole:
            return row.descriptionId;
        case DescriptionArgCountRole:
            return static_cast<int32_t>(row.descriptionArgCount);
        case NodeLabelIdRole:
            return row.nodeLabelId;
        case ItemNameRole:
            return row.itemName;
        case ItemPathRole:
            return row.itemPath;
        case ItemIconNameRole:
            return row.itemIconName;
        case IsDirectoryRole:
            return row.isDirectory;
        case ActionLabelIdRole:
            return row.actionLabelId;
        case ActionLabelArgCountRole:
            return static_cast<int32_t>(row.actionLabelArgCount);
        case ActionExternalRole:
            return row.actionExternal;
        case ActionPendingRole:
            return row.actionPending;
        default:
            return {};
    }
}

QHash<int, QByteArray> ErrorListModel::roleNames() const {
    return {
            {RowIdRole, "rowId"},
            {SectionTitleIdRole, "sectionTitleId"},
            {TitleIdRole, "titleId"},
            {TitleArgCountRole, "titleArgCount"},
            {TitleCountRole, "titleCount"},
            {DescriptionIdRole, "descriptionId"},
            {DescriptionArgCountRole, "descriptionArgCount"},
            {NodeLabelIdRole, "nodeLabelId"},
            {ItemNameRole, "itemName"},
            {ItemPathRole, "itemPath"},
            {ItemIconNameRole, "itemIconName"},
            {IsDirectoryRole, "isDirectory"},
            {ActionLabelIdRole, "actionLabelId"},
            {ActionLabelArgCountRole, "actionLabelArgCount"},
            {ActionExternalRole, "actionExternal"},
            {ActionPendingRole, "actionPending"},
    };
}

std::optional<ErrorDbId> ErrorListModel::errorDbId(const QString &rowId) const {
    const auto rowIt = std::ranges::find(_rows, rowId, &Row::rowId);
    if (rowIt == _rows.end() || rowIt->rowId == conflictsRowId) {
        return std::nullopt;
    }
    return rowIt->errorDbId;
}

// Another synchronization: its rows replace the previous ones wholesale.
void ErrorListModel::reset() {
    beginResetModel();
    _fileIconResolver.clear();
    _rows = buildRows();
    endResetModel();
}

// Applies the next rows in place. Both lists are sorted the same way, so a single merge pass removes, inserts and updates
// rows. Consecutive removed or inserted rows go in one range each: a large change moves the rows once per range, not once
// per row. A row whose section changed is removed, then inserted at its new place.
void ErrorListModel::refresh() {
    auto nextRows = buildRows();
    const auto removeRange = [this](const std::size_t first, const std::size_t last) {
        beginRemoveRows({}, static_cast<int>(first), static_cast<int>(last) - 1);
        (void) _rows.erase(_rows.begin() + static_cast<std::ptrdiff_t>(first), _rows.begin() + static_cast<std::ptrdiff_t>(last));
        endRemoveRows();
    };

    std::size_t rowIndex = 0;
    std::size_t nextIndex = 0;
    while (nextIndex < nextRows.size()) {
        auto removedEnd = rowIndex;
        while (removedEnd < _rows.size() && isBefore(_rows[removedEnd], nextRows[nextIndex])) {
            ++removedEnd;
        }
        if (removedEnd > rowIndex) {
            removeRange(rowIndex, removedEnd);
        }

        // Same section and database id: the same row.
        if (rowIndex < _rows.size() && !isBefore(nextRows[nextIndex], _rows[rowIndex])) {
            updateRow(static_cast<int32_t>(rowIndex), std::move(nextRows[nextIndex]));
            ++rowIndex;
            ++nextIndex;
            continue;
        }

        auto insertedEnd = nextIndex + 1;
        while (insertedEnd < nextRows.size() && (rowIndex == _rows.size() || isBefore(nextRows[insertedEnd], _rows[rowIndex]))) {
            ++insertedEnd;
        }
        const auto insertedCount = insertedEnd - nextIndex;
        beginInsertRows({}, static_cast<int>(rowIndex), static_cast<int>(rowIndex + insertedCount) - 1);
        (void) _rows.insert(_rows.begin() + static_cast<std::ptrdiff_t>(rowIndex),
                            std::make_move_iterator(nextRows.begin() + static_cast<std::ptrdiff_t>(nextIndex)),
                            std::make_move_iterator(nextRows.begin() + static_cast<std::ptrdiff_t>(insertedEnd)));
        endInsertRows();
        rowIndex += insertedCount;
        nextIndex = insertedEnd;
    }

    if (rowIndex < _rows.size()) {
        removeRange(rowIndex, _rows.size());
    }
}

std::vector<ErrorListModel::Row> ErrorListModel::buildRows() const {
    const auto &errors = _projection.currentErrors();
    const auto context = _selectionStore.currentSyncContext();
    const auto conflictCount =
            static_cast<int32_t>(std::ranges::count(errors, SyncErrorCategory::Conflicts, &ClassifiedSyncError::category));
    const bool aggregateConflicts = conflictCount > kMaxIndividualConflicts;

    std::vector<Row> rows;
    rows.reserve(errors.size() + 1);
    for (const auto &classified: errors) {
        auto section = classified.category;
        if (section == SyncErrorCategory::Conflicts) {
            if (aggregateConflicts) {
                continue;
            }
            section = SyncErrorCategory::FilesToCheck;
        }
        rows.push_back(makeErrorRow(classified, section, context));
    }
    if (aggregateConflicts) {
        rows.push_back(makeConflictsRow(conflictCount));
    }

    (void) std::ranges::sort(rows, [](const Row &lhs, const Row &rhs) { return isBefore(lhs, rhs); });
    return rows;
}

ErrorListModel::Row ErrorListModel::makeErrorRow(const ClassifiedSyncError &classified, const SyncErrorCategory section,
                                                 const std::optional<SyncContext> &context) const {
    const auto &error = classified.error;
    const bool isAdmin = context.has_value() && context->drive.admin();
    const auto presentation = describeSyncError(classified.kind, isAdmin);
    const auto variant = syncErrorTraits(classified.kind).variant;

    Row row;
    row.rowId = errorRowId(error.dbId());
    row.section = section;
    row.errorDbId = error.dbId();
    row.titleId = textId(presentation.title.id);
    row.titleArgCount = presentation.title.nodeLabelArgs;
    row.descriptionId = textId(presentation.description.id);
    row.descriptionArgCount = presentation.description.nodeLabelArgs;
    row.nodeLabelId = textId(syncErrorNodeLabelId(error.nodeType()));

    const SyncPath syncRoot = context.has_value() ? context->syncInfo.localPath() : SyncPath{};
    // The chip shows the name; its tooltip, the path within the synchronization, or the local path of its root.
    if (variant == SyncErrorRowVariant::SyncRoot) {
        row.itemName = Path2QStr(syncRoot.filename());
        row.itemPath = displayLocalPath(Path2QStr(syncRoot));
        row.isDirectory = true;
    } else {
        row.itemName = Path2QStr(error.path().filename());
        row.itemPath = Path2QStr(error.path().relative_path().lexically_normal());
        row.isDirectory = error.nodeType() == NodeType::Directory;
    }
    if (!row.itemName.isEmpty()) {
        row.itemIconName = _fileIconResolver.iconName(row.itemName, row.isDirectory ? NodeType::Directory : error.nodeType());
    }

    if (isActionShown(presentation.action)) {
        row.actionLabelId = textId(presentation.actionLabel.id);
        row.actionLabelArgCount = presentation.actionLabel.nodeLabelArgs;
        row.actionExternal = opensBrowser(presentation.action, error);
        row.actionPending = isActionPending(presentation.action);
    }
    return row;
}

// The "Manage" button of this row opens the version conflict pages, added with them.
ErrorListModel::Row ErrorListModel::makeConflictsRow(const int32_t conflictCount) {
    Row row;
    row.rowId = conflictsRowId;
    row.section = SyncErrorCategory::Conflicts;
    row.titleId = u"manyConflictErrorTitle"_s;
    row.titleCount = conflictCount;
    row.descriptionId = u"manyConflictErrorDescription"_s;
    return row;
}

bool ErrorListModel::isActionPending(const SyncErrorAction action) const {
    const auto syncDbId = _selectionStore.currentSyncDbId();
    switch (action) {
        case SyncErrorAction::RestartSync:
            return _syncService.isStartSyncPending(syncDbId);
        case SyncErrorAction::RefreshErrors:
            return _syncErrorService.isRefreshPending(syncDbId);
        default:
            return false;
    }
}

void ErrorListModel::updateRow(const int32_t rowIndex, Row nextRow) {
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
    addRoleIf(row.titleId != nextRow.titleId, TitleIdRole);
    addRoleIf(row.titleArgCount != nextRow.titleArgCount, TitleArgCountRole);
    addRoleIf(row.titleCount != nextRow.titleCount, TitleCountRole);
    addRoleIf(row.descriptionId != nextRow.descriptionId, DescriptionIdRole);
    addRoleIf(row.descriptionArgCount != nextRow.descriptionArgCount, DescriptionArgCountRole);
    addRoleIf(row.nodeLabelId != nextRow.nodeLabelId, NodeLabelIdRole);
    addRoleIf(row.itemName != nextRow.itemName, ItemNameRole);
    addRoleIf(row.itemPath != nextRow.itemPath, ItemPathRole);
    addRoleIf(row.itemIconName != nextRow.itemIconName, ItemIconNameRole);
    addRoleIf(row.isDirectory != nextRow.isDirectory, IsDirectoryRole);
    addRoleIf(row.actionLabelId != nextRow.actionLabelId, ActionLabelIdRole);
    addRoleIf(row.actionLabelArgCount != nextRow.actionLabelArgCount, ActionLabelArgCountRole);
    addRoleIf(row.actionExternal != nextRow.actionExternal, ActionExternalRole);
    addRoleIf(row.actionPending != nextRow.actionPending, ActionPendingRole);

    row = std::move(nextRow);
    if (!changedRoles.empty()) {
        emit dataChanged(index(rowIndex, 0), index(rowIndex, 0), changedRoles);
    }
}

} // namespace KDC
