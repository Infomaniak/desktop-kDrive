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
#include "serviceutils.h"

#include <QLoggingCategory>
#include <QPointer>

namespace KDC {

Q_LOGGING_CATEGORY(lcDriveService, "gui.v4.driveservice", QtInfoMsg)

DriveService::DriveService(CommService &commService, AppCache &appCache, QObject *const parent) :
    QObject(parent),
    _commService(commService),
    _appCache(appCache) {
    (void) connect(&_commService, &CommService::driveAdded, &_appCache, &AppCache::upsertDrive);
    (void) connect(&_commService, &CommService::driveUpdated, &_appCache, &AppCache::upsertDrive);
    (void) connect(&_commService, &CommService::driveRemoved, &_appCache, &AppCache::removeDrive);
    (void) connect(&_appCache, &AppCache::selectedDriveDbIdChanged, this, &DriveService::activeDriveDbIdChanged);
}

void DriveService::loadDrives() {
    beginRequest();
    setLastError(QString());

    const QPointer<DriveService> self(this);
    _commService.requestDriveInfoList([self](const ExitInfo &exitInfo, const std::vector<DriveInfo> &list) {
        if (!self) {
            return;
        }

        self->endRequest();
        if (exitInfo.code() != ExitCode::Ok) {
            self->setLastError(ServiceUtils::formatExitInfo(exitInfo));
            return;
        }

        self->_appCache.replaceDrives(list);
    });
}

void DriveService::deleteDrive(const qint64 driveDbId) {
    beginRequest();
    setLastError(QString());

    const QPointer<DriveService> self(this);
    // Cache consistency is signal-driven: we wait for driveRemoved/driveUpdated pushes.
    _commService.requestDriveDelete(static_cast<DriveDbId>(driveDbId), [self](const ExitInfo &exitInfo) {
        if (!self) {
            return;
        }

        self->endRequest();
        if (exitInfo.code() != ExitCode::Ok) {
            self->setLastError(ServiceUtils::formatExitInfo(exitInfo));
        }
    });
}

void DriveService::setActiveDrive(const qint64 driveDbId) {
    _appCache.setSelectedDriveDbId(driveDbId);
}

void DriveService::updateDrive(const DriveInfo &driveInfo) {
    beginRequest();
    setLastError(QString());

    const QPointer<DriveService> self(this);
    _commService.requestDriveUpdate(driveInfo, [self](const ExitInfo &exitInfo) {
        if (!self) {
            return;
        }

        self->endRequest();
        if (exitInfo.code() != ExitCode::Ok) {
            self->setLastError(ServiceUtils::formatExitInfo(exitInfo));
        }
    });
}

void DriveService::beginRequest() {
    ++_pendingRequestCount;
    setLoading(true);
}

void DriveService::endRequest() {
    if (_pendingRequestCount <= 0) {
        qCWarning(lcDriveService) << "endRequest called with non-positive pending count:" << _pendingRequestCount;
        _pendingRequestCount = 0;
        setLoading(false);
        return;
    }

    --_pendingRequestCount;
    if (_pendingRequestCount == 0) {
        setLoading(false);
    }
}

void DriveService::setLoading(const bool loading) {
    if (_loading == loading) {
        return;
    }
    _loading = loading;
    emit loadingChanged();
}

void DriveService::setLastError(const QString &error) {
    if (_lastError == error) {
        return;
    }
    _lastError = error;
    emit lastErrorChanged();
}

} // namespace KDC
