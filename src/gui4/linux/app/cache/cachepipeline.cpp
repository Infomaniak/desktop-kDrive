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

#include "cachepipeline.h"

#include "libcommon/utility/cstypes.h"

#include <QLoggingCategory>

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcCachePipeline, "gui.v4.cachepipeline", QtInfoMsg)
} // namespace

CachePipeline::CachePipeline(CommService &commService, AppCache &appCache, QObject *const parent) :
    QObject(parent),
    _commService(commService),
    _appCache(appCache) {
    (void) connect(&_commService, &CommService::userAdded, this, [this](const UserInfo &info) {
        if (acceptPush("userAdded")) _appCache.upsertUser(info);
    });
    (void) connect(&_commService, &CommService::userUpdated, this, [this](const UserInfo &info) {
        if (acceptPush("userUpdated")) _appCache.upsertUser(info);
    });
    (void) connect(&_commService, &CommService::userRemoved, this, [this](const UserDbId userDbId) {
        if (acceptPush("userRemoved")) _appCache.removeUser(userDbId);
    });

    (void) connect(&_commService, &CommService::accountAdded, this, [this](const AccountInfo &info) {
        if (acceptPush("accountAdded")) _appCache.upsertAccount(info);
    });
    (void) connect(&_commService, &CommService::accountUpdated, this, [this](const AccountInfo &info) {
        if (acceptPush("accountUpdated")) _appCache.upsertAccount(info);
    });
    (void) connect(&_commService, &CommService::accountRemoved, this, [this](const AccountDbId accountDbId) {
        if (acceptPush("accountRemoved")) _appCache.removeAccount(accountDbId);
    });

    (void) connect(&_commService, &CommService::driveAdded, this, [this](const DriveInfo &info) {
        if (acceptPush("driveAdded")) _appCache.upsertDrive(info);
    });
    (void) connect(&_commService, &CommService::driveUpdated, this, [this](const DriveInfo &info) {
        if (acceptPush("driveUpdated")) _appCache.upsertDrive(info);
    });
    (void) connect(&_commService, &CommService::driveRemoved, this, [this](const DriveDbId driveDbId) {
        if (acceptPush("driveRemoved")) _appCache.removeDrive(driveDbId);
    });

    (void) connect(&_commService, &CommService::syncAdded, this, [this](const SyncInfo &info) {
        if (acceptPush("syncAdded")) _appCache.upsertSync(info);
    });
    (void) connect(&_commService, &CommService::syncUpdated, this, [this](const SyncInfo &info) {
        if (acceptPush("syncUpdated")) _appCache.upsertSync(info);
    });
    (void) connect(&_commService, &CommService::syncRemoved, this, [this](const SyncDbId syncDbId) {
        if (acceptPush("syncRemoved")) _appCache.removeSync(syncDbId);
    });

    (void) connect(&_commService, &CommService::errorAdded, this, [this](const ErrorInfo &info) {
        if (!acceptPush("errorAdded")) {
            return;
        }
        if (info.level() == ErrorLevel::Server) {
            _appCache.upsertServerError(info);
            return;
        }
        _appCache.upsertSyncError(info);
    });
    (void) connect(&_commService, &CommService::errorRemoved, this, [this](const ErrorDbId errorDbId) {
        if (!acceptPush("errorRemoved")) {
            return;
        }
        if (_appCache.syncError(errorDbId).has_value()) {
            _appCache.removeSyncError(errorDbId);
            return;
        }
        if (_appCache.serverError(errorDbId).has_value()) {
            _appCache.removeServerError(errorDbId);
        }
    });
}

void CachePipeline::markHydrated() {
    _hydrated = true;
    qCInfo(lcCachePipeline) << "Cache hydration completed; live cache push mutations enabled";
}

bool CachePipeline::acceptPush(const char *const signalName) const {
    if (_hydrated) {
        return true;
    }

    qCWarning(lcCachePipeline) << "Cache push dropped before hydration completed | signal:" << signalName;
    return false;
}

} // namespace KDC
