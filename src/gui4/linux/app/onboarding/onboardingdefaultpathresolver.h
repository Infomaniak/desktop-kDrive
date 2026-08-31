/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include "app/cache/cachetypes.h"

#include <QObject>
#include <QString>

#include <unordered_set>

namespace KDC {

class AppCache;
class CommService;
class OnboardingState;
class ServiceEventBus;
struct ExitInfo;
struct GoodPathResult;

/**
 * Resolves the default local folder of a drive as soon as it is selected.
 *
 * Preparing the folder here, rather than when advanced settings open, keeps that modal free of blocking requests and
 * matches the Windows client. A drive whose folder cannot be resolved is unselected, because onboarding has nothing
 * to synchronize it into.
 */
class OnboardingDefaultPathResolver final : public QObject {
        Q_OBJECT

    public:
        explicit OnboardingDefaultPathResolver(const AppCache &appCache, OnboardingState &onboardingState,
                                               CommService &commService, ServiceEventBus &serviceEventBus,
                                               QObject *parent = nullptr);

        /** True while at least one selected drive still awaits its default folder. */
        [[nodiscard]] bool hasPendingResolutions() const { return !_pendingKeys.empty(); }
        void invalidatePendingRequests();

    signals:
        void pendingResolutionsChanged();

    private:
        [[nodiscard]] bool pathTakenByAnotherDrive(const QString &path, const AvailableDriveKey &excludedKey) const;
        void resolveMissingDefaultPaths();
        void handleGoodPathResult(const AvailableDriveKey &key, uint64_t generation, const ExitInfo &exitInfo,
                                  const GoodPathResult &result);
        void finishRequest(const AvailableDriveKey &key);

        const AppCache &_appCache;
        OnboardingState &_onboardingState;
        CommService &_commService;
        ServiceEventBus &_serviceEventBus;
        std::unordered_set<AvailableDriveKey> _pendingKeys;
        uint64_t _generation{0};
};

} // namespace KDC
