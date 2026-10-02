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

#include "excludedfolderscontroller.h"

#include "app/appconstants.h"
#include "app/cache/appcache.h"
#include "app/services/commservice.h"

#include <QLoggingCategory>
#include <QPointer>

#include <algorithm>
#include <iterator>

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcExcludedFoldersController, "gui.v4.excludedfolderscontroller", QtInfoMsg)
} // namespace

ExcludedFoldersController::ExcludedFoldersController(AppCache &appCache, CommService &commService, QObject *const parent) :
    QObject(parent),
    _appCache(appCache),
    _commService(commService),
    _folderProvider(commService),
    _folderTreeModel(_folderProvider, this) {
    (void) connect(&_folderTreeModel, &RemoteFolderTreeModel::stateChanged, this, &ExcludedFoldersController::stateChanged);
    (void) connect(&_folderTreeModel, &RemoteFolderTreeModel::selectionChanged, this, &ExcludedFoldersController::stateChanged);
}

bool ExcludedFoldersController::canSave() const {
    if (_state != State::Editing) {
        return false;
    }

    if (_folderTreeModel.loading() || _folderTreeModel.loadFailed() || excludedFolderLimitExceeded()) {
        return false;
    }

    return _folderTreeModel.blackList() != _confirmedBlackList;
}

bool ExcludedFoldersController::excludedFolderLimitExceeded() const {
    if (_state != State::Editing) {
        return false;
    }

    return std::ssize(_folderTreeModel.blackList()) > maxExcludedFolders();
}

qsizetype ExcludedFoldersController::maxExcludedFolders() {
    return AppConstants::SyncConfiguration::maxExcludedFolders;
}

// An open page keeps its draft untouched: the target is only recorded, and `close()` loads it once the page leaves.
void ExcludedFoldersController::preload(const SyncDbId syncDbId) {
    _preloadSyncDbId = syncDbId;
    if (_pageOpen || isLoadedOrLoading(syncDbId)) {
        return;
    }

    resetTarget();
    _syncDbId = syncDbId;
    qCInfo(lcExcludedFoldersController) << "Preloading excluded folders | syncDbId:" << _syncDbId;

    loadBlackList();
}

void ExcludedFoldersController::releasePreload(const SyncDbId syncDbId) {
    if (syncDbId == 0 || syncDbId != _preloadSyncDbId) {
        return;
    }

    _preloadSyncDbId = 0;
    // The open page keeps its target; `close()` resets it now that nothing is preloaded.
    if (_pageOpen) {
        return;
    }

    resetTarget();
    emit stateChanged();
}

void ExcludedFoldersController::open(const qint64 syncDbId) {
    _pageOpen = true;
    // A preloaded target never carries a stale draft: `close()` reloads it each time the page leaves.
    if (isLoadedOrLoading(static_cast<SyncDbId>(syncDbId))) {
        qCInfo(lcExcludedFoldersController) << "Opening preloaded excluded folders | syncDbId:" << _syncDbId;
        return;
    }

    resetTarget();

    _syncDbId = static_cast<SyncDbId>(syncDbId);
    qCInfo(lcExcludedFoldersController) << "Opening excluded folders | syncDbId:" << _syncDbId;

    loadBlackList();
}

void ExcludedFoldersController::close(const qint64 syncDbId) {
    if (static_cast<SyncDbId>(syncDbId) != _syncDbId) {
        return;
    }

    _pageOpen = false;
    resetTarget();
    // Still preloaded: reload it, which drops an abandoned draft and picks up a saved blacklist, for the next opening.
    if (_preloadSyncDbId != 0) {
        _syncDbId = _preloadSyncDbId;
        loadBlackList();
        return;
    }

    emit stateChanged();
}

void ExcludedFoldersController::retry() {
    if (_state != State::LoadFailed) {
        return;
    }

    loadBlackList();
}

void ExcludedFoldersController::save() {
    if (!canSave()) {
        return;
    }

    const SyncDbId syncDbId = _syncDbId;
    const uint64_t targetGeneration = _targetGeneration;
    const std::vector<NodeId> blackList = _folderTreeModel.blackList();
    qCInfo(lcExcludedFoldersController) << "Saving excluded folders | syncDbId:" << syncDbId
                                        << "/ excludedFolders:" << blackList.size();

    _saveFailed = false;
    setState(State::Saving);

    _commService.requestBlacklistedNodeSetList(
            syncDbId, blackList, [self = QPointer(this), targetGeneration, syncDbId, blackList](const ExitInfo &exitInfo) {
                if (!self || targetGeneration != self->_targetGeneration) {
                    return;
                }

                if (!exitInfo) {
                    qCWarning(lcExcludedFoldersController) << "Excluded folders saving failed | syncDbId:" << syncDbId
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

void ExcludedFoldersController::retranslate() {
    _folderTreeModel.retranslate();
}

// A failed load is never reused, so opening the page retries it.
bool ExcludedFoldersController::isLoadedOrLoading(const SyncDbId syncDbId) const {
    return syncDbId != 0 && syncDbId == _syncDbId && _state != State::Idle && _state != State::LoadFailed;
}

void ExcludedFoldersController::loadBlackList() {
    const auto context = _appCache.syncContext(_syncDbId);
    if (!context) {
        qCWarning(lcExcludedFoldersController) << "Cannot edit a missing synchronization | syncDbId:" << _syncDbId;
        setState(State::LoadFailed);
        return;
    }

    const UserDbId userDbId = context->userDisplayInfo.dbId();
    const DriveId driveId = context->drive.driveId();
    const NodeId rootNodeId = context->syncInfo.targetNodeId();
    const SyncDbId syncDbId = _syncDbId;
    const uint64_t targetGeneration = _targetGeneration;
    setState(State::LoadingBlackList);

    _commService.requestBlacklistedNodeList(
            syncDbId, [self = QPointer(this), targetGeneration, syncDbId, userDbId, driveId, rootNodeId](
                              const ExitInfo &exitInfo, const std::vector<NodeId> &blackList) {
                if (!self || targetGeneration != self->_targetGeneration) {
                    return;
                }

                if (!exitInfo) {
                    qCWarning(lcExcludedFoldersController) << "Blacklist loading failed | syncDbId:" << syncDbId
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

void ExcludedFoldersController::setState(const State state) {
    if (_state == state) {
        return;
    }

    _state = state;
    emit stateChanged();
}

void ExcludedFoldersController::resetTarget() {
    ++_targetGeneration;
    _syncDbId = 0;
    _state = State::Idle;
    _confirmedBlackList.clear();
    _saveFailed = false;
}

} // namespace KDC
