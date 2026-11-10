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
#include "app/errors/inotifywatchlimit.h"
#include "app/errors/syncerrorcatalog.h"
#include "app/services/syncerrorservice.h"
#include "app/services/syncservice.h"
#include "app/syncconfiguration/localpaths.h"
#include "libcommon/utility/types.h"
#include "version.h"

#include <QDateTime>
#include <QGuiApplication>
#include <QStringList>
#include <QSysInfo>
#include <QVariantList>
#include <QVariantMap>

#include <algorithm>
#include <iterator>
#include <tuple>

namespace KDC {

using namespace Qt::StringLiterals;

namespace {

constexpr auto conflictsRowId = "conflicts"_L1;

QString errorRowId(const ErrorDbId errorDbId) {
    return u"error:%1"_s.arg(static_cast<qlonglong>(errorDbId));
}

QString textId(const char *const id) {
    return id ? QString::fromLatin1(id) : QString{};
}

bool isActionShown(const SyncErrorAction action) {
    switch (action) {
        case SyncErrorAction::None:
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

// Distribution and graphical session, such as "Ubuntu 26.04 LTS, Wayland": the behavior of the app depends on both.
QString systemDescription() {
    QString session = QGuiApplication::platformName();
    if (session == u"wayland"_s) {
        session = u"Wayland"_s;
    } else if (session == u"xcb"_s) {
        session = u"X11"_s;
    }
    return u"%1, %2"_s.arg(QSysInfo::prettyProductName(), session);
}

// Technical details of an unexpected error, sent to the support: where the sync engine raised it, the item paths, and
// the versions of the app and the system. Empty, `Unknown` and `None` values are skipped. Paths are relative to the
// synchronization root, so the copied text does not reveal the rest of the computer. Labels are not translated: they
// name fields for the support.
std::vector<std::pair<QString, QString>> technicalDetails(const Error &error, const std::optional<SyncContext> &context) {
    std::vector<std::pair<QString, QString>> details;
    const auto add = [&details](const QString &label, const QString &value) {
        if (!value.isEmpty() && value != u"Unknown"_s && value != u"None"_s) {
            (void) details.emplace_back(label, value);
        }
    };
    const SyncPath syncRoot = context.has_value() ? context->syncInfo.localPath().lexically_normal() : SyncPath{};
    const auto pathBelowRoot = [&syncRoot](const SyncPath &path) {
        if (path.empty() || syncRoot.empty()) {
            return Path2QStr(path.lexically_normal());
        }
        return Path2QStr(absoluteSyncPath(syncRoot, path).lexically_relative(syncRoot));
    };

    add(u"ID"_s, QString::number(error.dbId()));
    if (error.time() > 0) {
        add(u"Timestamp"_s, QDateTime::fromSecsSinceEpoch(error.time()).toString(Qt::ISODate));
    }
    add(u"Level"_s, QString::fromStdString(toString(error.level())));
    add(u"Exit code"_s, QString::fromStdString(toString(error.exitCode())));
    add(u"Exit cause"_s, QString::fromStdString(toString(error.exitCause())));
    add(u"Auto-resolved"_s, error.isAutoResolved() ? u"true"_s : u"false"_s);
    add(u"Function"_s, QString::fromStdString(error.functionName()));
    add(u"Worker"_s, QString::fromStdString(error.workerName()));
    add(u"Sync DB ID"_s, QString::number(error.syncDbId()));
    // Without synchronization context, the drive is unknown; a context whose drive id is 0 is shown, as an anomaly.
    if (context.has_value()) {
        add(u"Drive ID"_s, QString::number(context->drive.driveId()));
    }
    add(u"Path"_s, pathBelowRoot(error.path()));
    add(u"Destination path"_s, pathBelowRoot(error.destinationPath()));
    add(u"Node type"_s, QString::fromStdString(toString(error.nodeType())));
    add(u"Conflict type"_s, QString::fromStdString(toString(error.conflictType())));
    add(u"Inconsistency type"_s, QString::fromStdString(toString(error.inconsistencyType())));
    add(u"Cancel type"_s, QString::fromStdString(toString(error.cancelType())));
    add(u"Local node ID"_s, QString::fromStdString(error.localNodeId()));
    add(u"Remote node ID"_s, QString::fromStdString(error.remoteNodeId()));
    add(u"App version"_s, QString::fromLatin1(KDRIVE_VERSION_STRING));
    add(u"System"_s, systemDescription());
    return details;
}

QVariantList technicalDetailsVariant(const std::vector<std::pair<QString, QString>> &details) {
    QVariantList list;
    list.reserve(static_cast<qsizetype>(details.size()));
    for (const auto &[label, value]: details) {
        list.append(QVariantMap{{u"label"_s, label}, {u"value"_s, value}});
    }
    return list;
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
    (void) connect(&_syncErrorService, &SyncErrorService::conflictResolutionPendingChanged, this, refreshPendingRows);
    _rows = buildRows();
}

QString ErrorListModel::technicalDetailsText(const QString &rowId) const {
    const auto rowIt = std::ranges::find(_rows, rowId, &Row::rowId);
    if (rowIt == _rows.end()) {
        return {};
    }

    QStringList lines;
    for (const auto &[label, value]: rowIt->technicalDetails) {
        lines.append(u"%1: %2"_s.arg(label, value));
    }
    return lines.join(u'\n');
}

bool ErrorListModel::isConflictsRow(const QString &rowId) const {
    return rowId == conflictsRowId;
}

int32_t ErrorListModel::firstRowOf(const std::vector<ErrorDbId> &errorDbIds) const {
    for (int32_t rowIndex = 0; rowIndex < static_cast<int32_t>(_rows.size()); ++rowIndex) {
        const auto &row = _rows[static_cast<std::size_t>(rowIndex)];
        const auto standsFor = [&row](const ErrorDbId errorDbId) {
            return row.rowId == conflictsRowId ? std::ranges::find(row.conflictDbIds, errorDbId) != row.conflictDbIds.end()
                                               : row.errorDbId == errorDbId;
        };
        if (std::ranges::any_of(errorDbIds, standsFor)) {
            return rowIndex;
        }
    }
    return -1;
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
        case DescriptionCountRole:
            return row.descriptionCount;
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
        case VariantRole:
            return QVariant::fromValue(row.variant);
        case CopyablePathRole:
            return row.copyablePath;
        case TechnicalDetailsRole:
            return technicalDetailsVariant(row.technicalDetails);
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
            {DescriptionCountRole, "descriptionCount"},
            {NodeLabelIdRole, "nodeLabelId"},
            {ItemNameRole, "itemName"},
            {ItemPathRole, "itemPath"},
            {ItemIconNameRole, "itemIconName"},
            {IsDirectoryRole, "isDirectory"},
            {VariantRole, "variant"},
            {CopyablePathRole, "copyablePath"},
            {TechnicalDetailsRole, "technicalDetails"},
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
    std::vector<ErrorDbId> conflictDbIds;
    for (const auto &classified: errors) {
        if (classified.category == SyncErrorCategory::Conflicts) {
            conflictDbIds.push_back(classified.error.dbId());
        }
    }
    const bool aggregateConflicts = std::ssize(conflictDbIds) > kMaxIndividualConflicts;

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
        rows.push_back(makeConflictsRow(std::move(conflictDbIds)));
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
    if (classified.kind == SyncErrorKind::NotEnoughINotifyWatches) {
        // The limit is read again with each refresh: it can be raised while the error is listed. Unreadable, the row
        // keeps the description without a number.
        row.descriptionCount = readWatchLimit();
        if (row.descriptionCount == 0) {
            row.descriptionId = u"linuxErrINotifyWatchesDescription"_s;
        }
    }
    row.nodeLabelId = textId(syncErrorNodeLabelId(error.nodeType()));
    row.variant = variant;

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

    // Shown in its `~` form; the controller copies the absolute path.
    if (variant == SyncErrorRowVariant::CopyablePath && !error.destinationPath().empty() && !syncRoot.empty()) {
        row.copyablePath = displayLocalPath(Path2QStr(absoluteSyncPath(syncRoot, error.destinationPath())));
    }
    if (variant == SyncErrorRowVariant::Technical) {
        row.technicalDetails = technicalDetails(error, context);
    }

    if (isActionShown(presentation.action)) {
        row.actionLabelId = textId(presentation.actionLabel.id);
        row.actionLabelArgCount = presentation.actionLabel.nodeLabelArgs;
        row.actionExternal = opensBrowser(presentation.action, error);
        row.actionPending = isActionPending(presentation.action);
    }
    return row;
}

// Its "Manage" button opens the version conflicts pages.
ErrorListModel::Row ErrorListModel::makeConflictsRow(std::vector<ErrorDbId> conflictDbIds) const {
    Row row;
    row.rowId = conflictsRowId;
    row.section = SyncErrorCategory::Conflicts;
    row.titleId = u"manyConflictErrorTitle"_s;
    row.titleCount = static_cast<int32_t>(conflictDbIds.size());
    row.conflictDbIds = std::move(conflictDbIds);
    row.descriptionId = u"manyConflictErrorDescription"_s;
    row.actionLabelId = u"buttonManage"_s;
    row.actionPending = _syncErrorService.isConflictResolutionPending(_selectionStore.currentSyncDbId());
    return row;
}

bool ErrorListModel::isActionPending(const SyncErrorAction action) const {
    const auto syncDbId = _selectionStore.currentSyncDbId();
    switch (action) {
        case SyncErrorAction::RestartSync:
            return _syncService.isStartSyncPending(syncDbId);
        case SyncErrorAction::RefreshErrors:
            return _syncErrorService.isRefreshPending(syncDbId);
        case SyncErrorAction::ChooseVersion:
            return _syncErrorService.isConflictResolutionPending(syncDbId);
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
    addRoleIf(row.descriptionCount != nextRow.descriptionCount, DescriptionCountRole);
    addRoleIf(row.nodeLabelId != nextRow.nodeLabelId, NodeLabelIdRole);
    addRoleIf(row.itemName != nextRow.itemName, ItemNameRole);
    addRoleIf(row.itemPath != nextRow.itemPath, ItemPathRole);
    addRoleIf(row.itemIconName != nextRow.itemIconName, ItemIconNameRole);
    addRoleIf(row.isDirectory != nextRow.isDirectory, IsDirectoryRole);
    addRoleIf(row.variant != nextRow.variant, VariantRole);
    addRoleIf(row.copyablePath != nextRow.copyablePath, CopyablePathRole);
    addRoleIf(row.technicalDetails != nextRow.technicalDetails, TechnicalDetailsRole);
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
