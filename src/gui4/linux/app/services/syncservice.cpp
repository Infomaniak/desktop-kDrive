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

#include "syncservice.h"

#include "app/cache/appcache.h"
#include "app/services/cachepopulator.h"
#include "libcommon/utility/types.h"

#include <QLoggingCategory>

#include <optional>
#include <utility>

namespace {
constexpr char serviceKeySync[] = "sync";
constexpr char actionStartSync[] = "startSync";
constexpr char actionStopSync[] = "stopSync";
constexpr char actionStartSyncsAfterLogin[] = "startSyncsAfterLogin";
constexpr char actionDeleteSync[] = "deleteSync";
constexpr char actionQuerySyncStatus[] = "querySyncStatus";
constexpr char actionFindGoodPathForNewSync[] = "findGoodPathForNewSync";
constexpr char actionIsPathValidForNewSync[] = "isPathValidForNewSync";
constexpr char actionCreateRemoteFolder[] = "createRemoteFolder";

Q_LOGGING_CATEGORY(lcSyncService, "gui.v4.syncservice", QtInfoMsg)
} // namespace

namespace KDC {

SyncService::SyncService(CommService &commService, AppCache &appCache, CachePopulator &cachePopulator,
                         ServiceActionTracker &serviceActionTracker, ServiceEventBus &serviceEventBus, QObject *const parent) :
    QObject(parent),
    _commService(commService),
    _appCache(appCache),
    _cachePopulator(cachePopulator),
    _serviceActionTracker(serviceActionTracker),
    _serviceEventBus(serviceEventBus) {
    (void) connect(&_appCache, &AppCache::syncsChanged, this, &SyncService::releaseSyncedReservations);
    (void) connect(&_cachePopulator, &CachePopulator::reconciliationCompleted, this, &SyncService::releaseReconciledReservations);
    (void) connect(&_serviceActionTracker, &ServiceActionTracker::servicePendingChanged, this,
                   [this](const ServiceActionTracker::ServiceKey &serviceKey, const bool) {
                       if (serviceKey == serviceKeySync) {
                           emit loadingChanged();
                       }
                   });
    (void) connect(&_serviceActionTracker, &ServiceActionTracker::actionPendingChanged, this,
                   [this](const ServiceActionTracker::ServiceKey &serviceKey, const ServiceActionTracker::ActionKey &actionKey,
                          const ServiceActionTracker::ScopeId scopeId, const bool) {
                       if (serviceKey == serviceKeySync &&
                           (actionKey == actionStartSync || actionKey == actionStopSync || actionKey == actionDeleteSync)) {
                           emit syncActionPendingChanged(scopeId);
                       }
                   });
}

bool SyncService::loading() const {
    return _serviceActionTracker.isServicePending(serviceKeySync);
}

bool SyncService::addDriveSync(const SyncAddRequest &request, const CommService::SyncInfoCallback &callback) {
    if (!request.serverFolderNodeId.empty()) { // Advanced sync: allowed next to a classic one, no reservation.
        _commService.requestSyncAdd(request, callback);
        return true;
    }

    const AvailableDriveKey key{.userDbId = request.userDbId, .accountId = request.accountId, .driveId = request.driveId};
    if (_appCache.isAvailableDriveConfigured(key) || _appCache.isSyncCreationPending(key)) {
        qCWarning(lcSyncService) << "Classic sync creation refused: drive already synchronized or being synchronized | userDbId:"
                                 << key.userDbId << "/ accountId:" << key.accountId << "/ driveId:" << key.driveId;
        return false;
    }

    _appCache.setSyncCreationPending(key, true);
    _commService.requestSyncAdd(request, [this, key, callback](const ExitInfo &exitInfo, const BaseSync &syncInfo) {
        if (exitInfo) {
            _awaitingSyncPush[key] = syncInfo.dbId();
            releaseSyncedReservations();
        } else {
            (void) _awaitingReconciliation.insert(key);
            _cachePopulator.reconcile();
        }

        callback(exitInfo, syncInfo);
    });
    return true;
}

void SyncService::createRemoteFolder(const UserDbId userDbId, const DriveId driveId, const NodeId &parentNodeId,
                                     const QString &name, const CommService::NodeIdCallback &callback) {
    beginAction(actionCreateRemoteFolder);

    _commService.requestNodeCreateMissingFolders(userDbId, driveId, parentNodeId, QStr2Path(name),
                                                 [this, callback](const ExitInfo &exitInfo, const NodeId &nodeId) {
                                                     endAction(actionCreateRemoteFolder);
                                                     if (!exitInfo) {
                                                         notifyRequestFailure(exitInfo, RequestNum::NODE_CREATEMISSINGFOLDERS);
                                                     }

                                                     callback(exitInfo, nodeId);
                                                 });
}

void SyncService::startSync(const qint64 syncDbId) {
    startSyncs({static_cast<SyncDbId>(syncDbId)});
}

void SyncService::startSyncs(const std::vector<SyncDbId> &syncDbIds) {
    controlSyncs(SyncControlRequest::Start, syncDbIds);
}

void SyncService::startSyncsAfterLogin(const UserDbId userDbId) {
    if (isActionPending(actionStartSyncsAfterLogin, userDbId)) {
        qCWarning(lcSyncService) << "Sync restart after login ignored: already pending | userDbId:" << userDbId;
        return;
    }

    qCInfo(lcSyncService) << "Restarting synchronizations after login | userDbId:" << userDbId;
    beginAction(actionStartSyncsAfterLogin, userDbId);
    _commService.requestStartSyncsAfterLogin(userDbId, [this, userDbId](const ExitInfo &exitInfo) {
        endAction(actionStartSyncsAfterLogin, userDbId);
        if (!exitInfo) {
            notifyRequestFailure(exitInfo, RequestNum::SYNC_START_AFTER_LOGIN);
            return;
        }
        qCInfo(lcSyncService) << "Synchronizations restarted after login | userDbId:" << userDbId;
    });
}

void SyncService::stopSync(const qint64 syncDbId) {
    stopSyncs({static_cast<SyncDbId>(syncDbId)});
}

void SyncService::stopSyncs(const std::vector<SyncDbId> &syncDbIds) {
    controlSyncs(SyncControlRequest::Stop, syncDbIds);
}

void SyncService::deleteSync(const qint64 syncDbId) {
    deleteSync(syncDbId, {});
}

void SyncService::deleteSync(const SyncDbId syncDbId, const CommService::VoidCallback &callback) {
    beginAction(actionDeleteSync, syncDbId);

    // Cache consistency is signal-driven: we wait for syncRemoved/syncUpdated pushes.
    _commService.requestSyncDelete(syncDbId, [this, syncDbId, callback](const ExitInfo &exitInfo) {
        endAction(actionDeleteSync, syncDbId);
        if (!exitInfo) {
            notifyRequestFailure(exitInfo, RequestNum::SYNC_DELETE);
        }

        if (callback) {
            callback(exitInfo);
        }
    });
}

void SyncService::querySyncStatus(const qint64 syncDbId) {
    beginAction(actionQuerySyncStatus, syncDbId);

    _commService.requestSyncStatus(syncDbId, [this, syncDbId](const ExitInfo &exitInfo, const SyncStatus status) {
        endAction(actionQuerySyncStatus, syncDbId);
        if (!exitInfo) {
            notifyRequestFailure(exitInfo, RequestNum::SYNC_STATUS);
            return;
        }

        emit syncStatusReceived(syncDbId, toInt(status));
    });
}

void SyncService::findGoodPathForNewSync(const QString &basePath) {
    beginAction(actionFindGoodPathForNewSync);
    const auto generation = ++_findGoodPathGeneration;

    _commService.requestFindGoodPathForNewSync(QStr2Path(basePath),
                                               [this, generation](const ExitInfo &exitInfo, const GoodPathResult &result) {
                                                   endAction(actionFindGoodPathForNewSync);
                                                   if (generation != _findGoodPathGeneration) {
                                                       return;
                                                   }

                                                   if (!exitInfo) {
                                                       notifyRequestFailure(exitInfo, RequestNum::UTILITY_FINDGOODPATHFORNEWSYNC);
                                                       return;
                                                   }

                                                   emit suggestedPathReceived(Path2QStr(result.goodPath), result.errorMessage);
                                               });
}

void SyncService::isPathValidForNewSync(const QString &path, const int32_t syncConfiguration) {
    const auto generation = ++_pathValidationGeneration;

    if (!isValidSyncConfigurationValue(syncConfiguration)) {
        emit pathValidationReceived(false);
        return;
    }

    beginAction(actionIsPathValidForNewSync);

    _commService.requestIsPathValidForNewSync(QStr2Path(path), static_cast<SyncConfiguration>(syncConfiguration),
                                              [this, generation](const ExitInfo &exitInfo, const bool isValid) {
                                                  endAction(actionIsPathValidForNewSync);
                                                  if (generation != _pathValidationGeneration) {
                                                      return;
                                                  }

                                                  if (!exitInfo) {
                                                      notifyRequestFailure(exitInfo, RequestNum::UTILITY_ISPATHVALIDFORNEWSYNC);
                                                      emit pathValidationReceived(false);
                                                      return;
                                                  }

                                                  emit pathValidationReceived(isValid);
                                              });
}

bool SyncService::isStartSyncPending(const qint64 syncDbId) const {
    return isActionPending(actionStartSync, syncDbId);
}

bool SyncService::isStopSyncPending(const qint64 syncDbId) const {
    return isActionPending(actionStopSync, syncDbId);
}

bool SyncService::isDeleteSyncPending(const qint64 syncDbId) const {
    return isActionPending(actionDeleteSync, syncDbId);
}

bool SyncService::isQuerySyncStatusPending(const qint64 syncDbId) const {
    return isActionPending(actionQuerySyncStatus, syncDbId);
}

bool SyncService::isFindGoodPathForNewSyncPending() const {
    return isActionPending(actionFindGoodPathForNewSync);
}

bool SyncService::isPathValidForNewSyncPending() const {
    return isActionPending(actionIsPathValidForNewSync);
}

void SyncService::beginAction(const ServiceActionTracker::ActionKey &actionKey, const ServiceActionTracker::ScopeId scopeId) {
    _serviceActionTracker.beginAction(serviceKeySync, actionKey, scopeId);
}

void SyncService::endAction(const ServiceActionTracker::ActionKey &actionKey, const ServiceActionTracker::ScopeId scopeId) {
    _serviceActionTracker.endAction(serviceKeySync, actionKey, scopeId);
}

bool SyncService::isActionPending(const ServiceActionTracker::ActionKey &actionKey,
                                  const ServiceActionTracker::ScopeId scopeId) const {
    return _serviceActionTracker.isActionPending(serviceKeySync, actionKey, scopeId);
}

/**
 * A start and a stop never overlap for one synchronization: a failed request restores the status it replaced, which would
 * otherwise be the optimistic status of the other request.
 */
bool SyncService::isStartOrStopSyncPending(const qint64 syncDbId) const {
    return isStartSyncPending(syncDbId) || isStopSyncPending(syncDbId);
}

/**
 * Marks every target pending with its optimistic status at once, so that it is counted nowhere until its own reply, then
 * queues the requests. The server jobs of SYNC_START and SYNC_STOP clear a cache shared by every synchronization
 * (`AppServer::_syncCacheMap`) without a lock, so a single queue serves every batch: "Pause all" then "Resume all" before
 * the first reply never sends two requests at once.
 */
void SyncService::controlSyncs(const SyncControlRequest request, const std::vector<SyncDbId> &syncDbIds) {
    const bool start = request == SyncControlRequest::Start;
    const auto batch = std::make_shared<SyncControlBatch>();
    batch->request = request;

    for (const SyncDbId syncDbId: syncDbIds) {
        if (isStartOrStopSyncPending(syncDbId)) {
            qCWarning(lcSyncService) << (start ? "Sync start" : "Sync stop")
                                     << "ignored: a start or stop is already pending | syncDbId:" << syncDbId;
            continue;
        }

        beginAction(start ? actionStartSync : actionStopSync, syncDbId);
        const auto previousStatus = setSyncStatusOptimistically(syncDbId, start ? SyncStatus::Starting : SyncStatus::StopAsked);
        _syncControlQueue.push_back({.syncDbId = syncDbId, .previousStatus = previousStatus, .batch = batch});
        ++batch->remainingCount;
    }

    if (batch->remainingCount > 1) {
        qCInfo(lcSyncService) << (start ? "Starting" : "Stopping")
                              << "syncs one after the other | count:" << batch->remainingCount;
    }

    if (!_syncControlInFlight) {
        sendNextSyncControl();
    }
}

/**
 * Sends the next queued request, whatever its batch. A synchronization removed while it was waiting is skipped.
 */
void SyncService::sendNextSyncControl() {
    if (_syncControlQueue.empty()) {
        _syncControlInFlight = false;
        return;
    }

    QueuedSyncControl queued = _syncControlQueue.front();
    _syncControlQueue.pop_front();
    const bool start = queued.batch->request == SyncControlRequest::Start;

    if (!_appCache.syncContext(queued.syncDbId).has_value()) {
        qCInfo(lcSyncService) << (start ? "Sync start" : "Sync stop")
                              << "skipped: the sync was removed | syncDbId:" << queued.syncDbId;
        endAction(start ? actionStartSync : actionStopSync, queued.syncDbId);
        countSyncControlDone(queued.batch);
        sendNextSyncControl();
        return;
    }

    // A progress push may have replaced the optimistic status while the request was waiting. Without this, a success
    // would show that pushed status until the next tick and let the user send the same action again.
    // The latest status reported by the server is then the one a failure restores.
    const SyncStatus optimisticStatus = start ? SyncStatus::Starting : SyncStatus::StopAsked;
    if (const auto runtimeInfo = _appCache.syncRuntimeInfo(queued.syncDbId);
        runtimeInfo.has_value() && runtimeInfo->status != optimisticStatus) {
        queued.previousStatus = setSyncStatusOptimistically(queued.syncDbId, optimisticStatus);
    }

    _syncControlInFlight = true;
    ++queued.batch->sentCount;
    const auto onReply = [this, queued](const ExitInfo &exitInfo) {
        finishSyncControl(queued.batch->request, queued, exitInfo);
        if (!exitInfo) {
            ++queued.batch->failureCount;
        }

        countSyncControlDone(queued.batch);
        sendNextSyncControl();
    };

    if (start) {
        _commService.requestSyncStart(queued.syncDbId, onReply);
    } else {
        _commService.requestSyncStop(queued.syncDbId, onReply);
    }
}

/**
 * Counts one request of a batch as done, and logs the outcome of a batch of several requests once its last one is done.
 */
void SyncService::countSyncControlDone(const std::shared_ptr<SyncControlBatch> &batch) {
    --batch->remainingCount;
    if (batch->remainingCount > 0 || batch->sentCount <= 1) {
        return;
    }

    qCInfo(lcSyncService) << (batch->request == SyncControlRequest::Start ? "Sync starts" : "Sync stops")
                          << "sent | count:" << batch->sentCount << "/ failures:" << batch->failureCount;
}

/**
 * Ends the pending state of one request, and restores the status it replaced if it failed.
 */
void SyncService::finishSyncControl(const SyncControlRequest request, const QueuedSyncControl &queued, const ExitInfo &exitInfo) {
    const bool start = request == SyncControlRequest::Start;
    endAction(start ? actionStartSync : actionStopSync, queued.syncDbId);
    if (exitInfo) {
        return;
    }

    revertOptimisticSyncStatus(queued.syncDbId, start ? SyncStatus::Starting : SyncStatus::StopAsked, queued.previousStatus);
    notifyRequestFailure(exitInfo, start ? RequestNum::SYNC_START : RequestNum::SYNC_STOP);
}

/**
 * Shows the transition requested by the user before the server reports it: the server only pushes the new status on its
 * next progress tick, after its reply. Returns the replaced status, or nothing when the sync is not in the cache.
 */
std::optional<SyncStatus> SyncService::setSyncStatusOptimistically(const SyncDbId syncDbId, const SyncStatus status) {
    auto runtimeInfo = _appCache.syncRuntimeInfo(syncDbId);
    if (!runtimeInfo.has_value()) {
        return std::nullopt;
    }

    const SyncStatus previousStatus = runtimeInfo->status;
    runtimeInfo->status = status;
    _appCache.updateSyncRuntimeInfo(syncDbId, *runtimeInfo);
    return previousStatus;
}

/**
 * Restores the status replaced by a failed request, unless the server has reported a status since.
 */
void SyncService::revertOptimisticSyncStatus(const SyncDbId syncDbId, const SyncStatus optimisticStatus,
                                             const std::optional<SyncStatus> previousStatus) {
    if (!previousStatus.has_value()) {
        return;
    }

    auto runtimeInfo = _appCache.syncRuntimeInfo(syncDbId);
    if (!runtimeInfo.has_value() || runtimeInfo->status != optimisticStatus) {
        return;
    }

    runtimeInfo->status = *previousStatus;
    _appCache.updateSyncRuntimeInfo(syncDbId, *runtimeInfo);
}

bool SyncService::isValidSyncConfigurationValue(const int32_t syncConfiguration) const {
    return syncConfiguration >= toInt(SyncConfiguration::Classic) && syncConfiguration < toInt(SyncConfiguration::EnumEnd);
}

/**
 * Releases the reservations of successful creations once their SYNC_ADDED push is in AppCache.
 */
void SyncService::releaseSyncedReservations() {
    for (auto it = _awaitingSyncPush.begin(); it != _awaitingSyncPush.end();) {
        if (!_appCache.sync(it->second)) {
            ++it;
            continue;
        }

        const auto key = it->first;
        it = _awaitingSyncPush.erase(it);
        _appCache.setSyncCreationPending(key, false);
    }
}

/**
 * Releases the reservations of failed creations once a reconciliation succeeds. reconcile() restarts any running
 * population, so its completion reflects the server state after the failure. A failed reconciliation may end before
 * the syncs are reloaded, so the drives then stay reserved until a later one succeeds, rather than risk a duplicate.
 */
void SyncService::releaseReconciledReservations() {
    for (const auto keys = std::exchange(_awaitingReconciliation, {}); const auto &availableDriveKey: keys) {
        _appCache.setSyncCreationPending(availableDriveKey, false);
    }
}

void SyncService::notifyRequestFailure(const ExitInfo &exitInfo, const RequestNum requestNum) {
    qCWarning(lcSyncService) << "Sync service request failed | code:" << exitInfo.code() << "/ cause:" << exitInfo.cause();
    _serviceEventBus.notifyGenericError(exitInfo, requestNum);
}

} // namespace KDC
