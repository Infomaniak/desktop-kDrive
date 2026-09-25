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

#include "syncfolderselectioncontroller.h"

#include "app/appconstants.h"
#include "app/cache/appcache.h"
#include "app/services/commservice.h"

#include <QLoggingCategory>
#include <QPointer>

#include <algorithm>
#include <iterator>

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcSyncFolderSelectionController, "gui.v4.syncfolderselectioncontroller", QtInfoMsg)
} // namespace

SyncFolderSelectionController::SyncFolderSelectionController(AppCache &appCache, CommService &commService,
                                                             QObject *const parent) :
    QObject(parent),
    _appCache(appCache),
    _commService(commService),
    _folderProvider(commService),
    _folderTreeModel(_folderProvider, this) {
    (void) connect(&_folderTreeModel, &RemoteFolderTreeModel::stateChanged, this, &SyncFolderSelectionController::stateChanged);
    (void) connect(&_folderTreeModel, &RemoteFolderTreeModel::selectionChanged, this,
                   &SyncFolderSelectionController::stateChanged);
}

bool SyncFolderSelectionController::canSave() const {
    if (_state != State::Editing) {
        return false;
    }

    if (_folderTreeModel.loading() || _folderTreeModel.loadFailed() || excludedFolderLimitExceeded()) {
        return false;
    }

    return _folderTreeModel.blackList() != _confirmedBlackList;
}

bool SyncFolderSelectionController::excludedFolderLimitExceeded() const {
    if (_state != State::Editing) {
        return false;
    }

    return std::ssize(_folderTreeModel.blackList()) > maxExcludedFolders();
}

qsizetype SyncFolderSelectionController::maxExcludedFolders() {
    return AppConstants::SyncConfiguration::maxExcludedFolders;
}

void SyncFolderSelectionController::open(const qint64 syncDbId) {
    resetTarget();

    _syncDbId = static_cast<SyncDbId>(syncDbId);
    qCInfo(lcSyncFolderSelectionController) << "Opening folder selection | syncDbId:" << _syncDbId;

    loadBlackList();
}

void SyncFolderSelectionController::close(const qint64 syncDbId) {
    if (static_cast<SyncDbId>(syncDbId) != _syncDbId) {
        return;
    }

    resetTarget();
    emit stateChanged();
}

void SyncFolderSelectionController::retry() {
    if (_state != State::LoadFailed) {
        return;
    }

    loadBlackList();
}

void SyncFolderSelectionController::save() {
    if (!canSave()) {
        return;
    }

    const SyncDbId syncDbId = _syncDbId;
    const std::vector<NodeId> blackList = _folderTreeModel.blackList();
    qCInfo(lcSyncFolderSelectionController) << "Saving folder selection | syncDbId:" << syncDbId
                                            << "/ excludedFolders:" << blackList.size();

    _saveFailed = false;
    setState(State::Saving);

    _commService.requestBlacklistedNodeSetList(
            syncDbId, blackList, [self = QPointer(this), syncDbId, blackList](const ExitInfo &exitInfo) {
                if (!self || syncDbId != self->_syncDbId) {
                    return;
                }

                if (!exitInfo) {
                    qCWarning(lcSyncFolderSelectionController) << "Folder selection saving failed | syncDbId:" << syncDbId
                                                               << "/ code:" << exitInfo.code() << "/ cause:" << exitInfo.cause();
                    self->_saveFailed = true;
                    self->setState(State::Editing);
                    return;
                }

                self->_confirmedBlackList = blackList;
                self->setState(State::Editing);
                emit self->saved();
            });
}

void SyncFolderSelectionController::retranslate() {
    _folderTreeModel.retranslate();
}

void SyncFolderSelectionController::loadBlackList() {
    const auto context = _appCache.syncContext(_syncDbId);
    if (!context) {
        qCWarning(lcSyncFolderSelectionController) << "Cannot edit a missing synchronization | syncDbId:" << _syncDbId;
        setState(State::LoadFailed);
        return;
    }

    const UserDbId userDbId = context->userDisplayInfo.dbId();
    const DriveId driveId = context->drive.driveId();
    const NodeId rootNodeId = context->syncInfo.targetNodeId();
    const SyncDbId syncDbId = _syncDbId;
    setState(State::LoadingBlackList);

    _commService.requestBlacklistedNodeList(syncDbId, [self = QPointer(this), syncDbId, userDbId, driveId, rootNodeId](
                                                              const ExitInfo &exitInfo, const std::vector<NodeId> &blackList) {
        if (!self || syncDbId != self->_syncDbId) {
            return;
        }

        if (!exitInfo) {
            qCWarning(lcSyncFolderSelectionController) << "Blacklist loading failed | syncDbId:" << syncDbId
                                                       << "/ code:" << exitInfo.code() << "/ cause:" << exitInfo.cause();
            self->setState(State::LoadFailed);
            return;
        }

        self->_confirmedBlackList = blackList;
        (void) std::ranges::sort(self->_confirmedBlackList);
        self->setState(State::Editing);
        self->_folderTreeModel.configure(userDbId, driveId, rootNodeId, blackList);
    });
}

void SyncFolderSelectionController::setState(const State state) {
    if (_state == state) {
        return;
    }

    _state = state;
    emit stateChanged();
}

void SyncFolderSelectionController::resetTarget() {
    _syncDbId = 0;
    _state = State::Idle;
    _confirmedBlackList.clear();
    _saveFailed = false;
}

} // namespace KDC
