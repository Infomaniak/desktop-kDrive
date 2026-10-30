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

#include "app/mainwindow/conflictversioncontroller.h"

#include "app/cache/mainselectionstore.h"
#include "app/cache/syncerrorsprojection.h"
#include "app/fileformat.h"
#include "app/mainwindow/syncerroractionhandler.h"
#include "app/services/syncerrorservice.h"
#include "libcommon/info/nodeconflictinfo.h"

#include <QDateTime>
#include <QLocale>
#include <QLoggingCategory>
#include <QPointer>

#include <algorithm>
#include <memory>

namespace KDC {

namespace {

Q_LOGGING_CATEGORY(lcConflictVersion, "gui.v4.conflictversion", QtInfoMsg)

ReplicaSide replicaSide(const ConflictVersionController::Version version) {
    return version == ConflictVersionController::Version::Local ? ReplicaSide::Local : ReplicaSide::Remote;
}

} // namespace

ConflictVersionController::ConflictVersionController(const SyncErrorsProjection &projection,
                                                     const MainSelectionStore &selectionStore, SyncErrorService &syncErrorService,
                                                     SyncErrorActionHandler &actionHandler, QObject *const parent) :
    QObject(parent),
    _projection(projection),
    _selectionStore(selectionStore),
    _syncErrorService(syncErrorService),
    _actionHandler(actionHandler) {
    (void) connect(&_projection, &SyncErrorsProjection::currentErrorsChanged, this, [this] {
        // A successful resolution removes the errors, possibly before its response: the response closes the dialog.
        if (isOpen() && !_awaitingResolution && !currentError()) {
            qCInfo(lcConflictVersion) << "Conflict gone, closing the dialog | errorDbId:" << _errorDbIds[_index];
            close();
        }
    });
    // The dialog shows a conflict of the previous synchronization.
    (void) connect(&_selectionStore, &MainSelectionStore::currentSyncDbIdChanged, this, &ConflictVersionController::close);
    (void) connect(&_syncErrorService, &SyncErrorService::conflictResolutionPendingChanged, this, [this](const qint64 syncDbId) {
        if (isOpen() && syncDbId == _syncDbId) {
            emit changed();
        }
    });
}

bool ConflictVersionController::resolutionPending() const {
    return isOpen() && _syncErrorService.isConflictResolutionPending(_syncDbId);
}

ConflictVersionController::Version ConflictVersionController::mostRecent() const {
    if (!_online.modificationDate.has_value() || !_local.modificationDate.has_value() ||
        *_online.modificationDate == *_local.modificationDate) {
        return Version::None;
    }
    return *_online.modificationDate > *_local.modificationDate ? Version::Online : Version::Local;
}

void ConflictVersionController::open(std::vector<ErrorDbId> errorDbIds) {
    reset();
    _syncDbId = _selectionStore.currentSyncDbId();
    _errorDbIds = std::move(errorDbIds);
    _choices.assign(_errorDbIds.size(), Version::None);
    if (!currentError()) {
        qCWarning(lcConflictVersion) << "Version selection ignored, conflict unavailable | count:" << _errorDbIds.size();
        reset();
        return;
    }
    qCInfo(lcConflictVersion) << "Version selection opened | syncDbId:" << _syncDbId << "| count:" << _errorDbIds.size();
    showCurrent();
}

void ConflictVersionController::close() {
    if (!isOpen()) {
        return;
    }
    // The conflicts not validated yet have no choice and are left as they are.
    if (!_awaitingResolution) {
        sendChoices();
    }
    reset();
    emit changed();
}

void ConflictVersionController::select(const Version version) {
    // Once the choices are sent, the shown selection stays the one being applied.
    if (!isOpen() || loading() || _awaitingResolution || version == Version::None) {
        return;
    }
    _selection = version;
    emit changed();
}

void ConflictVersionController::validate() {
    if (!isOpen() || loading() || _selection == Version::None || _awaitingResolution) {
        return;
    }
    _choices[_index] = _selection;
    if (!isLast()) {
        ++_index;
        showCurrent();
        return;
    }

    _resolutionFailed = false;
    _awaitingResolution = true;
    sendChoices();
    emit changed();
}

void ConflictVersionController::openVersion(const Version version) const {
    const auto *const classified = currentError();
    const auto context = _selectionStore.currentSyncContext();
    if (!classified || !context.has_value() || version == Version::None) {
        qCWarning(lcConflictVersion) << "Version opening ignored, conflict unavailable";
        return;
    }
    _actionHandler.openConflictVersion(*classified, context.value(), replicaSide(version));
}

// The conflict, looked up again in the current errors: it can disappear while the dialog is open.
const ClassifiedSyncError *ConflictVersionController::currentError() const {
    if (_index >= _errorDbIds.size()) {
        return nullptr;
    }
    const auto errorDbId = _errorDbIds[_index];
    const auto &errors = _projection.currentErrors();
    const auto classifiedIt = std::ranges::find_if(
            errors, [errorDbId](const ClassifiedSyncError &classified) { return classified.error.dbId() == errorDbId; });
    return classifiedIt == errors.end() ? nullptr : std::to_address(classifiedIt);
}

// Shows the conflict at `_index` and loads both of its versions.
void ConflictVersionController::showCurrent() {
    ++_generation;
    _selection = Version::None;
    _online = {};
    _local = {};
    _loadFailed = false;
    _resolutionFailed = false;
    _pendingLoads = 0;

    const auto *const classified = currentError();
    if (!classified) {
        // Resolved elsewhere since the dialog opened.
        qCInfo(lcConflictVersion) << "Conflict gone, closing the dialog | errorDbId:" << _errorDbIds[_index];
        close();
        return;
    }
    const auto &error = classified->error;
    _itemName = Path2QStr(error.path().filename());
    _itemIconName = _itemName.isEmpty() ? QString{} : _fileIconResolver.iconName(_itemName, error.nodeType());
    load(*classified, Version::Online);
    load(*classified, Version::Local);
    emit changed();
}

void ConflictVersionController::load(const ClassifiedSyncError &classified, const Version version) {
    // The server renamed the local version into a conflict copy and put the remote version at the original path.
    const auto &error = classified.error;
    const auto &path = version == Version::Local ? error.destinationPath() : error.path();
    ++_pendingLoads;
    _syncErrorService.fetchConflictInfo(
            _syncDbId, path, replicaSide(version),
            [self = QPointer(this), generation = _generation, version](const bool ok, const NodeConflictInfo &info) {
                if (self && generation == self->_generation) {
                    self->onLoaded(version, ok, info);
                }
            });
}

void ConflictVersionController::onLoaded(const Version version, const bool ok, const NodeConflictInfo &info) {
    --_pendingLoads;
    if (ok) {
        auto &versionInfo = version == Version::Local ? _local : _online;
        versionInfo.authorName = QString::fromStdString(std::string(info.authorName()));
        versionInfo.size = formatFileSize(NodeType::File, info.fileSize());
        versionInfo.modificationDate = info.lastModificationDate();
        versionInfo.date = QLocale().toString(QDateTime::fromSecsSinceEpoch(info.lastModificationDate()).toLocalTime(),
                                              QLocale::ShortFormat);
    } else {
        _loadFailed = true;
    }
    // The more recent version is selected once both are known; equal dates leave the choice to the user.
    if (!loading() && _selection == Version::None) {
        _selection = mostRecent();
    }
    emit changed();
}

// Sends the validated choices. After the last conflict, the response closes the dialog, or keeps it open with an error
// so that the user can try again.
void ConflictVersionController::sendChoices() {
    std::vector<ErrorDbId> keepLocal;
    std::vector<ErrorDbId> keepRemote;
    for (std::size_t choiceIndex = 0; choiceIndex < _choices.size(); ++choiceIndex) {
        if (_choices[choiceIndex] == Version::Local) {
            keepLocal.push_back(_errorDbIds[choiceIndex]);
        } else if (_choices[choiceIndex] == Version::Online) {
            keepRemote.push_back(_errorDbIds[choiceIndex]);
        }
    }
    if (keepLocal.empty() && keepRemote.empty()) {
        return;
    }

    const auto isFinal = _awaitingResolution;
    _syncErrorService.resolveConflicts(_syncDbId, keepLocal, keepRemote,
                                       [self = QPointer(this), generation = _generation, isFinal](const bool ok) {
                                           if (!isFinal || !self || generation != self->_generation) {
                                               return;
                                           }
                                           self->_awaitingResolution = false;
                                           if (ok) {
                                               self->reset();
                                           } else {
                                               self->_resolutionFailed = true;
                                           }
                                           emit self->changed();
                                       });
}

void ConflictVersionController::reset() {
    ++_generation;
    _errorDbIds.clear();
    _choices.clear();
    _index = 0;
    _awaitingResolution = false;
    _itemName.clear();
    _itemIconName.clear();
    _pendingLoads = 0;
    _loadFailed = false;
    _resolutionFailed = false;
    _selection = Version::None;
    _online = {};
    _local = {};
}

} // namespace KDC
