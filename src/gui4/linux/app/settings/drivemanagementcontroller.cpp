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

#include "drivemanagementcontroller.h"

#include "app/appconstants.h"
#include "app/cache/appcache.h"
#include "app/services/commservice.h"
#include "app/services/syncservice.h"
#include "app/syncconfiguration/localpaths.h"
#include "libcommon/utility/types.h"

#include <QDesktopServices>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QPointer>
#include <QUrl>

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcDriveManagementController, "gui.v4.drivemanagementcontroller", QtInfoMsg)

[[nodiscard]] QColor colorFromDriveValue(const std::string &value) {
    const QColor color{QString::fromStdString(value)};
    return color.isValid() ? color : AppConstants::Drive::defaultColor();
}

} // namespace

DriveManagementController::DriveManagementController(AppCache &appCache, CommService &commService, SyncService &syncService,
                                                     QObject *const parent) :
    QObject(parent),
    _appCache(appCache),
    _commService(commService),
    _syncService(syncService) {
    (void) connect(&_appCache, &AppCache::accountsChanged, this, &DriveManagementController::refresh);
    (void) connect(&_appCache, &AppCache::drivesChanged, this, &DriveManagementController::refresh);
    (void) connect(&_appCache, &AppCache::syncsChanged, this, &DriveManagementController::refresh);
    (void) connect(&_appCache, &AppCache::syncCreationPendingChanged, this, &DriveManagementController::refresh);
    (void) connect(&_syncService, &SyncService::syncActionPendingChanged, this, [this](const qint64 syncDbId) {
        if (hasTarget() && (syncDbId == _mainSyncDbId || syncDbId == _deletingSyncDbId)) {
            emit presentationChanged();
        }
    });
}

bool DriveManagementController::deletePending() const {
    const bool mainSyncDeletePending = hasMainSync() && _syncService.isDeleteSyncPending(_mainSyncDbId);
    const bool requestedDeletePending = _deletingSyncDbId != 0 && _syncService.isDeleteSyncPending(_deletingSyncDbId);
    return mainSyncDeletePending || requestedDeletePending;
}

void DriveManagementController::open(const qint64 driveDbId) {
    resetTarget();

    _driveDbId = static_cast<DriveDbId>(driveDbId);
    qCInfo(lcDriveManagementController) << "Opening drive management | driveDbId:" << _driveDbId;

    refresh();
}

void DriveManagementController::close(const qint64 driveDbId) {
    if (static_cast<DriveDbId>(driveDbId) != _driveDbId) {
        return;
    }

    resetTarget();
    emit presentationChanged();
}

void DriveManagementController::reloadSelection() {
    if (!hasMainSync() || _selectionState == SelectionState::Loading) {
        return;
    }

    loadSelection();
    emit presentationChanged();
}

void DriveManagementController::openLocalFolder() const {
    if (!hasMainSync()) {
        return;
    }

    const auto mainSync = _appCache.sync(_mainSyncDbId);
    if (!mainSync) {
        return;
    }

    const QString syncFolderPath = Path2QStr(mainSync->localPath());
    const QFileInfo localFolder{syncFolderPath};
    if (syncFolderPath.isEmpty() || !localFolder.exists() || !localFolder.isDir()) {
        qCWarning(lcDriveManagementController) << "Cannot open missing synchronization folder | path:" << syncFolderPath;
        return;
    }

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(localFolder.absoluteFilePath()))) {
        qCWarning(lcDriveManagementController)
                << "Desktop service failed to open synchronization folder | path:" << syncFolderPath;
    }
}

void DriveManagementController::deleteMainSync() {
    if (!hasMainSync() || deletePending()) {
        return;
    }

    const SyncDbId syncDbId = _mainSyncDbId;
    const uint64_t targetGeneration = _targetGeneration;
    qCInfo(lcDriveManagementController) << "Deleting main synchronization | syncDbId:" << syncDbId;

    _deletingSyncDbId = syncDbId;

    // Bound to the target rather than to the main synchronization: the SYNC_REMOVED push may replace the main
    // synchronization before this response arrives, and the confirmation dialog still waits for the outcome.
    _syncService.deleteSync(syncDbId, [self = QPointer(this), targetGeneration, syncDbId](const ExitInfo &exitInfo) {
        if (!self || targetGeneration != self->_targetGeneration) {
            return;
        }

        self->_deletingSyncDbId = 0;
        emit self->presentationChanged();

        if (!exitInfo) {
            qCWarning(lcDriveManagementController) << "Main synchronization deletion failed | syncDbId:" << syncDbId
                                                   << "/ code:" << exitInfo.code() << "/ cause:" << exitInfo.cause();
            emit self->deleteFailed();
            return;
        }

        // The page itself follows the SYNC_REMOVED push through AppCache.
        emit self->deleteSucceeded();
    });
}

// Re-resolves the target from AppCache. The main synchronization can change under the page: deleted, replaced by the
// next classic one of a legacy migration, or created from this page.
void DriveManagementController::refresh() {
    if (!hasTarget()) {
        return;
    }

    const auto context = _appCache.driveContext(_driveDbId);
    if (!context || context->syncInfos.empty()) {
        qCInfo(lcDriveManagementController) << "Managed drive has no synchronization left | driveDbId:" << _driveDbId;
        resetTarget();
        emit presentationChanged();
        emit driveRemoved();
        return;
    }

    _availableDriveKey = AvailableDriveKey{
            .userDbId = context->userDisplayInfo.dbId(),
            .accountId = context->accountInfo.accountId(),
            .driveId = context->drive.driveId(),
    };
    _driveName = QString::fromStdString(context->drive.name());
    _driveColor = colorFromDriveValue(context->drive.color());

    const auto mainSync = _appCache.mainSync(_driveDbId);
    const SyncDbId mainSyncDbId = mainSync ? mainSync->dbId() : 0;
    _localPath = mainSync ? displayLocalPath(Path2QStr(mainSync->localPath())) : QString();
    _syncCreationPending = _appCache.isSyncCreationPending(_availableDriveKey);

    if (mainSyncDbId != _mainSyncDbId) {
        _mainSyncDbId = mainSyncDbId;
        _customSelection = false;
        ++_selectionGeneration;
        _selectionState = SelectionState::Idle;
        if (hasMainSync()) {
            loadSelection();
        }
    }

    emit presentationChanged();
}

void DriveManagementController::loadSelection() {
    _selectionState = SelectionState::Loading;
    const uint64_t selectionGeneration = _selectionGeneration;
    const SyncDbId syncDbId = _mainSyncDbId;

    _commService.requestBlacklistedNodeList(syncDbId, [self = QPointer(this), selectionGeneration, syncDbId](
                                                              const ExitInfo &exitInfo, const std::vector<NodeId> &blackList) {
        if (!self || selectionGeneration != self->_selectionGeneration) {
            return;
        }

        if (!exitInfo) {
            qCWarning(lcDriveManagementController) << "Blacklist loading failed | syncDbId:" << syncDbId
                                                   << "/ code:" << exitInfo.code() << "/ cause:" << exitInfo.cause();
            self->_selectionState = SelectionState::Failed;
            emit self->presentationChanged();
            return;
        }

        self->_selectionState = SelectionState::Loaded;
        self->_customSelection = !blackList.empty();
        emit self->presentationChanged();
    });
}

void DriveManagementController::resetTarget() {
    ++_targetGeneration;
    ++_selectionGeneration;
    _driveDbId = 0;
    _availableDriveKey = AvailableDriveKey{};
    _mainSyncDbId = 0;
    _driveName.clear();
    _driveColor = QColor();
    _localPath.clear();
    _selectionState = SelectionState::Idle;
    _syncCreationPending = false;
    _customSelection = false;
    _deletingSyncDbId = 0;
}

} // namespace KDC
