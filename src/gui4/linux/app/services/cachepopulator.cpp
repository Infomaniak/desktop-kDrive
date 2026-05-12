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

#include "cachepopulator.h"

#include <QLoggingCategory>

#include <cstdlib>

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcCachePopulator, "gui.v4.cachepopulator", QtInfoMsg)

[[noreturn]] void exitOnPopulationFailure(const char *const stage, const ExitInfo &exitInfo) {
    qCCritical(lcCachePopulator) << "Cache population failed at" << stage << "| code:" << exitInfo.code()
                                 << "/ cause:" << exitInfo.cause();
    std::exit(EXIT_FAILURE); // TODO send a sentry message
}
} // namespace

CachePopulator::CachePopulator(CommService &commService, AppCache &appCache, QObject *const parent) :
    QObject(parent),
    _commService(commService),
    _appCache(appCache) {}

void CachePopulator::bootstrap() {
    loadUsers();
}

void CachePopulator::loadUsers() {
    _commService.requestUserDisplayInfoList([this](const ExitInfo &exitInfo, const std::vector<UserDisplayInfo> &list) {
        if (!exitInfo) {
            exitOnPopulationFailure("users", exitInfo);
        }

        _appCache.replaceUsers(list);
        loadAccounts();
    });
}

void CachePopulator::loadAccounts() {
    _commService.requestAccountInfoList([this](const ExitInfo &exitInfo, const std::vector<AccountInfo> &list) {
        if (!exitInfo) {
            exitOnPopulationFailure("accounts", exitInfo);
        }

        _appCache.replaceAccounts(list);
        loadDrives();
    });
}

void CachePopulator::loadDrives() {
    _commService.requestDriveInfoList([this](const ExitInfo &exitInfo, const std::vector<DriveInfo> &list) {
        if (!exitInfo) {
            exitOnPopulationFailure("drives", exitInfo);
        }

        _appCache.replaceDrives(list);
        loadSyncs();
    });
}

void CachePopulator::loadSyncs() {
    _commService.requestSyncInfoList([this](const ExitInfo &exitInfo, const std::vector<SyncInfo> &list) {
        if (!exitInfo) {
            exitOnPopulationFailure("syncs", exitInfo);
        }

        _appCache.replaceSyncs(list);
        loadSyncErrors();
    });
}

void CachePopulator::loadSyncErrors() {
    _commService.requestErrorInfoList([this](const ExitInfo &exitInfo, const std::vector<ErrorInfo> &list) {
        if (!exitInfo) {
            exitOnPopulationFailure("errors", exitInfo);
        }

        std::vector<ErrorInfo> syncErrors;
        std::vector<ErrorInfo> serverErrors;
        syncErrors.reserve(list.size());
        serverErrors.reserve(list.size());
        for (const auto &info: list) {
            switch (info.level()) {
                using enum KDC::ErrorLevel;

                case Node:
                case SyncPal:
                    syncErrors.push_back(info);
                    break;
                case Server:
                    serverErrors.push_back(info);
                    break;
                default:
                    qCWarning(lcCachePopulator)
                            << "Received error with unknown level:" << toInt(info.level()) << "and dbId:" << info.dbId();
            }
        }

        _appCache.replaceSyncErrors(syncErrors);
        _appCache.replaceServerErrors(serverErrors);
        emit bootstrapCompleted();
        activateLiveInfoRefresh();
    });
}

void CachePopulator::activateLiveInfoRefresh() const {
    _commService.requestActivateLoadInfo([](const ExitInfo &exitInfo) {
        if (!exitInfo) {
            qCWarning(lcCachePopulator) << "Live info refresh activation failed | code:" << exitInfo.code()
                                        << "/ cause:" << exitInfo.cause();
        }
    });
}

} // namespace KDC
