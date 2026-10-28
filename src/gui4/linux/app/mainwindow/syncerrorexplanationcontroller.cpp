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

#include "app/mainwindow/syncerrorexplanationcontroller.h"

#include "app/cache/mainselectionstore.h"
#include "app/cache/syncerrorsprojection.h"
#include "app/errors/inotifywatchlimit.h"
#include "app/errors/syncerrorcatalog.h"
#include "app/mainwindow/syncerroractionhandler.h"
#include "app/syncconfiguration/localpaths.h"
#include "libcommon/utility/types.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QLoggingCategory>

#include <algorithm>
#include <memory>

namespace KDC {

namespace {

Q_LOGGING_CATEGORY(lcSyncErrorExplanation, "gui.v4.syncerrorexplanation", QtInfoMsg)

// The main button of each dialog.
SyncErrorAction mainAction(const SyncErrorExplanation kind) {
    switch (kind) {
        case SyncErrorExplanation::LocalAccess:
            return SyncErrorAction::OpenParentFolder;
        case SyncErrorExplanation::SyncDirNotFound:
            return SyncErrorAction::OpenSyncParentFolder;
        case SyncErrorExplanation::SyncDirDiskMissing:
        case SyncErrorExplanation::INotifyWatches:
            return SyncErrorAction::RestartSync;
        case SyncErrorExplanation::SyncDirChangedLocally:
        case SyncErrorExplanation::SyncDirChangedRemotely:
            return SyncErrorAction::CreateNewSync;
        case SyncErrorExplanation::None:
        case SyncErrorExplanation::EnumEnd:
            break;
    }
    return SyncErrorAction::None;
}

} // namespace

SyncErrorExplanationController::SyncErrorExplanationController(const SyncErrorsProjection &projection,
                                                               const MainSelectionStore &selectionStore,
                                                               SyncErrorActionHandler &actionHandler, QObject *const parent) :
    QObject(parent),
    _projection(projection),
    _selectionStore(selectionStore),
    _actionHandler(actionHandler) {
    (void) connect(&_projection, &SyncErrorsProjection::currentErrorsChanged, this, [this] {
        if (isOpen() && !currentError()) {
            qCInfo(lcSyncErrorExplanation) << "Explained error gone, closing the dialog | errorDbId:" << _errorDbId.value();
            close();
        }
    });
    // The dialog describes an error of the previous synchronization.
    (void) connect(&_selectionStore, &MainSelectionStore::currentSyncDbIdChanged, this, &SyncErrorExplanationController::close);
}

void SyncErrorExplanationController::open(const ErrorDbId errorDbId) {
    _errorDbId = errorDbId;
    const auto *const classified = currentError();
    const auto kind = classified ? syncErrorTraits(classified->kind).explanation : SyncErrorExplanation::None;
    if (kind == SyncErrorExplanation::None) {
        qCWarning(lcSyncErrorExplanation) << "No explanation for this error | errorDbId:" << errorDbId;
        _errorDbId.reset();
        return;
    }

    _kind = kind;
    _nodeLabelId = QString::fromLatin1(syncErrorNodeLabelId(classified->error.nodeType()));
    _copyablePath = displayLocalPath(absolutePath(*classified));
    const auto &error = classified->error;
    _isDirectory = error.nodeType() == NodeType::Directory;
    _itemName = Path2QStr(error.path().filename());
    _itemPath = Path2QStr(error.path().relative_path().lexically_normal());
    _itemIconName = _itemName.isEmpty() ? QString{} : _fileIconResolver.iconName(_itemName, error.nodeType());
    if (kind == SyncErrorExplanation::INotifyWatches) {
        _watchLimit = readWatchLimit();
        _watchLimitCommand = buildWatchLimitCommand(proposedWatchLimit(_watchLimit));
        qCInfo(lcSyncErrorExplanation) << "Inotify watches limit | current:" << _watchLimit
                                       << "| proposed:" << proposedWatchLimit(_watchLimit);
    }
    qCInfo(lcSyncErrorExplanation) << "Explanation dialog opened | kind:" << classified->kind << "| errorDbId:" << errorDbId;
    emit changed();
}

void SyncErrorExplanationController::close() {
    if (!isOpen()) {
        return;
    }
    _errorDbId.reset();
    emit changed();
}

void SyncErrorExplanationController::triggerMainAction() {
    runAndClose(mainAction(_kind));
}

void SyncErrorExplanationController::createNewSync() {
    runAndClose(SyncErrorAction::CreateNewSync);
}

void SyncErrorExplanationController::copyPath() const {
    const auto *const classified = currentError();
    if (!classified) {
        qCWarning(lcSyncErrorExplanation) << "Path not copied, error unavailable";
        return;
    }
    QGuiApplication::clipboard()->setText(absolutePath(*classified));
    qCInfo(lcSyncErrorExplanation) << "Path copied to the clipboard | errorDbId:" << classified->error.dbId();
}

void SyncErrorExplanationController::copyWatchLimitCommand() const {
    if (_watchLimitCommand.isEmpty()) {
        return;
    }
    QGuiApplication::clipboard()->setText(_watchLimitCommand);
    qCInfo(lcSyncErrorExplanation) << "Inotify watches command copied to the clipboard";
}

// The error, looked up again in the current errors: it can disappear while the dialog is open.
const ClassifiedSyncError *SyncErrorExplanationController::currentError() const {
    if (!_errorDbId.has_value()) {
        return nullptr;
    }
    const auto &errors = _projection.currentErrors();
    const auto classifiedIt = std::ranges::find_if(
            errors, [this](const ClassifiedSyncError &classified) { return classified.error.dbId() == *_errorDbId; });
    return classifiedIt == errors.end() ? nullptr : std::to_address(classifiedIt);
}

void SyncErrorExplanationController::runAndClose(const SyncErrorAction action) {
    const auto *const classified = currentError();
    const auto context = _selectionStore.currentSyncContext();
    if (!classified || !context.has_value()) {
        qCWarning(lcSyncErrorExplanation) << "Explanation action ignored, error unavailable | action:" << action;
        close();
        return;
    }
    _actionHandler.run(action, *classified, context.value());
    close();
}

// The item of a local access error, or the synchronization root for the folder dialogs.
QString SyncErrorExplanationController::absolutePath(const ClassifiedSyncError &classified) const {
    const auto context = _selectionStore.currentSyncContext();
    if (!context.has_value()) {
        return {};
    }
    const auto &syncRoot = context->syncInfo.localPath();
    const SyncPath path =
            _kind == SyncErrorExplanation::LocalAccess ? absoluteSyncPath(syncRoot, classified.error.path()) : syncRoot;
    return Path2QStr(path);
}

} // namespace KDC
