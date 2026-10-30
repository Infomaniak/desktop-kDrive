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

#include "app/mainwindow/conflictspagecontroller.h"

#include "app/cache/mainselectionstore.h"
#include "app/cache/syncerrorsprojection.h"
#include "app/mainwindow/conflictversioncontroller.h"
#include "app/services/syncerrorservice.h"

#include <QLoggingCategory>
#include <QPointer>

#include <algorithm>


namespace KDC {

namespace {

Q_LOGGING_CATEGORY(lcConflictsPage, "gui.v4.conflictspage", QtInfoMsg)

ConflictResolutionStrategy resolutionStrategy(const ConflictsPageController::Strategy strategy) {
    switch (strategy) {
        case ConflictsPageController::Strategy::MostRecent:
            return ConflictResolutionStrategy::KeepMostRecent;
        case ConflictsPageController::Strategy::Online:
            return ConflictResolutionStrategy::KeepRemote;
        case ConflictsPageController::Strategy::Local:
            return ConflictResolutionStrategy::KeepLocal;
    }
    return ConflictResolutionStrategy::KeepMostRecent;
}

bool isConflictsPage(const ActivitiesNavigation::Page page) {
    return page == ActivitiesNavigation::Page::ConflictsQuick || page == ActivitiesNavigation::Page::ConflictsList;
}

} // namespace

ConflictsPageController::ConflictsPageController(const SyncErrorsProjection &projection, const MainSelectionStore &selectionStore,
                                                 SyncErrorService &syncErrorService, ConflictVersionController &conflictVersion,
                                                 ActivitiesNavigation &activitiesNavigation, QObject *const parent) :
    QObject(parent),
    _projection(projection),
    _selectionStore(selectionStore),
    _syncErrorService(syncErrorService),
    _conflictVersion(conflictVersion),
    _activitiesNavigation(activitiesNavigation),
    _list(projection, selectionStore, syncErrorService),
    _count(static_cast<int32_t>(conflictDbIds().size())),
    _page(activitiesNavigation.currentPage()) {
    (void) connect(&_activitiesNavigation, &ActivitiesNavigation::currentPageChanged, this,
                   &ConflictsPageController::onPageChanged);
    (void) connect(&_projection, &SyncErrorsProjection::currentErrorsChanged, this, &ConflictsPageController::onErrorsChanged);
    (void) connect(&_selectionStore, &MainSelectionStore::currentSyncDbIdChanged, this, &ConflictsPageController::onSyncChanged);
    (void) connect(&_syncErrorService, &SyncErrorService::conflictResolutionPendingChanged, this, [this](const qint64 syncDbId) {
        if (syncDbId == _selectionStore.currentSyncDbId()) {
            emit changed();
        }
    });
}

bool ConflictsPageController::resolutionPending() const {
    return _syncErrorService.isConflictResolutionPending(_selectionStore.currentSyncDbId());
}

void ConflictsPageController::selectStrategy(const Strategy strategy) {
    if (_strategy == strategy && !_failed) {
        return;
    }
    _strategy = strategy;
    _failed = false;
    emit changed();
}

void ConflictsPageController::apply() {
    const auto errorDbIds = conflictDbIds();
    if (errorDbIds.empty() || resolutionPending()) {
        return;
    }

    const auto syncDbId = _selectionStore.currentSyncDbId();
    qCInfo(lcConflictsPage) << "Grouped choice requested | syncDbId:" << syncDbId << "| strategy:" << _strategy
                            << "| count:" << errorDbIds.size();
    _failed = false;
    _awaitingResolution = true;
    _syncErrorService.resolveConflictsQuick(syncDbId, errorDbIds, resolutionStrategy(_strategy),
                                            [self = QPointer(this), generation = _generation](const bool ok) {
                                                if (!self || generation != self->_generation) {
                                                    return;
                                                }
                                                self->_awaitingResolution = false;
                                                if (ok) {
                                                    self->leave("grouped choice applied");
                                                } else {
                                                    self->_failed = true;
                                                }
                                                emit self->changed();
                                            });
    emit changed();
}

void ConflictsPageController::manageIndividually() {
    _activitiesNavigation.show(ActivitiesNavigation::Page::ConflictsList);
}

void ConflictsPageController::startChoices() {
    auto errorDbIds = _list.errorDbIds();
    if (errorDbIds.empty()) {
        qCWarning(lcConflictsPage) << "Version selection ignored, no conflict left";
        return;
    }
    qCInfo(lcConflictsPage) << "Version selection of every conflict requested | count:" << errorDbIds.size();
    _conflictVersion.open(std::move(errorDbIds));
}

void ConflictsPageController::chooseVersion(const QString &rowId) {
    const auto errorDbId = _list.errorDbId(rowId);
    if (!errorDbId.has_value()) {
        qCWarning(lcConflictsPage) << "Version selection ignored, conflict unavailable | rowId:" << rowId;
        return;
    }
    _conflictVersion.open({errorDbId.value()});
}

bool ConflictsPageController::isShown() const {
    return isConflictsPage(_activitiesNavigation.currentPage());
}

std::vector<ErrorDbId> ConflictsPageController::conflictDbIds() const {
    std::vector<ErrorDbId> errorDbIds;
    for (const auto &classified: _projection.currentErrors()) {
        if (classified.category == SyncErrorCategory::Conflicts) {
            errorDbIds.push_back(classified.error.dbId());
        }
    }
    return errorDbIds;
}

// Coming from the errors page starts a new grouped choice; coming back from the list keeps it. Leaving the list clears its
// search.
void ConflictsPageController::onPageChanged() {
    const auto previousPage = _page;
    _page = _activitiesNavigation.currentPage();
    if (_page == ActivitiesNavigation::Page::ConflictsQuick && !isConflictsPage(previousPage)) {
        _strategy = Strategy::MostRecent;
        _failed = false;
        emit changed();
    }
    if (previousPage == ActivitiesNavigation::Page::ConflictsList && _page != ActivitiesNavigation::Page::ConflictsList) {
        _list.setSearchText({});
    }
}

void ConflictsPageController::onErrorsChanged() {
    const auto conflictCount = static_cast<int32_t>(conflictDbIds().size());
    if (conflictCount != _count) {
        _count = conflictCount;
        emit changed();
    }
    // A grouped choice removes the errors possibly before its response, which leaves the page itself.
    if (_count == 0 && !_awaitingResolution && isShown()) {
        leave("no conflict left");
    }
}

void ConflictsPageController::onSyncChanged() {
    ++_generation;
    _awaitingResolution = false;
    _failed = false;
    _count = static_cast<int32_t>(conflictDbIds().size());
    if (isShown()) {
        showErrors("synchronization changed");
    }
    emit changed();
}

void ConflictsPageController::showErrors(const char *const reason) {
    if (!isShown()) {
        return;
    }
    qCInfo(lcConflictsPage) << "Back to the errors page |" << reason;
    _activitiesNavigation.show(ActivitiesNavigation::Page::Errors);
}

// Once the conflicts are resolved, the errors page only matters for the other errors. The conflicts are not counted: the
// server may remove them before or after its response.
void ConflictsPageController::leave(const char *const reason) {
    if (!isShown()) {
        return;
    }
    const auto &errors = _projection.currentErrors();
    if (std::ranges::any_of(errors, [](const ClassifiedSyncError &classified) {
            return classified.category != SyncErrorCategory::Conflicts;
        })) {
        showErrors(reason);
        return;
    }
    qCInfo(lcConflictsPage) << "Back to the activity list, no other error |" << reason;
    _activitiesNavigation.show(ActivitiesNavigation::Page::List);
}

} // namespace KDC
