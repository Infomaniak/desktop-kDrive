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

#include "driveservice.h"

#include <QLoggingCategory>

namespace {
constexpr char serviceKeyDrive[] = "drive";
constexpr char actionLoadDrives[] = "loadDrives";
constexpr char actionDeleteDrive[] = "deleteDrive";
constexpr char actionUpdateDrive[] = "updateDrive";
} // namespace

namespace KDC {

Q_LOGGING_CATEGORY(lcDriveService, "gui.v4.driveservice", QtInfoMsg)

DriveService::DriveService(CommService &commService, AppCache &appCache, ServiceActionTracker &serviceActionTracker,
                           ServiceEventBus &serviceEventBus, QObject *const parent) :
    QObject(parent),
    _commService(commService),
    _appCache(appCache),
    _serviceActionTracker(serviceActionTracker),
    _serviceEventBus(serviceEventBus) {
    (void) connect(&_serviceActionTracker, &ServiceActionTracker::servicePendingChanged, this,
                   [this](const ServiceActionTracker::ServiceKey &serviceKey, const bool) {
                       if (serviceKey == serviceKeyDrive) {
                           emit loadingChanged();
                       }
                   });
}

bool DriveService::loading() const {
    return _serviceActionTracker.isServicePending(serviceKeyDrive);
}

void DriveService::loadDrives() {
    beginAction(actionLoadDrives);

    _commService.requestDriveInfoList([this](const ExitInfo &exitInfo, const std::vector<DriveInfo> &list) {
        endAction(actionLoadDrives);
        if (!exitInfo) {
            notifyRequestFailure(exitInfo, RequestNum::DRIVE_INFOLIST);
            return;
        }

        _appCache.replaceDrives(list);
    });
}

void DriveService::deleteDrive(const qint64 driveDbId) {
    beginAction(actionDeleteDrive, driveDbId);

    // Cache consistency is signal-driven: we wait for driveRemoved/driveUpdated pushes.
    _commService.requestDriveDelete(static_cast<DriveDbId>(driveDbId), [this, driveDbId](const ExitInfo &exitInfo) {
        endAction(actionDeleteDrive, driveDbId);
        if (!exitInfo) {
            notifyRequestFailure(exitInfo, RequestNum::DRIVE_DELETE);
        }
    });
}

void DriveService::updateDrive(const DriveInfo &driveInfo) {
    beginAction(actionUpdateDrive, static_cast<qint64>(driveInfo.dbId()));

    _commService.requestDriveUpdate(driveInfo, [this, driveDbId = static_cast<qint64>(driveInfo.dbId())](const ExitInfo &exitInfo) {
        endAction(actionUpdateDrive, driveDbId);
        if (!exitInfo) {
            notifyRequestFailure(exitInfo, RequestNum::DRIVE_UPDATE);
        }
    });
}

void DriveService::beginAction(const ServiceActionTracker::ActionKey &actionKey, const ServiceActionTracker::ScopeId scopeId) {
    _serviceActionTracker.beginAction(serviceKeyDrive, actionKey, scopeId);
}

void DriveService::endAction(const ServiceActionTracker::ActionKey &actionKey, const ServiceActionTracker::ScopeId scopeId) {
    _serviceActionTracker.endAction(serviceKeyDrive, actionKey, scopeId);
}

bool DriveService::isActionPending(const ServiceActionTracker::ActionKey &actionKey,
                                   const ServiceActionTracker::ScopeId scopeId) const {
    return _serviceActionTracker.isActionPending(serviceKeyDrive, actionKey, scopeId);
}

void DriveService::notifyRequestFailure(const ExitInfo &exitInfo, const RequestNum requestNum) {
    qCWarning(lcDriveService) << "Drive service request failed | code:" << exitInfo.code() << "/ cause:" << exitInfo.cause();
    _serviceEventBus.notifyGenericError(exitInfo, requestNum);
}

} // namespace KDC
