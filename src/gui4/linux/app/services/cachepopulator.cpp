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

#include "app/services/sentryservice.h"

#include <QLoggingCategory>
#include <QString>

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcCachePopulator, "gui.v4.cachepopulator", QtInfoMsg)

[[noreturn]] void exitOnPopulationFailure(const char *const stage, const ExitInfo &exitInfo) {
    qCCritical(lcCachePopulator) << "Cache population failed at" << stage << "| code:" << exitInfo.code()
                                 << "/ cause:" << exitInfo.cause();
    SentryService::reportFatalAndExit(
            QStringLiteral("Cache population failed"),
            QStringLiteral("stage: %1 | %2").arg(QString::fromLatin1(stage), QString::fromStdString(toString(exitInfo))));
}
} // namespace

CachePopulator::CachePopulator(CommService &commService, AppCache &appCache, QObject *const parent) :
    QObject(parent),
    _commService(commService),
    _appCache(appCache) {}

void CachePopulator::bootstrap() {
    loadUsers(PopulationMode::Bootstrap);
}

void CachePopulator::reconcile() {
    loadUsers(PopulationMode::Reconciliation);
}

void CachePopulator::loadUsers(const PopulationMode mode) {
    _commService.requestUserDisplayInfoList([this, mode](const ExitInfo &exitInfo, const std::vector<UserDisplayInfo> &list) {
        if (!exitInfo && handlePopulationFailure("users", exitInfo, mode)) {
            return;
        }

        _appCache.replaceUsers(list);
        loadAccounts(mode);
    });
}

void CachePopulator::loadAccounts(const PopulationMode mode) {
    _commService.requestAccountInfoList([this, mode](const ExitInfo &exitInfo, const std::vector<Account> &list) {
        if (!exitInfo && handlePopulationFailure("accounts", exitInfo, mode)) {
            return;
        }

        _appCache.replaceAccounts(list);
        loadDrives(mode);
    });
}

void CachePopulator::loadDrives(const PopulationMode mode) {
    _commService.requestDriveList([this, mode](const ExitInfo &exitInfo, const std::vector<Drive> &list) {
        if (!exitInfo && handlePopulationFailure("drives", exitInfo, mode)) {
            return;
        }

        _appCache.replaceDrives(list);
        loadSyncs(mode);
    });
}

void CachePopulator::loadSyncs(const PopulationMode mode) {
    _commService.requestSyncInfoList([this, mode](const ExitInfo &exitInfo, const std::vector<SyncInfo> &list) {
        if (!exitInfo && handlePopulationFailure("syncs", exitInfo, mode)) {
            return;
        }

        _appCache.replaceSyncs(list);
        loadSyncErrors(mode);
    });
}

void CachePopulator::loadSyncErrors(const PopulationMode mode) {
    _commService.requestErrorInfoList([this, mode](const ExitInfo &exitInfo, const std::vector<ErrorInfo> &list) {
        if (!exitInfo && handlePopulationFailure("errors", exitInfo, mode)) {
            return;
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
        if (mode == PopulationMode::Bootstrap) {
            emit bootstrapCompleted();
        } else {
            emit reconciliationCompleted();
        }
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

bool CachePopulator::handlePopulationFailure(const char *const stage, const ExitInfo &exitInfo, const PopulationMode mode) {
    if (mode == PopulationMode::Bootstrap) {
        exitOnPopulationFailure(stage, exitInfo);
    }

    qCWarning(lcCachePopulator) << "Cache reconciliation failed at" << stage << "| code:" << exitInfo.code()
                                << "/ cause:" << exitInfo.cause();
    emit reconciliationFailed();
    return true;
}

} // namespace KDC
