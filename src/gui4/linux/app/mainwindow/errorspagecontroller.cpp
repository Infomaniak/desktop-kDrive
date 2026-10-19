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

#include "app/mainwindow/errorspagecontroller.h"

#include "app/cache/mainselectionstore.h"
#include "app/cache/syncerrorsprojection.h"
#include "app/mainwindow/syncerroractionhandler.h"
#include "app/navigation/activitiesnavigation.h"
#include "app/services/syncerrorservice.h"
#include "libcommon/utility/types.h"

#include <QLoggingCategory>

#include <algorithm>

namespace KDC {

namespace {

Q_LOGGING_CATEGORY(lcErrorsPageController, "gui.v4.errorspage", QtInfoMsg)

} // namespace

ErrorsPageController::ErrorsPageController(const SyncErrorsProjection &projection, const MainSelectionStore &selectionStore,
                                           const SyncService &syncService, SyncErrorService &syncErrorService,
                                           SyncErrorActionHandler &actionHandler, ActivitiesNavigation &activitiesNavigation,
                                           QObject *const parent) :
    QObject(parent),
    _projection(projection),
    _selectionStore(selectionStore),
    _syncErrorService(syncErrorService),
    _actionHandler(actionHandler),
    _activitiesNavigation(activitiesNavigation),
    _model(projection, selectionStore, syncService, syncErrorService),
    _errorCount(projection.currentErrorCount()) {
    (void) connect(&_projection, &SyncErrorsProjection::currentErrorsChanged, this, [this] {
        if (const auto errorCount = _projection.currentErrorCount(); errorCount != _errorCount) {
            _errorCount = errorCount;
            emit errorCountChanged();
        }
    });
    (void) connect(&_syncErrorService, &SyncErrorService::refreshPendingChanged, this, [this](const qint64 syncDbId) {
        if (syncDbId == _selectionStore.currentSyncDbId()) {
            emit refreshPendingChanged();
        }
    });
    // Another synchronization may have its own refresh in flight.
    (void) connect(&_selectionStore, &MainSelectionStore::currentSyncDbIdChanged, this,
                   &ErrorsPageController::refreshPendingChanged);
}

int32_t ErrorsPageController::errorCount() const {
    return _errorCount;
}

bool ErrorsPageController::refreshPending() const {
    return _syncErrorService.isRefreshPending(_selectionStore.currentSyncDbId());
}

void ErrorsPageController::refresh() {
    const auto syncDbId = _selectionStore.currentSyncDbId();
    qCInfo(lcErrorsPageController) << "Refreshing the sync errors | syncDbId:" << syncDbId << "| errorCount:" << _errorCount;
    _syncErrorService.refreshErrors(syncDbId);
}

void ErrorsPageController::invokeAction(const QString &rowId) {
    const auto *const classified = currentError(rowId);
    const auto context = _selectionStore.currentSyncContext();
    if (!classified || !context.has_value()) {
        qCWarning(lcErrorsPageController) << "Sync error action ignored, error unavailable | rowId:" << rowId;
        return;
    }
    _actionHandler.trigger(*classified, context.value());
}

// The error of a row, looked up again in the current errors: a row can disappear between its display and a click.
const ClassifiedSyncError *ErrorsPageController::currentError(const QString &rowId) const {
    const auto errorDbId = _model.errorDbId(rowId);
    if (!errorDbId.has_value()) {
        return nullptr;
    }

    const auto &errors = _projection.currentErrors();
    const auto classifiedIt = std::ranges::find_if(
            errors, [&errorDbId](const ClassifiedSyncError &classified) { return classified.error.dbId() == *errorDbId; });
    return classifiedIt == errors.end() ? nullptr : &*classifiedIt;
}

void ErrorsPageController::showActivities() {
    qCInfo(lcErrorsPageController) << "Back to the activity list from the empty errors page";
    _activitiesNavigation.show(ActivitiesNavigation::Page::List);
}

} // namespace KDC
