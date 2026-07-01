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

#include "app/cache/onboardingstate.h"
#include "app/onboarding/driveselectioncontroller.h"
#include "app/onboarding/onboardingflowcontroller.h"
#include "app/onboarding/onboardinglogincoordinator.h"
#include "app/onboarding/onboardingsynccreationcoordinator.h"

#include <QObject>

#include <cstdint>
#include <optional>

namespace KDC {

class AppCache;
class AvailableDrivesModel;
class CachePopulator;
class CommService;
class ServiceEventBus;
class UserService;

/**
 * Owns the complete, short-lived object graph for one Linux v4 onboarding run.
 *
 * Process-long services are injected by reference and are never owned by the session. Member declaration order is intentional:
 * dependants are destroyed before the state and flow objects they reference.
 */
class OnboardingSession final : public QObject {
        Q_OBJECT
        Q_PROPERTY(OnboardingFlowController *flowController READ flowController CONSTANT)
        Q_PROPERTY(DriveSelectionController *driveSelectionController READ driveSelectionController CONSTANT)
        Q_PROPERTY(AvailableDrivesModel *availableDrivesModel READ availableDrivesModel CONSTANT)

    public:
        enum class EntryPoint : uint8_t {
            Login,
            DriveSelection,
        };

        explicit OnboardingSession(AppCache &appCache, CommService &commService, UserService &userService,
                                   CachePopulator &cachePopulator, ServiceEventBus &serviceEventBus, EntryPoint entryPoint,
                                   std::optional<UserDbId> selectedUserDbId, uint64_t generation, QObject *parent = nullptr);

        [[nodiscard]] OnboardingFlowController *flowController() { return &_flowController; }
        [[nodiscard]] DriveSelectionController *driveSelectionController() { return &_driveSelectionController; }
        [[nodiscard]] AvailableDrivesModel *availableDrivesModel();
        [[nodiscard]] uint64_t generation() const { return _generation; }
        void invalidatePendingOperations();

    signals:
        void openWindowRequested();

    private:
        UserService &_userService;
        OnboardingState _onboardingState;
        OnboardingFlowController _flowController;
        OnboardingLoginCoordinator _loginCoordinator;
        DriveSelectionController _driveSelectionController;
        OnboardingSyncCreationCoordinator _syncCreationCoordinator;
        const uint64_t _generation;
};

} // namespace KDC
