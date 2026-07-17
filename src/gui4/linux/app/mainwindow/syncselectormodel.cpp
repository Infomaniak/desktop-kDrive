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

#include "app/mainwindow/syncselectormodel.h"

#include "app/appconstants.h"
#include "libcommon/utility/types.h"

#include <algorithm>
#include <cstddef>

namespace KDC {

namespace {

QColor driveColor(const Drive &drive) {
    const QColor color{QString::fromStdString(drive.color())};
    return color.isValid() ? color : AppConstants::Drive::defaultColor();
}

bool driveHasWarning(const Drive &drive) {
    return drive.maintenanceInfo().inMaintenance() || drive.locked() || drive.accessDenied();
}

QString localFolderName(const BaseSync &syncInfo) {
    const QString folderName = Path2QStr(syncInfo.localPath().filename());
    return folderName.isEmpty() ? Path2QStr(syncInfo.localPath()) : folderName;
}

} // namespace

SyncSelectorModel::SyncSelectorModel(const AppCache &cache, MainSelectionStore &selectionStore, QObject *const parent) :
    QAbstractListModel(parent),
    _cache(cache),
    _selectionStore(selectionStore) {
    (void) connect(&_cache, &AppCache::usersChanged, this, &SyncSelectorModel::rebuild);
    (void) connect(&_cache, &AppCache::accountsChanged, this, &SyncSelectorModel::rebuild);
    (void) connect(&_cache, &AppCache::drivesChanged, this, &SyncSelectorModel::rebuild);
    (void) connect(&_cache, &AppCache::syncsChanged, this, &SyncSelectorModel::rebuild);
    (void) connect(&_cache, &AppCache::syncErrorsChanged, this, &SyncSelectorModel::rebuild);
    (void) connect(&_selectionStore, &MainSelectionStore::currentSyncDbIdChanged, this,
                   &SyncSelectorModel::handleSelectionChanged);

    rebuild();
}

int SyncSelectorModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(_entries.size());
}

QVariant SyncSelectorModel::data(const QModelIndex &index, const int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    const auto &entry = _entries[static_cast<std::size_t>(index.row())];
    switch (role) {
        case EntryTypeRole:
            return QVariant::fromValue(entry.type);
        case SyncDbIdRole:
            return static_cast<qint64>(entry.syncDbId);
        case TitleRole:
        case Qt::DisplayRole:
            return entry.title;
        case SubtitleRole:
            return entry.subtitle;
        case DriveColorRole:
            return entry.driveColor;
        case ErrorCountRole:
            return entry.errorCount;
        case WarningRole:
            return entry.warning;
        case SelectedRole:
            return entry.syncDbId == _selectedSyncDbId;
        default:
            return {};
    }
}

QHash<int, QByteArray> SyncSelectorModel::roleNames() const {
    return {
            {EntryTypeRole, "entryType"}, {SyncDbIdRole, "syncDbId"},     {TitleRole, "title"},
            {SubtitleRole, "subtitle"},   {DriveColorRole, "driveColor"}, {ErrorCountRole, "errorCount"},
            {WarningRole, "hasWarning"},  {SelectedRole, "isSelected"},
    };
}

qint32 SyncSelectorModel::selectedRow() const {
    return rowForSyncDbId(_selectedSyncDbId);
}

void SyncSelectorModel::rebuild() {
    const auto previousSelectedRow = selectedRow();
    std::vector<Entry> entries;

    for (const auto &driveContext: _cache.driveContexts()) {
        const auto color = driveColor(driveContext.drive);
        const bool warning = driveHasWarning(driveContext.drive);
        const QString driveName = QString::fromStdString(driveContext.drive.name());

        const auto classicSyncCount = static_cast<std::size_t>(std::ranges::count_if(
                driveContext.syncInfos, [](const BaseSync &syncInfo) { return syncInfo.targetNodeId().empty(); }));
        const auto appendSync = [&](const BaseSync &syncInfo) {
            const bool isClassic = syncInfo.targetNodeId().empty();
            const auto syncContext = _cache.syncContext(syncInfo.dbId());
            const auto errorCount = syncContext.has_value() ? static_cast<qint32>(syncContext->errors.size()) : 0;
            const bool usePreciseClassicLabel = isClassic && classicSyncCount > 1;
            entries.emplace_back(Entry{
                    isClassic ? EntryType::ClassicSync : EntryType::AdvancedSync,
                    syncInfo.dbId(),
                    usePreciseClassicLabel || !isClassic ? localFolderName(syncInfo) : driveName,
                    usePreciseClassicLabel || !isClassic ? driveName : QString{},
                    color,
                    errorCount,
                    warning,
            });
        };

        for (const auto &syncInfo: driveContext.syncInfos) {
            if (syncInfo.targetNodeId().empty()) {
                appendSync(syncInfo);
            }
        }
        for (const auto &syncInfo: driveContext.syncInfos) {
            if (!syncInfo.targetNodeId().empty()) {
                appendSync(syncInfo);
            }
        }
    }

    beginResetModel();
    _entries = std::move(entries);
    _selectedSyncDbId = static_cast<SyncDbId>(_selectionStore.currentSyncDbId());
    endResetModel();

    if (selectedRow() != previousSelectedRow) {
        emit selectedRowChanged();
    }
}

void SyncSelectorModel::handleSelectionChanged() {
    const auto previousSelectedRow = selectedRow();
    _selectedSyncDbId = static_cast<SyncDbId>(_selectionStore.currentSyncDbId());
    const auto currentSelectedRow = selectedRow();

    if (previousSelectedRow >= 0) {
        emit dataChanged(index(previousSelectedRow, 0), index(previousSelectedRow, 0), {SelectedRole});
    }
    if (currentSelectedRow >= 0 && currentSelectedRow != previousSelectedRow) {
        emit dataChanged(index(currentSelectedRow, 0), index(currentSelectedRow, 0), {SelectedRole});
    }
    if (currentSelectedRow != previousSelectedRow) {
        emit selectedRowChanged();
    }
}

qint32 SyncSelectorModel::rowForSyncDbId(const SyncDbId syncDbId) const {
    if (syncDbId == 0) {
        return -1;
    }
    for (qint32 row = 0; row < static_cast<qint32>(_entries.size()); ++row) {
        const auto &entry = _entries[static_cast<std::size_t>(row)];
        if (entry.syncDbId == syncDbId) {
            return row;
        }
    }
    return -1;
}

} // namespace KDC
