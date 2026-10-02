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

#include "advancedsyncscontroller.h"

#include "app/appconstants.h"
#include "app/cache/appcache.h"
#include "app/services/commservice.h"
#include "app/services/syncservice.h"
#include "app/syncconfiguration/localpaths.h"

#include <QDesktopServices>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QPointer>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcAdvancedSyncsController, "gui.v4.advancedsyncscontroller", QtInfoMsg)

[[nodiscard]] QColor colorFromDriveValue(const std::string &value) {
    const QColor color{QString::fromStdString(value)};
    return color.isValid() ? color : AppConstants::Drive::defaultColor();
}

[[nodiscard]] QString lastPathSegment(const SyncPath &path) {
    const QString name = Path2QStr(path.filename());
    return name.isEmpty() ? Path2QStr(path) : name;
}

/**
 * A synchronization without a remote target is an extra classic one left by a legacy migration: it synchronizes the drive
 * root, which is presented by the drive name.
 */
[[nodiscard]] AdvancedSyncListModel::Row rowForSync(const BaseSync &syncInfo, const QString &driveName) {
    const bool targetsDriveRoot = syncInfo.targetNodeId().empty() || syncInfo.targetPath().empty();

    AdvancedSyncListModel::Row row;
    row.syncDbId = syncInfo.dbId();
    row.localFolderName = lastPathSegment(syncInfo.localPath());
    row.localPath = displayLocalPath(Path2QStr(syncInfo.localPath()));
    row.remoteFolderName = targetsDriveRoot ? driveName : lastPathSegment(syncInfo.targetPath());
    row.remotePath = targetsDriveRoot ? u"/"_s : Path2QStr(syncInfo.targetPath());
    return row;
}

[[nodiscard]] bool isSupportedWebUrl(const QUrl &url) {
    return url.isValid() && !url.host().isEmpty() && (url.scheme() == u"https"_s || url.scheme() == u"http"_s);
}

} // namespace

AdvancedSyncsController::AdvancedSyncsController(AppCache &appCache, CommService &commService, SyncService &syncService,
                                                 QObject *const parent) :
    QObject(parent),
    _appCache(appCache),
    _commService(commService),
    _syncService(syncService) {
    (void) connect(&_appCache, &AppCache::accountsChanged, this, &AdvancedSyncsController::refresh);
    (void) connect(&_appCache, &AppCache::drivesChanged, this, &AdvancedSyncsController::refresh);
    (void) connect(&_appCache, &AppCache::syncsChanged, this, &AdvancedSyncsController::refresh);
    (void) connect(&_syncService, &SyncService::syncActionPendingChanged, this, [this](const qint64 syncDbId) {
        if (targets(static_cast<SyncDbId>(syncDbId))) {
            _model.setDeletePending(static_cast<SyncDbId>(syncDbId), _syncService.isDeleteSyncPending(syncDbId));
        }
    });
}

void AdvancedSyncsController::open(const qint64 driveDbId) {
    resetTarget();

    _driveDbId = static_cast<DriveDbId>(driveDbId);
    qCInfo(lcAdvancedSyncsController) << "Opening advanced synchronizations | driveDbId:" << _driveDbId;

    refresh();
}

void AdvancedSyncsController::close(const qint64 driveDbId) {
    if (static_cast<DriveDbId>(driveDbId) != _driveDbId) {
        return;
    }

    resetTarget();
    emit presentationChanged();
}

void AdvancedSyncsController::reloadBlackList(const qint64 syncDbId) {
    if (const auto *const row = _model.row(static_cast<SyncDbId>(syncDbId));
        !hasTarget() || !row || row->blackListState == AdvancedSyncListModel::BlackListState::Loading) {
        return;
    }

    loadBlackList(static_cast<SyncDbId>(syncDbId));
}

void AdvancedSyncsController::openLocalFolder(const qint64 syncDbId) const {
    if (!targets(static_cast<SyncDbId>(syncDbId))) {
        return;
    }

    const auto syncInfo = _appCache.sync(static_cast<SyncDbId>(syncDbId));
    if (!syncInfo) {
        return;
    }

    const QString syncFolderPath = Path2QStr(syncInfo->localPath());
    const QFileInfo localFolder{syncFolderPath};
    if (syncFolderPath.isEmpty() || !localFolder.exists() || !localFolder.isDir()) {
        qCWarning(lcAdvancedSyncsController) << "Cannot open missing synchronization folder | path:" << syncFolderPath;
        return;
    }

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(localFolder.absoluteFilePath()))) {
        qCWarning(lcAdvancedSyncsController) << "Desktop service failed to open synchronization folder | path:" << syncFolderPath;
    }
}

void AdvancedSyncsController::openRemoteFolder(const qint64 syncDbId) {
    if (!targets(static_cast<SyncDbId>(syncDbId))) {
        return;
    }

    const auto syncInfo = _appCache.sync(static_cast<SyncDbId>(syncDbId));
    if (!syncInfo) {
        return;
    }

    if (syncInfo->targetNodeId().empty()) {
        if (const QUrl driveUrl =
                    AppConstants::WebDrive::destinationUri(_driveId, AppConstants::WebDrive::Destination::OnlineDrive);
            !QDesktopServices::openUrl(driveUrl)) {
            qCWarning(lcAdvancedSyncsController) << "Desktop service failed to open the web drive | driveId:" << _driveId;
        }
        return;
    }

    _commService.requestSyncGetPrivateLinkUrl(
            _driveDbId, syncInfo->targetNodeId(), [syncDbId](const ExitInfo &exitInfo, const QString &urlText) {
                if (!exitInfo) {
                    qCWarning(lcAdvancedSyncsController) << "Remote folder link request failed | syncDbId:" << syncDbId
                                                         << "/ code:" << exitInfo.code() << "/ cause:" << exitInfo.cause();
                    return;
                }

                const QUrl url{urlText};
                if (!isSupportedWebUrl(url)) {
                    qCWarning(lcAdvancedSyncsController) << "Remote folder link rejected | syncDbId:" << syncDbId;
                    return;
                }

                if (!QDesktopServices::openUrl(url)) {
                    qCWarning(lcAdvancedSyncsController)
                            << "Desktop service failed to open remote folder | syncDbId:" << syncDbId;
                }
            });
}

void AdvancedSyncsController::deleteSync(const qint64 syncDbId) {
    const auto targetSyncDbId = static_cast<SyncDbId>(syncDbId);
    if (!targets(targetSyncDbId) || deletePending() || _syncService.isDeleteSyncPending(targetSyncDbId)) {
        return;
    }

    const uint64_t targetGeneration = _targetGeneration;
    qCInfo(lcAdvancedSyncsController) << "Deleting advanced synchronization | syncDbId:" << targetSyncDbId;

    _deletingSyncDbId = targetSyncDbId;
    emit presentationChanged();

    // The SYNC_REMOVED push may remove the row before this response arrives; the confirmation dialog still waits for it.
    _syncService.deleteSync(targetSyncDbId, [self = QPointer(this), targetGeneration, syncDbId](const ExitInfo &exitInfo) {
        if (!self || targetGeneration != self->_targetGeneration) {
            return;
        }

        self->_deletingSyncDbId = 0;
        emit self->presentationChanged();

        if (!exitInfo) {
            qCWarning(lcAdvancedSyncsController) << "Advanced synchronization deletion failed | syncDbId:" << syncDbId
                                                 << "/ code:" << exitInfo.code() << "/ cause:" << exitInfo.cause();
            emit self->deleteFailed(syncDbId);
            return;
        }

        emit self->deleteSucceeded(syncDbId);
    });
}

// Re-resolves the advanced synchronizations of the target from AppCache: one can be added, deleted, or become the main
// synchronization when a legacy migration left several classic ones.
void AdvancedSyncsController::refresh() {
    if (!hasTarget()) {
        return;
    }

    std::vector<AdvancedSyncListModel::Row> rows;
    const auto context = _appCache.driveContext(_driveDbId);
    if (context) {
        _driveId = context->drive.driveId();
        _driveColor = colorFromDriveValue(context->drive.color());

        const QString driveName = QString::fromStdString(context->drive.name());
        for (const auto &syncInfo: _appCache.advancedSyncs(_driveDbId)) {
            AdvancedSyncListModel::Row row = rowForSync(syncInfo, driveName);
            row.deletePending = _syncService.isDeleteSyncPending(syncInfo.dbId());
            rows.push_back(std::move(row));
        }
    }

    for (const SyncDbId addedSyncDbId: _model.replaceRows(std::move(rows))) {
        loadBlackList(addedSyncDbId);
    }

    emit presentationChanged();
}

void AdvancedSyncsController::loadBlackList(const SyncDbId syncDbId) {
    _model.setBlackListState(syncDbId, AdvancedSyncListModel::BlackListState::Loading);
    const uint64_t targetGeneration = _targetGeneration;

    _commService.requestBlacklistedNodeList(syncDbId, [self = QPointer(this), targetGeneration, syncDbId](
                                                              const ExitInfo &exitInfo, const std::vector<NodeId> &blackList) {
        if (!self || targetGeneration != self->_targetGeneration) {
            return;
        }

        if (!exitInfo) {
            qCWarning(lcAdvancedSyncsController) << "Blacklist loading failed | syncDbId:" << syncDbId
                                                 << "/ code:" << exitInfo.code() << "/ cause:" << exitInfo.cause();
            self->_model.setBlackListState(syncDbId, AdvancedSyncListModel::BlackListState::Failed);
            return;
        }

        self->_model.setCustomSelection(syncDbId, !blackList.empty());
        self->_model.setBlackListState(syncDbId, AdvancedSyncListModel::BlackListState::Loaded);
    });
}

void AdvancedSyncsController::resetTarget() {
    ++_targetGeneration;
    _driveDbId = 0;
    _driveId = 0;
    _driveColor = QColor();
    _deletingSyncDbId = 0;
    (void) _model.replaceRows({});
}

} // namespace KDC
