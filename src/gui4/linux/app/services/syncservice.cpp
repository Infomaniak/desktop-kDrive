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

#include <utility>

namespace {
constexpr char serviceKeySync[] = "sync";
constexpr char actionStartSync[] = "startSync";
constexpr char actionStopSync[] = "stopSync";
constexpr char actionDeleteSync[] = "deleteSync";
constexpr char actionQuerySyncStatus[] = "querySyncStatus";
constexpr char actionFindGoodPathForNewSync[] = "findGoodPathForNewSync";
constexpr char actionIsPathValidForNewSync[] = "isPathValidForNewSync";

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
    (void) connect(&_cachePopulator, &CachePopulator::reconciliationFailed, this, &SyncService::releaseReconciledReservations);
    (void) connect(&_serviceActionTracker, &ServiceActionTracker::servicePendingChanged, this,
                   [this](const ServiceActionTracker::ServiceKey &serviceKey, const bool) {
                       if (serviceKey == serviceKeySync) {
                           emit loadingChanged();
                       }
                   });
    (void) connect(&_serviceActionTracker, &ServiceActionTracker::actionPendingChanged, this,
                   [this](const ServiceActionTracker::ServiceKey &serviceKey, const ServiceActionTracker::ActionKey &actionKey,
                          const ServiceActionTracker::ScopeId scopeId, const bool) {
                       if (serviceKey == serviceKeySync && (actionKey == actionStartSync || actionKey == actionStopSync)) {
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

void SyncService::startSync(const qint64 syncDbId) {
    beginAction(actionStartSync, syncDbId);

    _commService.requestSyncStart(syncDbId, [this, syncDbId](const ExitInfo &exitInfo) {
        endAction(actionStartSync, syncDbId);
        if (!exitInfo) {
            notifyRequestFailure(exitInfo, RequestNum::SYNC_START);
        }
    });
}

void SyncService::stopSync(const qint64 syncDbId) {
    beginAction(actionStopSync, syncDbId);

    _commService.requestSyncStop(syncDbId, [this, syncDbId](const ExitInfo &exitInfo) {
        endAction(actionStopSync, syncDbId);
        if (!exitInfo) {
            notifyRequestFailure(exitInfo, RequestNum::SYNC_STOP);
        }
    });
}

void SyncService::deleteSync(const qint64 syncDbId) {
    beginAction(actionDeleteSync, syncDbId);

    // Cache consistency is signal-driven: we wait for syncRemoved/syncUpdated pushes.
    _commService.requestSyncDelete(syncDbId, [this, syncDbId](const ExitInfo &exitInfo) {
        endAction(actionDeleteSync, syncDbId);
        if (!exitInfo) {
            notifyRequestFailure(exitInfo, RequestNum::SYNC_DELETE);
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
 * Releases the reservations of failed creations. reconcile() restarts any running population, so its terminal signal
 * always reflects the server state after the failure.
 */
void SyncService::releaseReconciledReservations() {
    const auto keys = std::exchange(_awaitingReconciliation, {});
    for (const auto &key: keys) {
        _appCache.setSyncCreationPending(key, false);
    }
}

void SyncService::notifyRequestFailure(const ExitInfo &exitInfo, const RequestNum requestNum) {
    qCWarning(lcSyncService) << "Sync service request failed | code:" << exitInfo.code() << "/ cause:" << exitInfo.cause();
    _serviceEventBus.notifyGenericError(exitInfo, requestNum);
}

} // namespace KDC
