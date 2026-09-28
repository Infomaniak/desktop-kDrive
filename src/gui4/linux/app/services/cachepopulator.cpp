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

CachePopulator::CachePopulator(CommService &commService, AppCache &appCache, ParametersStore &parametersStore,
                               QObject *const parent) :
    QObject(parent),
    _commService(commService),
    _appCache(appCache),
    _parametersStore(parametersStore) {}

void CachePopulator::bootstrap() {
    startPopulation(PopulationMode::Bootstrap);
}

void CachePopulator::reconcile() {
    startPopulation(PopulationMode::Reconciliation);
}

void CachePopulator::startPopulation(const PopulationMode mode) {
    const uint64_t generation = ++_populationGeneration;
    _populationProgress = {};
    loadParameters(mode, generation);
    loadUserData(mode, generation);
}

void CachePopulator::loadParameters(const PopulationMode mode, const uint64_t generation) {
    _commService.requestParametersInfo([this, mode, generation](const ExitInfo &exitInfo, const ParametersInfo &parametersInfo) {
        if (generation != _populationGeneration) {
            return;
        }

        if (!exitInfo && handlePopulationFailure("parameters", exitInfo, mode)) {
            return;
        }

        _parametersStore.replaceParametersInfo(parametersInfo);
        markBranchCompleted(mode, PopulationBranch::Parameters);
    });
}

void CachePopulator::loadUserData(const PopulationMode mode, const uint64_t generation) {
    _commService.requestUserDisplayInfoList(
            [this, mode, generation](const ExitInfo &exitInfo, const std::vector<UserDisplayInfo> &list) {
                if (generation != _populationGeneration) {
                    return;
                }

                if (!exitInfo && handlePopulationFailure("users", exitInfo, mode)) {
                    return;
                }

                _appCache.replaceUsers(list);
                loadAccounts(mode, generation);
            });
}

void CachePopulator::loadAccounts(const PopulationMode mode, const uint64_t generation) {
    _commService.requestAccountInfoList([this, mode, generation](const ExitInfo &exitInfo, const std::vector<Account> &list) {
        if (generation != _populationGeneration) {
            return;
        }

        if (!exitInfo && handlePopulationFailure("accounts", exitInfo, mode)) {
            return;
        }

        _appCache.replaceAccounts(list);
        loadDrives(mode, generation);
    });
}

void CachePopulator::loadDrives(const PopulationMode mode, const uint64_t generation) {
    _commService.requestDriveList([this, mode, generation](const ExitInfo &exitInfo, const std::vector<Drive> &list) {
        if (generation != _populationGeneration) {
            return;
        }

        if (!exitInfo && handlePopulationFailure("drives", exitInfo, mode)) {
            return;
        }

        _appCache.replaceDrives(list);
        loadSyncs(mode, generation);
    });
}

void CachePopulator::loadSyncs(const PopulationMode mode, const uint64_t generation) {
    _commService.requestSyncInfoList([this, mode, generation](const ExitInfo &exitInfo, const std::vector<BaseSync> &list) {
        if (generation != _populationGeneration) {
            return;
        }

        if (!exitInfo && handlePopulationFailure("syncs", exitInfo, mode)) {
            return;
        }

        _appCache.replaceSyncs(list);
        loadSyncErrors(mode, generation);
    });
}

void CachePopulator::loadSyncErrors(const PopulationMode mode, const uint64_t generation) {
    _commService.requestErrorList([this, mode, generation](const ExitInfo &exitInfo, const std::vector<Error> &list) {
        if (generation != _populationGeneration) {
            return;
        }

        if (!exitInfo && handlePopulationFailure("errors", exitInfo, mode)) {
            return;
        }

        replaceErrorsByLevel(list);
        markBranchCompleted(mode, PopulationBranch::UserData);
    });
}

void CachePopulator::replaceErrorsByLevel(const std::vector<Error> &list) {
    std::vector<Error> syncErrors;
    std::vector<Error> serverErrors;
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
                qCWarning(lcCachePopulator) << "Received error with unknown level:" << toInt(info.level())
                                            << "and dbId:" << info.dbId();
        }
    }

    _appCache.replaceSyncErrors(syncErrors);
    _appCache.replaceServerErrors(serverErrors);
}

void CachePopulator::markBranchCompleted(const PopulationMode mode, const PopulationBranch branch) {
    switch (branch) {
        case PopulationBranch::Parameters:
            _populationProgress.parametersCompleted = true;
            break;
        case PopulationBranch::UserData:
            _populationProgress.userDataCompleted = true;
            break;
    }

    if (_populationProgress.terminalSignalEmitted || !_populationProgress.parametersCompleted ||
        !_populationProgress.userDataCompleted) {
        return;
    }

    _populationProgress.terminalSignalEmitted = true;
    if (mode == PopulationMode::Bootstrap) {
        emit bootstrapCompleted();
    } else {
        emit reconciliationCompleted();
    }
    activateLiveInfoRefresh();
}

void CachePopulator::activateLiveInfoRefresh() const {
    _commService.requestActivateLoadInfo([](const ExitInfo &exitInfo) {
        if (!exitInfo) {
            qCWarning(lcCachePopulator) << "Live info refresh activation failed | code:" << exitInfo.code()
                                        << "/ cause:" << exitInfo.cause();
            SentryService::reportError(QStringLiteral("Live info refresh activation failed"),
                                       QString::fromStdString(toString(exitInfo)));
        }
    });
}

bool CachePopulator::handlePopulationFailure(const char *const stage, const ExitInfo &exitInfo, const PopulationMode mode) {
    if (mode == PopulationMode::Bootstrap) {
        exitOnPopulationFailure(stage, exitInfo);
    }

    qCWarning(lcCachePopulator) << "Cache reconciliation failed at" << stage << "| code:" << exitInfo.code()
                                << "/ cause:" << exitInfo.cause();
    if (!_populationProgress.terminalSignalEmitted) {
        _populationProgress.terminalSignalEmitted = true;
        emit reconciliationFailed();
    }
    return true;
}

} // namespace KDC
