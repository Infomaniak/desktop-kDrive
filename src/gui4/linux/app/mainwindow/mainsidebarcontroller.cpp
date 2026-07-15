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

#include "libcommon/utility/types.h"

#include <QDesktopServices>
#include <QFileInfo>
#include <QUrl>

Q_LOGGING_CATEGORY(lcMainSidebarController, "gui.v4.mainsidebarcontroller", QtInfoMsg)

namespace KDC {

namespace {

const QColor defaultDriveColor{QStringLiteral("#0098FF")};

} // namespace

MainSidebarController::MainSidebarController(const AppCache &cache, MainSelectionStore &selectionStore, QObject *const parent) :
    QObject(parent),
    _selectionStore(selectionStore),
    _syncListModel(cache, selectionStore, this) {
    (void) connect(&_selectionStore, &MainSelectionStore::currentSyncContextChanged, this,
                   &MainSidebarController::currentSyncContextChanged);
    (void) connect(&_syncListModel, &QAbstractItemModel::modelReset, this, &MainSidebarController::syncCountChanged);
}

qint32 MainSidebarController::syncCount() const {
    return static_cast<qint32>(_syncListModel.rowCount());
}

qint32 MainSidebarController::selectedRow() const {
    return _syncListModel.selectedRow();
}

QString MainSidebarController::currentDriveName() const {
    const auto context = _selectionStore.currentSyncContext();
    return context.has_value() ? QString::fromStdString(context->drive.name()) : QString{};
}

QColor MainSidebarController::currentDriveColor() const {
    const auto context = _selectionStore.currentSyncContext();
    if (!context.has_value()) {
        return defaultDriveColor;
    }
    const QColor color{QString::fromStdString(context->drive.color())};
    return color.isValid() ? color : defaultDriveColor;
}

bool MainSidebarController::canOpenCurrentSyncFolder() const {
    const auto context = _selectionStore.currentSyncContext();
    return context.has_value() && !context->syncInfo.localPath().empty();
}

qint32 MainSidebarController::currentErrorCount() const {
    const auto context = _selectionStore.currentSyncContext();
    return context.has_value() ? static_cast<qint32>(context->errorInfoList.size()) : 0;
}

void MainSidebarController::selectSync(const qint64 syncDbId) {
    _selectionStore.selectSync(syncDbId);
}

bool MainSidebarController::openCurrentSyncFolder() const {
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

} // namespace KDC
