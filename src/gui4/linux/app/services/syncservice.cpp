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

#include "libcommon/utility/types.h"

#include <QLoggingCategory>

namespace {
constexpr char serviceKeySync[] = "sync";
constexpr char actionLoadSyncs[] = "loadSyncs";
constexpr char actionAddSync[] = "addSync";
constexpr char actionStartSync[] = "startSync";
constexpr char actionStopSync[] = "stopSync";
constexpr char actionDeleteSync[] = "deleteSync";
constexpr char actionQuerySyncStatus[] = "querySyncStatus";
constexpr char actionFindGoodPathForNewSync[] = "findGoodPathForNewSync";
constexpr char actionIsPathValidForNewSync[] = "isPathValidForNewSync";
} // namespace

namespace KDC {

Q_LOGGING_CATEGORY(lcSyncService, "gui.v4.syncservice", QtInfoMsg)

SyncService::SyncService(CommService &commService, AppCache &appCache, ServiceActionTracker &serviceActionTracker,
                         ServiceEventBus &serviceEventBus, QObject *const parent) :
    QObject(parent),
    _commService(commService),
    _appCache(appCache),
    _serviceActionTracker(serviceActionTracker),
    _serviceEventBus(serviceEventBus) {
    (void) connect(&_commService, &CommService::syncAdded, &_appCache, &AppCache::upsertSync);
    (void) connect(&_commService, &CommService::syncUpdated, &_appCache, &AppCache::upsertSync);
    (void) connect(&_commService, &CommService::syncRemoved, &_appCache, &AppCache::removeSync);
    (void) connect(&_commService, &CommService::errorAdded, &_appCache, &AppCache::upsertError);
    (void) connect(&_commService, &CommService::errorRemoved, &_appCache, &AppCache::removeError);
    (void) connect(&_serviceActionTracker, &ServiceActionTracker::servicePendingChanged, this,
                   [this](const QString &serviceKey, const bool pending) {
                       if (serviceKey == serviceKeySync) {
                           setLoading(pending);
                       }
                   });
    setLoading(_serviceActionTracker.isServicePending(serviceKeySync));
}

void SyncService::loadSyncs() {
    beginAction(actionLoadSyncs);

    _commService.requestSyncInfoList([this](const ExitInfo &exitInfo, const std::vector<SyncInfo> &list) {
        endAction(actionLoadSyncs);
        if (!exitInfo) {
            notifyRequestFailure(exitInfo, RequestNum::SYNC_INFOLIST);
            return;
        }

        _appCache.replaceSyncs(list);
    });
}

void SyncService::addSync(const qint64 userDbId, const qint64 accountId, const qint64 driveId, const QString &localFolderPath,
                          const QString &serverFolderPath, const QString &serverFolderNodeId, const bool liteSync) {
    beginAction(actionAddSync);

    SyncAddRequest request;
    request.userDbId = static_cast<UserDbId>(userDbId);
    request.accountId = static_cast<AccountId>(accountId);
    request.driveId = static_cast<DriveId>(driveId);
    request.localFolderPath = QStr2Path(localFolderPath);
    request.serverFolderPath = QStr2Path(serverFolderPath);
    request.serverFolderNodeId = QStr2Str(serverFolderNodeId);
    request.liteSync = liteSync;

    _commService.requestSyncAdd(request, [this](const ExitInfo &exitInfo, const SyncInfo &syncInfo) {
        endAction(actionAddSync);
        if (!exitInfo) {
            notifyRequestFailure(exitInfo, RequestNum::SYNC_ADD);
            return;
        }

        emit syncAddCompleted(static_cast<qint64>(syncInfo.dbId()));
    });
}

void SyncService::startSync(const qint64 syncDbId) {
    beginAction(actionStartSync, syncDbId);

    _commService.requestSyncStart(static_cast<SyncDbId>(syncDbId), [this, syncDbId](const ExitInfo &exitInfo) {
        endAction(actionStartSync, syncDbId);
        if (!exitInfo) {
            notifyRequestFailure(exitInfo, RequestNum::SYNC_START);
        }
    });
}

void SyncService::stopSync(const qint64 syncDbId) {
    beginAction(actionStopSync, syncDbId);

    _commService.requestSyncStop(static_cast<SyncDbId>(syncDbId), [this, syncDbId](const ExitInfo &exitInfo) {
        endAction(actionStopSync, syncDbId);
        if (!exitInfo) {
            notifyRequestFailure(exitInfo, RequestNum::SYNC_STOP);
        }
    });
}

void SyncService::deleteSync(const qint64 syncDbId) {
    beginAction(actionDeleteSync, syncDbId);

    // Cache consistency is signal-driven: we wait for syncRemoved/syncUpdated pushes.
    _commService.requestSyncDelete(static_cast<SyncDbId>(syncDbId), [this, syncDbId](const ExitInfo &exitInfo) {
        endAction(actionDeleteSync, syncDbId);
        if (!exitInfo) {
            notifyRequestFailure(exitInfo, RequestNum::SYNC_DELETE);
        }
    });
}

void SyncService::querySyncStatus(const qint64 syncDbId) {
    beginAction(actionQuerySyncStatus, syncDbId);

    _commService.requestSyncStatus(static_cast<SyncDbId>(syncDbId),
                                   [this, syncDbId](const ExitInfo &exitInfo, const SyncStatus status) {
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

    _commService.requestFindGoodPathForNewSync(QStr2Path(basePath),
                                               [this](const ExitInfo &exitInfo, const GoodPathResult &result) {
                                                   endAction(actionFindGoodPathForNewSync);
                                                   if (!exitInfo) {
                                                       notifyRequestFailure(exitInfo, RequestNum::UTILITY_FINDGOODPATHFORNEWSYNC);
                                                       return;
                                                   }

                                                   emit suggestedPathReceived(Path2QStr(result.goodPath), result.errorMessage);
                                               });
}

void SyncService::isPathValidForNewSync(const QString &path, const int32_t syncConfiguration) {
    if (!isValidSyncConfigurationValue(syncConfiguration)) {
        qCWarning(lcSyncService) << "isPathValidForNewSync: invalid sync configuration value:" << syncConfiguration;
        emit pathValidationReceived(false);
        return;
    }

    beginAction(actionIsPathValidForNewSync);

    _commService.requestIsPathValidForNewSync(QStr2Path(path), static_cast<SyncConfiguration>(syncConfiguration),
                                              [this](const ExitInfo &exitInfo, const bool isValid) {
                                                  endAction(actionIsPathValidForNewSync);
                                                  if (!exitInfo) {
                                                      notifyRequestFailure(exitInfo, RequestNum::UTILITY_ISPATHVALIDFORNEWSYNC);
                                                      emit pathValidationReceived(false);
                                                      return;
                                                  }

                                                  emit pathValidationReceived(isValid);
                                              });
}

bool SyncService::isLoadSyncsPending() const {
    return isActionPending(actionLoadSyncs);
}

bool SyncService::isAddSyncPending() const {
    return isActionPending(actionAddSync);
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

void SyncService::beginAction(const QString &actionKey, const qint64 scopeId) {
    _serviceActionTracker.beginAction(serviceKeySync, actionKey, scopeId);
}

void SyncService::endAction(const QString &actionKey, const qint64 scopeId) {
    _serviceActionTracker.endAction(serviceKeySync, actionKey, scopeId);
}

bool SyncService::isActionPending(const QString &actionKey, const qint64 scopeId) const {
    return _serviceActionTracker.isActionPending(serviceKeySync, actionKey, scopeId);
}

bool SyncService::isValidSyncConfigurationValue(const int32_t syncConfiguration) const {
    return syncConfiguration >= toInt(SyncConfiguration::Classic) && syncConfiguration < toInt(SyncConfiguration::EnumEnd);
}

void SyncService::setLoading(const bool loading) {
    if (_loading == loading) {
        return;
    }
    _loading = loading;
    emit loadingChanged();
}

void SyncService::notifyRequestFailure(const ExitInfo &exitInfo, const RequestNum requestNum) {
    qCWarning(lcSyncService) << "Sync service request failed | code:" << exitInfo.code() << "/ cause:" << exitInfo.cause();
    _serviceEventBus.notifyGenericError(exitInfo, requestNum);
}

} // namespace KDC
