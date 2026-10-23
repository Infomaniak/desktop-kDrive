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

#include "app/cache/syncerrorsprojection.h"

#include "app/cache/appcache.h"
#include "app/cache/mainselectionstore.h"
#include "app/errors/blockingerrorresolver.h"
#include "app/errors/syncerrorcatalog.h"
#include "app/errors/syncerrorclassifier.h"
#include "app/services/sentryservice.h"

#include <QLoggingCategory>

#include <algorithm>
#include <chrono>
#include <ranges>
#include <unordered_set>

namespace KDC {

namespace {

Q_LOGGING_CATEGORY(lcSyncErrorsProjection, "gui.v4.syncerrors", QtInfoMsg)

// When the server detects an error it already knows, it keeps its dbId, updates its time and signals UTILITY_ERROR_REMOVED
// then UTILITY_ERROR_ADDED. Each signal is sent on its own and may be read separately: the window merges the pair into one
// refresh, so the error neither disappears for a moment (blocking error, tray) nor is forgotten and logged again. The two
// signals of a pair were observed up to about 100 ms apart on reception.
constexpr auto refreshInterval = std::chrono::milliseconds{150};

std::string blockingErrorName(const std::optional<BlockingErrorKind> kind) {
    return kind ? toString(*kind) : "none";
}

// Logs a new or reclassified error. File paths are never logged.
void logClassification(const ClassifiedSyncError &classified) {
    const Error &error = classified.error;
    if (classified.kind == SyncErrorKind::Unknown) {
        qCWarning(lcSyncErrorsProjection) << "Unclassified sync error | dbId:" << error.dbId()
                                          << "/ syncDbId:" << error.syncDbId() << "/ level:" << error.level()
                                          << "/ exitCode:" << error.exitCode() << "/ exitCause:" << error.exitCause()
                                          << "/ nodeType:" << error.nodeType() << "/ cancelType:" << error.cancelType()
                                          << "/ inconsistencyType:" << error.inconsistencyType()
                                          << "/ conflictType:" << error.conflictType();
        // One issue per code and cause, whatever the user: tells which unexpected errors deserve a kind of their own.
        // The simulator's errors (negative ids, real ones are SQLite rowids) are not reported.
        if (error.dbId() > 0) {
            SentryService::reportWarning(
                    "Unclassified sync error: " + toString(error.exitCode()) + " / " + toString(error.exitCause()),
                    "level: " + toString(error.level()) + ", cancelType: " + toString(error.cancelType()) +
                            ", inconsistencyType: " + toString(error.inconsistencyType()) +
                            ", conflictType: " + toString(error.conflictType()) + ", function: " + error.functionName() +
                            ", worker: " + error.workerName());
        }
        return;
    }

    qCInfo(lcSyncErrorsProjection) << "Sync error classified | dbId:" << error.dbId() << "/ syncDbId:" << error.syncDbId()
                                   << "/ kind:" << classified.kind << "/ category:" << static_cast<int32_t>(classified.category)
                                   << "/ level:" << error.level() << "/ exitCode:" << error.exitCode()
                                   << "/ exitCause:" << error.exitCause();
}

} // namespace

SyncErrorsProjection::SyncErrorsProjection(const AppCache &cache, const MainSelectionStore &selectionStore,
                                           QObject *const parent) :
    QObject(parent),
    _cache(cache),
    _selectionStore(selectionStore) {
    // Started by the first signal and never pushed back, so a refresh happens at most refreshInterval after a change.
    _refreshTimer.setInterval(refreshInterval);
    _refreshTimer.setSingleShot(true);
    (void) connect(&_refreshTimer, &QTimer::timeout, this, &SyncErrorsProjection::refresh);

    (void) connect(&_cache, &AppCache::syncErrorsChanged, this, &SyncErrorsProjection::scheduleRefresh);
    (void) connect(&_cache, &AppCache::serverErrorsChanged, this, &SyncErrorsProjection::scheduleRefresh);
    // A selection change is applied at once, or the newly selected synchronization would show the previous one's errors
    // (blocking screen included) for up to refreshInterval. A pending coalesced refresh still runs afterwards.
    (void) connect(&_selectionStore, &MainSelectionStore::currentSyncDbIdChanged, this, &SyncErrorsProjection::refresh);
    // Also covers a change of the connection state of the selected synchronization's user.
    (void) connect(&_selectionStore, &MainSelectionStore::currentContextChanged, this, &SyncErrorsProjection::scheduleRefresh);

    refresh();
}

std::vector<ErrorDbId> SyncErrorsProjection::currentConflictIds() const {
    std::vector<ErrorDbId> conflictIds;
    for (const auto &classified: _currentErrors) {
        if (isUserResolvableConflict(classified.error)) {
            conflictIds.push_back(classified.error.dbId());
        }
    }

    (void) std::ranges::sort(conflictIds);
    return conflictIds;
}

void SyncErrorsProjection::scheduleRefresh() {
    if (!_refreshTimer.isActive()) {
        _refreshTimer.start();
    }
}

void SyncErrorsProjection::refresh() {
    const std::vector<Error> syncErrors = _cache.syncErrors();

    refreshClassifications(syncErrors);
    refreshCurrentErrors();
    refreshTrayRelevance();
    refreshUpdateRequired(syncErrors);
}

// Classifies the new and changed errors, and forgets the removed ones.
void SyncErrorsProjection::refreshClassifications(const std::vector<Error> &syncErrors) {
    std::unordered_set<ErrorDbId> presentDbIds;
    presentDbIds.reserve(syncErrors.size());

    for (const auto &error: syncErrors) {
        (void) presentDbIds.insert(error.dbId());

        const auto known = _classifiedByDbId.find(error.dbId());
        if (known != _classifiedByDbId.end() && known->second.error == error) {
            continue;
        }

        // A known error also changes when the server detects it again (new time, same dbId): it is updated without a
        // new log line unless its kind changed.
        const SyncErrorKind kind = classifySyncError(error);
        const bool isNewOrReclassified = known == _classifiedByDbId.end() || known->second.kind != kind;
        ClassifiedSyncError classified{error, kind, categorizeSyncError(error, kind)};
        if (isNewOrReclassified) {
            logClassification(classified);
        }
        (void) _classifiedByDbId.insert_or_assign(error.dbId(), std::move(classified));
    }

    (void) std::erase_if(_classifiedByDbId, [&presentDbIds](const auto &entry) { return !presentDbIds.contains(entry.first); });
}

void SyncErrorsProjection::refreshCurrentErrors() {
    const auto syncDbId = static_cast<SyncDbId>(_selectionStore.currentSyncDbId());
    const std::vector<Error> syncErrors = syncDbId > 0 ? _cache.errorsForSync(syncDbId) : std::vector<Error>{};

    std::vector<ClassifiedSyncError> currentErrors;
    currentErrors.reserve(syncErrors.size());
    for (const auto &error: syncErrors) {
        if (const auto known = _classifiedByDbId.find(error.dbId()); known != _classifiedByDbId.end()) {
            currentErrors.push_back(known->second);
        }
    }

    if (currentErrors != _currentErrors) {
        _currentErrors = std::move(currentErrors);
        emit currentErrorsChanged();
    }

    std::optional<BlockingErrorKind> blockingError;
    if (const auto context = _selectionStore.currentSyncContext(); context) {
        blockingError = resolveBlockingError(syncErrors, context->userDisplayInfo.connected());
    }

    if (blockingError != _currentBlockingError) {
        qCInfo(lcSyncErrorsProjection) << "Blocking error of the selected sync changed | syncDbId:" << syncDbId
                                       << "/ from:" << blockingErrorName(_currentBlockingError).c_str()
                                       << "/ to:" << blockingErrorName(blockingError).c_str();
        _currentBlockingError = blockingError;
        emit currentBlockingErrorChanged();
    }
}

void SyncErrorsProjection::refreshTrayRelevance() {
    const bool hasTrayRelevantError = std::ranges::any_of(
            _classifiedByDbId | std::views::values,
            [](const ClassifiedSyncError &classified) { return syncErrorTraits(classified.kind).showsInTray; });

    if (hasTrayRelevantError != _hasTrayRelevantError) {
        _hasTrayRelevantError = hasTrayRelevantError;
        emit trayRelevanceChanged();
    }
}

void SyncErrorsProjection::refreshUpdateRequired(const std::vector<Error> &syncErrors) {
    // Windows handles an `UpdateRequired` error of any level as a server error: sync errors are checked as well.
    const bool updateRequired = KDC::isUpdateRequired(_cache.serverErrors()) || KDC::isUpdateRequired(syncErrors);

    if (updateRequired != _updateRequired) {
        qCInfo(lcSyncErrorsProjection) << "Update required changed | required:" << updateRequired;
        _updateRequired = updateRequired;
        emit updateRequiredChanged();
    }
}

} // namespace KDC
