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

#include "app/services/syncerrorservice.h"

#include <QLoggingCategory>

namespace KDC {

namespace {

Q_LOGGING_CATEGORY(lcSyncErrorService, "gui.v4.syncerrorservice", QtInfoMsg)

constexpr char serviceKeySyncError[] = "syncError";
constexpr char actionRefreshErrors[] = "refreshErrors";
constexpr char actionResolveConflicts[] = "resolveConflicts";

} // namespace

SyncErrorService::SyncErrorService(CommService &commService, ServiceActionTracker &serviceActionTracker,
                                   ServiceEventBus &serviceEventBus, QObject *const parent) :
    QObject(parent),
    _commService(commService),
    _serviceActionTracker(serviceActionTracker),
    _serviceEventBus(serviceEventBus) {
    (void) connect(&_serviceActionTracker, &ServiceActionTracker::actionPendingChanged, this,
                   [this](const ServiceActionTracker::ServiceKey &serviceKey, const ServiceActionTracker::ActionKey &actionKey,
                          const ServiceActionTracker::ScopeId scopeId, const bool) {
                       if (serviceKey != serviceKeySyncError) {
                           return;
                       }
                       if (actionKey == actionRefreshErrors) {
                           emit refreshPendingChanged(scopeId);
                       } else if (actionKey == actionResolveConflicts) {
                           emit conflictResolutionPendingChanged(scopeId);
                       }
                   });
}

void SyncErrorService::refreshErrors(const qint64 syncDbId) {
    if (isRefreshPending(syncDbId)) {
        qCInfo(lcSyncErrorService) << "Error refresh ignored, already pending | syncDbId:" << syncDbId;
        return;
    }

    qCInfo(lcSyncErrorService) << "Error refresh requested | syncDbId:" << syncDbId;
    _serviceActionTracker.beginAction(serviceKeySyncError, actionRefreshErrors, syncDbId);

    // The cache is not touched here: the server removes the errors and adds the remaining ones back through push signals.
    _commService.requestErrorSyncRefresh(static_cast<SyncDbId>(syncDbId), [this, syncDbId](const ExitInfo &exitInfo) {
        _serviceActionTracker.endAction(serviceKeySyncError, actionRefreshErrors, syncDbId);
        if (!exitInfo) {
            qCWarning(lcSyncErrorService) << "Error refresh failed | syncDbId:" << syncDbId << "/ code:" << exitInfo.code()
                                          << "/ cause:" << exitInfo.cause();
            _serviceEventBus.notifyGenericError(exitInfo, RequestNum::ERROR_SYNC_REFRESH);
            return;
        }

        qCInfo(lcSyncErrorService) << "Error refresh done | syncDbId:" << syncDbId;
    });
}

bool SyncErrorService::isRefreshPending(const qint64 syncDbId) const {
    return _serviceActionTracker.isActionPending(serviceKeySyncError, actionRefreshErrors, syncDbId);
}

void SyncErrorService::fetchConflictInfo(const qint64 syncDbId, const SyncPath &relativePath, const ReplicaSide side,
                                         const ConflictInfoCallback &callback) const {
    _commService.requestNodeConflictInfo(
            static_cast<SyncDbId>(syncDbId), relativePath, side,
            [syncDbId, relativePath, side, callback](const ExitInfo &exitInfo, const NodeConflictInfo &info) {
                if (!exitInfo) {
                    qCWarning(lcSyncErrorService) << "Conflict info unavailable | syncDbId:" << syncDbId << "/ side:" << side
                                                  << "/ path:" << Path2QStr(relativePath) << "/ code:" << exitInfo.code()
                                                  << "/ cause:" << exitInfo.cause();
                }
                callback(static_cast<bool>(exitInfo), info);
            });
}

void SyncErrorService::resolveConflicts(const qint64 syncDbId, const std::vector<ErrorDbId> &keepLocal,
                                        const std::vector<ErrorDbId> &keepRemote, const ResolutionCallback &callback) {
    qCInfo(lcSyncErrorService) << "Conflict resolution requested | syncDbId:" << syncDbId << "/ keepLocal:" << keepLocal.size()
                               << "/ keepRemote:" << keepRemote.size();
    _serviceActionTracker.beginAction(serviceKeySyncError, actionResolveConflicts, syncDbId);

    _commService.requestErrorResolveConflicts(keepLocal, keepRemote, [this, syncDbId, callback](const ExitInfo &exitInfo) {
        _serviceActionTracker.endAction(serviceKeySyncError, actionResolveConflicts, syncDbId);
        if (!exitInfo) {
            qCWarning(lcSyncErrorService) << "Conflict resolution failed | syncDbId:" << syncDbId << "/ code:" << exitInfo.code()
                                          << "/ cause:" << exitInfo.cause();
            _serviceEventBus.notifyGenericError(exitInfo, RequestNum::ERROR_RESOLVE_CONFLICTS);
        } else {
            qCInfo(lcSyncErrorService) << "Conflict resolution done | syncDbId:" << syncDbId;
        }
        callback(static_cast<bool>(exitInfo));
    });
}

void SyncErrorService::resolveConflictsQuick(const qint64 syncDbId, const std::vector<ErrorDbId> &errorDbIds,
                                             const ConflictResolutionStrategy strategy, const ResolutionCallback &callback) {
    qCInfo(lcSyncErrorService) << "Quick conflict resolution requested | syncDbId:" << syncDbId
                               << "/ strategy:" << QString::fromStdString(toString(strategy)) << "/ count:" << errorDbIds.size();
    _serviceActionTracker.beginAction(serviceKeySyncError, actionResolveConflicts, syncDbId);

    _commService.requestErrorResolveConflictsQuick(errorDbIds, strategy, [this, syncDbId, callback](const ExitInfo &exitInfo) {
        _serviceActionTracker.endAction(serviceKeySyncError, actionResolveConflicts, syncDbId);
        if (exitInfo.code() == ExitCode::OperationCanceled) {
            // Another user action holds the synchronization: expected, not reported.
            qCInfo(lcSyncErrorService) << "Quick conflict resolution canceled, another action is running | syncDbId:" << syncDbId;
        } else if (!exitInfo) {
            qCWarning(lcSyncErrorService) << "Quick conflict resolution failed | syncDbId:" << syncDbId
                                          << "/ code:" << exitInfo.code() << "/ cause:" << exitInfo.cause();
            _serviceEventBus.notifyGenericError(exitInfo, RequestNum::ERROR_RESOLVE_CONFLICTS_QUICK);
        } else {
            qCInfo(lcSyncErrorService) << "Quick conflict resolution done | syncDbId:" << syncDbId;
        }
        callback(static_cast<bool>(exitInfo));
    });
}

bool SyncErrorService::isConflictResolutionPending(const qint64 syncDbId) const {
    return _serviceActionTracker.isActionPending(serviceKeySyncError, actionResolveConflicts, syncDbId);
}

} // namespace KDC
