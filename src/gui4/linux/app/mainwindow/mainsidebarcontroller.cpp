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

#include "app/mainwindow/mainsidebarcontroller.h"

#include "app/appconstants.h"
#include "libcommon/utility/types.h"

#include <QDesktopServices>
#include <QFileInfo>
#include <QUrl>

Q_LOGGING_CATEGORY(lcMainSidebarController, "gui.v4.mainsidebarcontroller", QtInfoMsg)

namespace KDC {

MainSidebarController::MainSidebarController(const AppCache &cache, MainSelectionStore &selectionStore, QObject *const parent) :
    QObject(parent),
    _selectionStore(selectionStore),
    _syncListModel(cache, selectionStore, this) {
    (void) connect(&_selectionStore, &MainSelectionStore::currentSyncContextChanged, this,
                   &MainSidebarController::currentSyncContextChanged);
    (void) connect(&_syncListModel, &SyncListModel::selectedRowChanged, this, &MainSidebarController::selectedRowChanged);
    (void) connect(&_syncListModel, &QAbstractItemModel::modelReset, this, &MainSidebarController::syncCountChanged);
    (void) connect(&_syncListModel, &QAbstractItemModel::modelReset, this, &MainSidebarController::currentSyncContextChanged);
}

qint32 MainSidebarController::syncCount() const {
    return _syncListModel.rowCount();
}

qint32 MainSidebarController::selectedRow() const {
    return _syncListModel.selectedRow();
}

QString MainSidebarController::currentTitle() const {
    return selectedData(SyncListModel::TitleRole).toString();
}

QString MainSidebarController::currentSubtitle() const {
    return selectedData(SyncListModel::SubtitleRole).toString();
}

QColor MainSidebarController::currentDriveColor() const {
    const auto context = _selectionStore.currentSyncContext();
    if (!context.has_value()) {
        return AppConstants::Drive::defaultColor();
    }
    const QColor color{QString::fromStdString(context->drive.color())};
    return color.isValid() ? color : AppConstants::Drive::defaultColor();
}

bool MainSidebarController::canOpenCurrentSyncFolder() const {
    // Keep this QML-facing capability check free of filesystem I/O because bindings may evaluate it frequently.
    const auto context = _selectionStore.currentSyncContext();
    return context.has_value() && !context->syncInfo.localPath().empty();
}

qint32 MainSidebarController::currentErrorCount() const {
    const auto context = _selectionStore.currentSyncContext();
    return context.has_value() ? static_cast<qint32>(context->errors.size()) : 0;
}

void MainSidebarController::selectSync(const qint64 syncDbId) {
    _selectionStore.selectSync(syncDbId);
}

bool MainSidebarController::openCurrentSyncFolder() const {
    // Do not reuse canOpenCurrentSyncFolder(): the action must read the current context once and validate the filesystem.
    const auto context = _selectionStore.currentSyncContext();
    if (!context.has_value()) {
        qCWarning(lcMainSidebarController) << "Cannot open sync folder without a current synchronization";
        return false;
    }

    const QString localPath = Path2QStr(context->syncInfo.localPath());
    const QFileInfo localFolder{localPath};
    if (localPath.isEmpty() || !localFolder.exists() || !localFolder.isDir()) {
        qCWarning(lcMainSidebarController) << "Cannot open missing synchronization folder | path:" << localPath;
        return false;
    }

    const QUrl localUrl = QUrl::fromLocalFile(localFolder.absoluteFilePath());
    qCInfo(lcMainSidebarController) << "Opening synchronization folder | syncDbId:" << context->syncInfo.dbId()
                                    << "| path:" << localPath;
    if (!QDesktopServices::openUrl(localUrl)) {
        qCWarning(lcMainSidebarController) << "Desktop service failed to open synchronization folder | path:" << localPath;
        return false;
    }
    return true;
}

QVariant MainSidebarController::selectedData(const SyncListModel::Role role) const {
    const qint32 row = _syncListModel.selectedRow();
    return row < 0 ? QVariant{} : _syncListModel.data(_syncListModel.index(row, 0), role);
}

} // namespace KDC
