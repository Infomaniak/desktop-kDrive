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

#include "cachebootstrapper.h"

#include <QLoggingCategory>
#include <QPointer>

namespace KDC {

Q_LOGGING_CATEGORY(lcCacheBootstrapper, "gui.v4.cachebootstrapper", QtInfoMsg)

CacheBootstrapper::CacheBootstrapper(CommService &commService, AppCache &appCache, QObject *const parent) :
    QObject(parent),
    _commService(commService),
    _appCache(appCache) {}

void CacheBootstrapper::bootstrap() {
    loadUsers();
}

void CacheBootstrapper::loadUsers() {
    const QPointer<CacheBootstrapper> self(this);
    _commService.requestUserInfoList([self](const ExitInfo &exitInfo, const std::vector<UserInfo> &list) {
        if (!self) {
            return;
        }

        if (!exitInfo) {
            qCWarning(lcCacheBootstrapper) << "User bootstrap failed | code:" << exitInfo.code() << "/ cause:" << exitInfo.cause();
            return;
        }

        self->_appCache.replaceUsers(list);
        self->loadAccounts();
    });
}

void CacheBootstrapper::loadAccounts() {
    const QPointer<CacheBootstrapper> self(this);
    _commService.requestAccountInfoList([self](const ExitInfo &exitInfo, const std::vector<AccountInfo> &list) {
        if (!self) {
            return;
        }

        if (!exitInfo) {
            qCWarning(lcCacheBootstrapper) << "Account bootstrap failed | code:" << exitInfo.code()
                                          << "/ cause:" << exitInfo.cause();
            return;
        }

        self->_appCache.replaceAccounts(list);
        self->loadDrives();
    });
}

void CacheBootstrapper::loadDrives() {
    const QPointer<CacheBootstrapper> self(this);
    _commService.requestDriveInfoList([self](const ExitInfo &exitInfo, const std::vector<DriveInfo> &list) {
        if (!self) {
            return;
        }

        if (!exitInfo) {
            qCWarning(lcCacheBootstrapper) << "Drive bootstrap failed | code:" << exitInfo.code() << "/ cause:" << exitInfo.cause();
            return;
        }

        self->_appCache.replaceDrives(list);
        self->loadSyncs();
    });
}

void CacheBootstrapper::loadSyncs() {
    const QPointer<CacheBootstrapper> self(this);
    _commService.requestSyncInfoList([self](const ExitInfo &exitInfo, const std::vector<SyncInfo> &list) {
        if (!self) {
            return;
        }

        if (!exitInfo) {
            qCWarning(lcCacheBootstrapper) << "Sync bootstrap failed | code:" << exitInfo.code() << "/ cause:" << exitInfo.cause();
            return;
        }

        self->_appCache.replaceSyncs(list);
        self->loadSyncErrors();
    });
}

void CacheBootstrapper::loadSyncErrors() {
    const QPointer<CacheBootstrapper> self(this);
    _commService.requestErrorInfoList([self](const ExitInfo &exitInfo, const std::vector<ErrorInfo> &list) {
        if (!self) {
            return;
        }

        if (!exitInfo) {
            qCWarning(lcCacheBootstrapper) << "Error bootstrap failed | code:" << exitInfo.code() << "/ cause:" << exitInfo.cause();
            return;
        }

        self->_appCache.replaceSyncErrors(list);
    });
}

} // namespace KDC
