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
#include "serviceutils.h"

#include "libcommon/utility/types.h"

#include <QLoggingCategory>

namespace KDC {

Q_LOGGING_CATEGORY(lcSyncService, "gui.v4.syncservice", QtInfoMsg)

SyncService::SyncService(CommService &commService, AppCache &appCache, QObject *const parent) :
    QObject(parent),
    _commService(commService),
    _appCache(appCache) {
    (void) connect(&_commService, &CommService::syncAdded, &_appCache, &AppCache::upsertSync);
    (void) connect(&_commService, &CommService::syncUpdated, &_appCache, &AppCache::upsertSync);
    (void) connect(&_commService, &CommService::syncRemoved, &_appCache, &AppCache::removeSync);
    (void) connect(&_commService, &CommService::errorAdded, &_appCache, &AppCache::upsertError);
    (void) connect(&_commService, &CommService::errorRemoved, &_appCache, &AppCache::removeError);
}

void SyncService::loadSyncs() {
    beginRequest();
    setLastError(QString());

    _commService.requestSyncInfoList([this](const ExitInfo &exitInfo, const std::vector<SyncInfo> &list) {
        endRequest();
        if (exitInfo.code() != ExitCode::Ok) {
            setLastError(ServiceUtils::formatExitInfo(exitInfo));
            return;
        }

        _appCache.replaceSyncs(list);
    });
}

void SyncService::addSync(const qint64 userDbId, const qint64 accountId, const qint64 driveId, const QString &localFolderPath,
                          const QString &serverFolderPath, const QString &serverFolderNodeId, const bool liteSync) {
    beginRequest();
    setLastError(QString());

    SyncAddRequest request;
    request.userDbId = static_cast<UserDbId>(userDbId);
    request.accountId = static_cast<AccountId>(accountId);
    request.driveId = static_cast<DriveId>(driveId);
    request.localFolderPath = QStr2Path(localFolderPath);
    request.serverFolderPath = QStr2Path(serverFolderPath);
    request.serverFolderNodeId = QStr2Str(serverFolderNodeId);
    request.liteSync = liteSync;

    _commService.requestSyncAdd(request, [this](const ExitInfo &exitInfo, const SyncInfo &syncInfo) {
        endRequest();
        if (exitInfo.code() != ExitCode::Ok) {
            setLastError(ServiceUtils::formatExitInfo(exitInfo));
            return;
        }

        emit syncAddCompleted(static_cast<qint64>(syncInfo.dbId()));
    });
}

void SyncService::startSync(const qint64 syncDbId) {
    beginRequest();
    setLastError(QString());

    _commService.requestSyncStart(static_cast<SyncDbId>(syncDbId), [this](const ExitInfo &exitInfo) {
        endRequest();
        if (exitInfo.code() != ExitCode::Ok) {
            setLastError(ServiceUtils::formatExitInfo(exitInfo));
        }
    });
}

void SyncService::stopSync(const qint64 syncDbId) {
    beginRequest();
    setLastError(QString());

    _commService.requestSyncStop(static_cast<SyncDbId>(syncDbId), [this](const ExitInfo &exitInfo) {
        endRequest();
        if (exitInfo.code() != ExitCode::Ok) {
            setLastError(ServiceUtils::formatExitInfo(exitInfo));
        }
    });
}

void SyncService::deleteSync(const qint64 syncDbId) {
    beginRequest();
    setLastError(QString());

    // Cache consistency is signal-driven: we wait for syncRemoved/syncUpdated pushes.
    _commService.requestSyncDelete(static_cast<SyncDbId>(syncDbId), [this](const ExitInfo &exitInfo) {
        endRequest();
        if (exitInfo.code() != ExitCode::Ok) {
            setLastError(ServiceUtils::formatExitInfo(exitInfo));
        }
    });
}

void SyncService::querySyncStatus(const qint64 syncDbId) {
    beginRequest();
    setLastError(QString());

    _commService.requestSyncStatus(static_cast<SyncDbId>(syncDbId),
                                   [this, syncDbId](const ExitInfo &exitInfo, const SyncStatus status) {
                                       endRequest();
                                       if (exitInfo.code() != ExitCode::Ok) {
                                           setLastError(ServiceUtils::formatExitInfo(exitInfo));
                                           return;
                                       }

                                       emit syncStatusReceived(syncDbId, toInt(status));
                                   });
}

void SyncService::findGoodPathForNewSync(const QString &basePath) {
    beginRequest();
    setLastError(QString());

    _commService.requestFindGoodPathForNewSync(
            QStr2Path(basePath), [this](const ExitInfo &exitInfo, const GoodPathResult &result) {
                endRequest();
                if (exitInfo.code() != ExitCode::Ok) {
                    setLastError(ServiceUtils::formatExitInfo(exitInfo));
                    return;
                }

                emit suggestedPathReceived(Path2QStr(result.goodPath), result.errorMessage);
            });
}

void SyncService::isPathValidForNewSync(const QString &path, const int32_t syncConfiguration) {
    setLastError(QString());
    if (!isValidSyncConfigurationValue(syncConfiguration)) {
        setLastError(QStringLiteral("Invalid sync configuration value: %1").arg(syncConfiguration));
        emit pathValidationReceived(false);
        return;
    }

    beginRequest();

    _commService.requestIsPathValidForNewSync(QStr2Path(path), static_cast<SyncConfiguration>(syncConfiguration),
                                              [this](const ExitInfo &exitInfo, const bool isValid) {
                                                  endRequest();
                                                  if (exitInfo.code() != ExitCode::Ok) {
                                                      setLastError(ServiceUtils::formatExitInfo(exitInfo));
                                                      emit pathValidationReceived(false);
                                                      return;
                                                  }

                                                  emit pathValidationReceived(isValid);
                                              });
}

void SyncService::beginRequest() {
    ++_pendingRequestCount;
    setLoading(true);
}

void SyncService::endRequest() {
    if (_pendingRequestCount <= 0) {
        qCWarning(lcSyncService) << "endRequest called with non-positive pending count:" << _pendingRequestCount;
        _pendingRequestCount = 0;
        setLoading(false);
        return;
    }

    --_pendingRequestCount;
    if (_pendingRequestCount == 0) {
        setLoading(false);
    }
}

bool SyncService::isValidSyncConfigurationValue(const int32_t syncConfiguration) const {
    return syncConfiguration >= toInt(SyncConfiguration::Classic) &&
           syncConfiguration < toInt(SyncConfiguration::EnumEnd);
}

void SyncService::setLoading(const bool loading) {
    if (_loading == loading) {
        return;
    }
    _loading = loading;
    emit loadingChanged();
}

void SyncService::setLastError(const QString &error) {
    if (_lastError == error) {
        return;
    }
    _lastError = error;
    emit lastErrorChanged();
}

} // namespace KDC
