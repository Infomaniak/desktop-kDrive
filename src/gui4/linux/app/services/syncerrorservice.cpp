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
                       if (serviceKey == serviceKeySyncError && actionKey == actionRefreshErrors) {
                           emit refreshPendingChanged(scopeId);
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

} // namespace KDC
