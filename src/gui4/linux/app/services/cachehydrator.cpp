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

#include "cachehydrator.h"

#include <QLoggingCategory>

namespace KDC {

Q_LOGGING_CATEGORY(lcCacheHydrator, "gui.v4.cachehydrator", QtInfoMsg)

CacheHydrator::CacheHydrator(CommService &commService, AppCache &appCache, QObject *const parent) :
    QObject(parent),
    _commService(commService),
    _appCache(appCache) {}

void CacheHydrator::bootstrap() const {
    loadUsers();
}

void CacheHydrator::loadUsers() const {
    _commService.requestUserInfoList([this](const ExitInfo &exitInfo, const std::vector<UserInfo> &list) {
        if (!exitInfo) {
            qCWarning(lcCacheHydrator) << "User bootstrap failed | code:" << exitInfo.code() << "/ cause:" << exitInfo.cause();
            loadAccounts();
            return;
        }

        _appCache.replaceUsers(list);
        loadAccounts();
    });
}

void CacheHydrator::loadAccounts() const {
    _commService.requestAccountInfoList([this](const ExitInfo &exitInfo, const std::vector<AccountInfo> &list) {
        if (!exitInfo) {
            qCWarning(lcCacheHydrator) << "Account bootstrap failed | code:" << exitInfo.code()
                                       << "/ cause:" << exitInfo.cause();
            loadDrives();
            return;
        }

        _appCache.replaceAccounts(list);
        loadDrives();
    });
}

void CacheHydrator::loadDrives() const {
    _commService.requestDriveInfoList([this](const ExitInfo &exitInfo, const std::vector<DriveInfo> &list) {
        if (!exitInfo) {
            qCWarning(lcCacheHydrator) << "Drive bootstrap failed | code:" << exitInfo.code() << "/ cause:" << exitInfo.cause();
            loadSyncs();
            return;
        }

        _appCache.replaceDrives(list);
        loadSyncs();
    });
}

void CacheHydrator::loadSyncs() const {
    _commService.requestSyncInfoList([this](const ExitInfo &exitInfo, const std::vector<SyncInfo> &list) {
        if (!exitInfo) {
            qCWarning(lcCacheHydrator) << "Sync bootstrap failed | code:" << exitInfo.code() << "/ cause:" << exitInfo.cause();
            loadSyncErrors();
            return;
        }

        _appCache.replaceSyncs(list);
        loadSyncErrors();
    });
}

void CacheHydrator::loadSyncErrors() const {
    _commService.requestErrorInfoList([this](const ExitInfo &exitInfo, const std::vector<ErrorInfo> &list) {
        if (!exitInfo) {
            qCWarning(lcCacheHydrator) << "Error bootstrap failed | code:" << exitInfo.code() << "/ cause:" << exitInfo.cause();
            return;
        }

        std::vector<ErrorInfo> syncErrors;
        std::vector<ErrorInfo> serverErrors;
        syncErrors.reserve(list.size());
        serverErrors.reserve(list.size());
        for (const auto &info: list) {
            if (info.level() == ErrorLevel::Server) {
                serverErrors.push_back(info);
                continue;
            }

            syncErrors.push_back(info);
        }

        _appCache.replaceSyncErrors(syncErrors);
        _appCache.replaceServerErrors(serverErrors);
    });
}

} // namespace KDC
