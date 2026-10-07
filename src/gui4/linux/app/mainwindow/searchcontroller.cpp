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

#include "app/mainwindow/searchcontroller.h"

#include "app/appconstants.h"
#include "app/cache/mainselectionstore.h"
#include "app/services/commservice.h"
#include "app/syncconfiguration/localpaths.h"

#include <QDesktopServices>
#include <QLoggingCategory>
#include <QPointer>
#include <QUrl>

#include <chrono>
#include <optional>

using namespace Qt::StringLiterals;

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcSearchController, "gui.v4.searchcontroller", QtInfoMsg)

// Leaves time to type a few characters before sending a request.
constexpr std::chrono::milliseconds searchDebounceDelay{300};
} // namespace

SearchController::SearchController(const CommService &commService, const MainSelectionStore &selectionStore,
                                   QObject *const parent) :
    QObject(parent),
    _commService(commService),
    _selectionStore(selectionStore) {
    _debounceTimer.setSingleShot(true);
    _debounceTimer.setInterval(searchDebounceDelay);
    (void) connect(&_debounceTimer, &QTimer::timeout, this, &SearchController::startSearch);

    (void) connect(&_selectionStore, &MainSelectionStore::currentSyncDbIdChanged, this,
                   &SearchController::handleSelectionChanged);
    (void) connect(&_selectionStore, &MainSelectionStore::currentContextChanged, this, &SearchController::refreshSelection);

    refreshSelection();
}

void SearchController::setQuery(const QString &query) {
    if (query == _query) {
        return;
    }

    _query = query;
    emit queryChanged();

    if (_query.trimmed().isEmpty()) {
        reset();
        return;
    }

    _debounceTimer.start();
}

bool SearchController::canLoadMore() const {
    return _state == State::Results && _hasMore && !_cursor.empty();
}

void SearchController::open() {
    refreshSelection();
    setQuery({});
    reset();
}

void SearchController::close() {
    setQuery({});
    reset();
}

void SearchController::retry() {
    if (_state == State::Error) {
        startSearch();
        return;
    }

    if (_nextPageFailed) {
        _nextPageFailed = false;
        emit paginationChanged();
        loadMore();
    }
}

void SearchController::loadMore() {
    // After a failed page, only an explicit retry requests it again: scrolling would otherwise loop on a lasting failure.
    if (!canLoadMore() || _loadingMore || _nextPageFailed) {
        return;
    }

    _loadingMore = true;
    _nextPageFailed = false;
    emit paginationChanged();

    requestPage(false);
}

void SearchController::openResult(const int row, const bool revealInFolder) const {
    const auto result = _model.result(row);
    const auto context = _selectionStore.currentSyncContext();
    if (!result.has_value() || !context.has_value()) {
        qCWarning(lcSearchController) << "Search result opening rejected for unknown row or synchronization | row:" << row;
        return;
    }

    if (!result->isAvailableLocally()) {
        const QUrl url = AppConstants::WebDrive::itemRedirectUri(context->drive.driveId(), result->id());
        qCInfo(lcSearchController) << "Opening search result online | nodeId:" << QString::fromStdString(result->id());
        if (!QDesktopServices::openUrl(url)) {
            qCWarning(lcSearchController) << "Desktop service failed to open search result online | url:" << url;
        }
        return;
    }

    // An empty path is the sync root itself (the target folder of an advanced sync). The result may have been moved or
    // deleted locally since the search: the resolution then fails and is only logged.
    const auto localPath = resolveExistingPathBelowSyncRoot(context->syncInfo.localPath(), result->path());
    if (!localPath.has_value()) {
        qCWarning(lcSearchController) << "Local search result is missing or outside the synchronization root"
                                      << "| path:" << Path2QStr(result->path());
        return;
    }

    // Revealing the sync root itself opens it rather than its parent, which lies outside of the synchronization.
    const bool revealParent = revealInFolder && !result->path().empty();
    const SyncPath pathToOpen = revealParent ? localPath.value().parent_path() : localPath.value();
    qCInfo(lcSearchController) << "Opening search result locally | nodeId:" << QString::fromStdString(result->id())
                               << "| revealInFolder:" << revealInFolder;
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(Path2QStr(pathToOpen)))) {
        qCWarning(lcSearchController) << "Desktop service failed to open local search result"
                                      << "| revealInFolder:" << revealInFolder << "| path:" << Path2QStr(pathToOpen);
    }
}

void SearchController::retranslate() {
    _model.retranslate();
}

/**
 * Drops the displayed results and every pending request, then returns to the Idle state. The query is left untouched:
 * callers clear it beforehand when needed.
 */
void SearchController::reset() {
    _debounceTimer.stop();
    ++_requestGeneration;

    _model.clear();
    _activeSyncDbId = 0;
    _activeQuery.clear();
    _hasMore = false;
    _cursor.clear();
    _loadingMore = false;
    _nextPageFailed = false;
    emit paginationChanged();

    setState(State::Idle);
}

/**
 * Sends the first page of the current query. Results of the previous query stay visible while it loads, so that typing
 * does not make the list flicker.
 */
void SearchController::startSearch() {
    _debounceTimer.stop();

    const QString query = _query.trimmed();
    const SyncDbId syncDbId = _selectionStore.currentSyncDbId();
    if (query.isEmpty() || syncDbId == 0) {
        reset();
        return;
    }

    _activeSyncDbId = syncDbId;
    _activeQuery = query;
    _hasMore = false;
    _cursor.clear();
    _loadingMore = false;
    _nextPageFailed = false;
    emit paginationChanged();

    setState(State::Loading);
    requestPage(true);
}

/**
 * Sends the first page of the active request, or its next page with the last cursor. Only the first page starts a new
 * generation: a next page shares the generation of the results it extends, so a newer query discards it as well.
 */
void SearchController::requestPage(const bool firstPage) {
    if (firstPage) {
        ++_requestGeneration;
    }

    const uint64_t generation = _requestGeneration;
    const SyncDbId syncDbId = _activeSyncDbId;
    const std::string cursor = firstPage ? std::string{} : _cursor;
    if (firstPage) {
        qCInfo(lcSearchController) << "Drive search requested | syncDbId:" << syncDbId << "| generation:" << generation
                                   << "| query:" << _activeQuery;
    } else {
        qCDebug(lcSearchController) << "Drive search next page requested | syncDbId:" << syncDbId << "| generation:" << generation
                                    << "| loadedRows:" << _model.rowCount();
    }

    const QPointer<SearchController> guard(this);
    _commService.requestDriveSearch(
            syncDbId, _activeQuery, cursor,
            [guard, generation, syncDbId, firstPage](const ExitInfo &exitInfo, const DriveSearchResult &searchResult) {
                if (!guard) {
                    return;
                }

                if (generation != guard->_requestGeneration) {
                    qCDebug(lcSearchController) << "Outdated drive search response ignored | generation:" << generation
                                                << "| currentGeneration:" << guard->_requestGeneration;
                    return;
                }

                if (!exitInfo) {
                    qCWarning(lcSearchController) << "Drive search failed | syncDbId:" << syncDbId
                                                  << "| generation:" << generation << "| firstPage:" << firstPage
                                                  << "| exitCode:" << exitInfo.code() << "| exitCause:" << exitInfo.cause();
                    if (firstPage) {
                        guard->_model.clear();
                        guard->setState(State::Error);
                    } else {
                        guard->_loadingMore = false;
                        guard->_nextPageFailed = true;
                        emit guard->paginationChanged();
                    }
                    return;
                }

                const int32_t addedRows = firstPage ? guard->_model.replace(searchResult.searchInfoList)
                                                    : guard->_model.appendPage(searchResult.searchInfoList);
                qCDebug(lcSearchController) << "Drive search page received | generation:" << generation
                                            << "| firstPage:" << firstPage
                                            << "| receivedRows:" << searchResult.searchInfoList.size()
                                            << "| addedRows:" << addedRows << "| totalRows:" << guard->_model.rowCount()
                                            << "| hasMore:" << searchResult.hasMore;

                guard->_hasMore = searchResult.hasMore;
                guard->_cursor = searchResult.cursor;
                guard->_loadingMore = false;
                guard->setState(guard->_model.rowCount() > 0 ? State::Results : State::Empty);
                emit guard->paginationChanged();
            });
}

/** Restarts the current query on the newly selected synchronization, whose drive may differ. */
void SearchController::handleSelectionChanged() {
    refreshSelection();

    if (_query.trimmed().isEmpty()) {
        reset();
        return;
    }

    _model.clear();
    startSearch();
}

void SearchController::refreshSelection() {
    const auto context = _selectionStore.currentSyncContext();

    if (const bool available = context.has_value(); available != _available) {
        _available = available;
        emit availableChanged();
    }

    if (const QString driveName = context.has_value() ? QString::fromStdString(context->drive.name()) : QString{};
        driveName != _driveName) {
        _driveName = driveName;
        emit driveNameChanged();
    }
}

void SearchController::setState(const State state) {
    if (state == _state) {
        return;
    }

    _state = state;
    emit stateChanged();
    // `canLoadMore` only applies to displayed results.
    emit paginationChanged();
}

} // namespace KDC
