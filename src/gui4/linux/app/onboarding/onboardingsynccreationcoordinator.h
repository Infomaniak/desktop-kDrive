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

#pragma once

#include "app/cache/cachetypes.h"

#include <QObject>
#include <QString>

#include <deque>

namespace KDC {

class AppCache;
class CachePopulator;
class CommService;
class OnboardingFlowController;
class OnboardingState;
class ServiceEventBus;
struct GoodPathResult;

/**
 * Coordinates automatic sync creation at the end of Linux v4 onboarding.
 *
 * Selected drives are configured sequentially with a collision-free local path and the kDrive root as remote target.
 * Successful creations are removed from the retry queue, while failed and not-yet-attempted creations remain pending.
 */
class OnboardingSyncCreationCoordinator final : public QObject {
        Q_OBJECT

    public:
        explicit OnboardingSyncCreationCoordinator(OnboardingFlowController &flowController, OnboardingState &onboardingState,
                                                   AppCache &appCache, CommService &commService, CachePopulator &cachePopulator,
                                                   ServiceEventBus &serviceEventBus, QObject *parent = nullptr);

    private:
        void startSynchronization();
        void createNextSynchronization();
        void prepareSynchronization(const AvailableDriveKey &key);
        void handleGoodPathResult(const AvailableDriveKey &key, const ExitInfo &exitInfo, const GoodPathResult &result);
        void createSynchronization(const AvailableDriveKey &key, const PendingSyncConfig &config);
        void discardPendingSynchronization(const AvailableDriveKey &key);
        void handleCreationFailure(bool cacheReconciliationRequired = false);
        void handleCacheReconciliationCompleted();
        void handleCacheReconciliationFailed();
        [[nodiscard]] QString defaultLocalPath(const QString &driveName) const;

        OnboardingFlowController &_flowController;
        OnboardingState &_onboardingState;
        AppCache &_appCache;
        CommService &_commService;
        CachePopulator &_cachePopulator;
        ServiceEventBus &_serviceEventBus;
        std::deque<AvailableDriveKey> _pendingDriveKeys;
        bool _cacheReconciliationPending{false};
};

} // namespace KDC
