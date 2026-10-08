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

#include "advancedsynccreationcontroller.h"

#include "app/cache/appcache.h"
#include "app/services/commservice.h"
#include "app/services/syncservice.h"
#include "app/syncconfiguration/localpaths.h"
#include "libcommon/utility/utility.h"

#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QPointer>


using namespace Qt::StringLiterals;

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcAdvancedSyncCreationController, "gui.v4.advancedsynccreationcontroller", QtInfoMsg)
} // namespace

AdvancedSyncCreationController::AdvancedSyncCreationController(AppCache &appCache, CommService &commService,
                                                               SyncService &syncService, const DriveDbId driveDbId,
                                                               QObject *const parent) :
    QObject(parent),
    _appCache(appCache),
    _commService(commService),
    _syncService(syncService),
    _folderProvider(commService),
    _pickerModel(_folderProvider, this),
    _driveDbId(driveDbId) {
    qCInfo(lcAdvancedSyncCreationController) << "Opening advanced sync creation | driveDbId:" << _driveDbId;
    (void) connect(&_pickerModel, &RemoteFolderPickerModel::selectionChanged, this,
                   &AdvancedSyncCreationController::presentationChanged);
}

bool AdvancedSyncCreationController::busy() const {
    return _state == State::CheckingLocalFolder || _state == State::Submitting;
}

QString AdvancedSyncCreationController::localFolderName() const {
    const QString name = QFileInfo(_localPath).fileName();
    return name.isEmpty() ? _localPath : name;
}

QString AdvancedSyncCreationController::localPath() const {
    return displayLocalPath(_localPath);
}

bool AdvancedSyncCreationController::canSubmit() const {
    return _state == State::Editing && !_locationPickerOpen && !_localPath.isEmpty() && !_remoteNodeId.isEmpty();
}

bool AdvancedSyncCreationController::canConfirmLocation() const {
    return _locationPickerOpen && _pickerModel.hasSelection();
}

void AdvancedSyncCreationController::cancelCurrentPage() {
    if (busy() || !active()) {
        return;
    }

    if (_locationPickerOpen) {
        _locationPickerOpen = false;
        emit presentationChanged();
        return;
    }

    finish();
}

void AdvancedSyncCreationController::requestLocalFolder() {
    if (_state != State::Editing || _locationPickerOpen) {
        return;
    }

    const QString initialFolder = _localPath.isEmpty() ? QDir::homePath() : _localPath;
    emit localFolderRequested(QUrl::fromLocalFile(initialFolder));
}

void AdvancedSyncCreationController::notifyLocalFolderDialogClosed() {
    emit localFolderDialogClosed();
}

void AdvancedSyncCreationController::applyLocalFolder(const QUrl &folderUrl) {
    const QString path = QDir::cleanPath(folderUrl.toLocalFile());
    if (_state != State::Editing || path.isEmpty()) {
        return;
    }

    _localFolderInvalid = false;
    _submitFailed = false;
    setState(State::CheckingLocalFolder);

    _commService.requestIsPathValidForNewSync(QStr2Path(path), SyncConfiguration::Advanced,
                                              [self = QPointer(this), path](const ExitInfo &exitInfo, const bool valid) {
                                                  if (!self || self->_state != State::CheckingLocalFolder) {
                                                      return;
                                                  }

                                                  if (!exitInfo || !valid) {
                                                      qCInfo(lcAdvancedSyncCreationController)
                                                              << "Local folder refused for an advanced sync | path:" << path;
                                                      // The previously accepted folder stays selected.
                                                      self->_localFolderInvalid = true;
                                                  } else {
                                                      self->_localPath = path;
                                                  }

                                                  self->setState(State::Editing);
                                              });
}

void AdvancedSyncCreationController::openLocationPicker() {
    if (_state != State::Editing || _locationPickerOpen) {
        return;
    }

    if (!_pickerConfigured) {
        const auto context = _appCache.driveContext(_driveDbId);
        if (!context) {
            return;
        }

        // Folders already targeted by a synchronization of the drive cannot be chosen again.
        NodeSet unavailableNodeIds;
        for (const auto &syncInfo: _appCache.syncsForDrive(_driveDbId)) {
            if (!syncInfo.targetNodeId().empty()) {
                (void) unavailableNodeIds.insert(syncInfo.targetNodeId());
            }
        }

        _pickerModel.configure(context->userDisplayInfo.dbId(), context->drive.driveId(),
                               QString::fromStdString(context->drive.name()), unavailableNodeIds);
        _pickerConfigured = true;
    }

    // The picker starts from the confirmed choice: a selection abandoned by a previous Cancel is dropped.
    _pickerModel.restoreSelection(QStr2Str(_remoteNodeId), _remoteFolderName, _remotePath);
    _submitFailed = false;
    _locationPickerOpen = true;
    emit presentationChanged();
}

void AdvancedSyncCreationController::confirmLocation() {
    if (!canConfirmLocation()) {
        return;
    }

    _remoteNodeId = _pickerModel.selectedNodeId();
    _remoteFolderName = _pickerModel.selectedName();
    _remotePath = _pickerModel.selectedPath();
    _locationPickerOpen = false;
    emit presentationChanged();
}

void AdvancedSyncCreationController::submit() {
    if (!canSubmit()) {
        return;
    }

    const auto context = _appCache.driveContext(_driveDbId);
    if (!context) {
        return;
    }

    SyncAddRequest request;
    request.userDbId = context->userDisplayInfo.dbId();
    request.accountId = context->accountInfo.accountId();
    request.driveId = context->drive.driveId();
    request.localFolderPath = QStr2Path(_localPath);
    request.serverFolderPath = QStr2Path(_remotePath);
    request.serverFolderNodeId = QStr2Str(_remoteNodeId);
    request.liteSync = false;

    _submitFailed = false;
    setState(State::Submitting);
    qCInfo(lcAdvancedSyncCreationController)
            << "Creating advanced synchronization | driveDbId:" << _driveDbId << "/ remoteNodeId:" << _remoteNodeId;

    const bool sent = _syncService.addDriveSync(request, [self = QPointer(this)](const ExitInfo &exitInfo, const BaseSync &) {
        if (!self) {
            return;
        }

        if (!exitInfo) {
            qCWarning(lcAdvancedSyncCreationController)
                    << "Advanced synchronization creation failed | code:" << exitInfo.code() << "/ cause:" << exitInfo.cause();
            self->_submitFailed = true;
            self->setState(State::Editing);
            return;
        }

        // The page lists the new synchronization once its SYNC_ADDED push reaches AppCache.
        self->finish();
    });

    if (!sent) {
        _submitFailed = true;
        setState(State::Editing);
    }
}

void AdvancedSyncCreationController::setState(const State state) {
    if (_state == state) {
        return;
    }

    _state = state;
    emit presentationChanged();
}

void AdvancedSyncCreationController::finish() {
    _locationPickerOpen = false;
    setState(State::Finished);
    emit activeChanged();
}

} // namespace KDC
