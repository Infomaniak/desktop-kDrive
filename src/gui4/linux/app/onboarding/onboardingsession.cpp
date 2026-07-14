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

#include "onboardingsession.h"

#include "app/cache/appcache.h"
#include "app/services/cachepopulator.h"
#include "app/services/commservice.h"
#include "app/services/serviceeventbus.h"
#include "app/services/userservice.h"

#include <QLoggingCategory>

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcOnboardingSession, "gui.v4.onboardingsession", QtInfoMsg)
} // namespace

OnboardingSession::OnboardingSession(AppCache &appCache, CommService &commService, UserService &userService,
                                     CachePopulator &cachePopulator, ServiceEventBus &serviceEventBus,
                                     const EntryPoint entryPoint, const std::optional<UserDbId> selectedUserDbId,
                                     const uint64_t generation, QObject *const parent) :
    QObject(parent),
    _userService(userService),
    _onboardingState(appCache),
    _loginCoordinator(_flowController, commService, userService, appCache, _onboardingState),
    _driveSelectionController(appCache, _onboardingState, userService, _flowController),
    _syncCreationCoordinator(_flowController, _onboardingState, appCache, commService, cachePopulator, serviceEventBus),
    _generation(generation) {
    (void) connect(&_loginCoordinator, &OnboardingLoginCoordinator::openWindowRequested, this,
                   &OnboardingSession::openWindowRequested);

    if (entryPoint != EntryPoint::DriveSelection) {
        return;
    }

    if (!selectedUserDbId.has_value()) {
        qCWarning(lcOnboardingSession) << "Drive-selection onboarding session has no selected user | generation:" << _generation;
        return;
    }

    _onboardingState.selectUser(*selectedUserDbId);
    _flowController.setCurrentStep(OnboardingFlowController::DriveSelection);
    userService.loadAvailableDrives(*selectedUserDbId);
}

AvailableDrivesModel *OnboardingSession::availableDrivesModel() {
    return _driveSelectionController.availableDrivesModel();
}

void OnboardingSession::invalidatePendingOperations() {
    _userService.invalidateLoginTokenRequest();
    if (const UserDbId selectedUserDbId = _onboardingState.typedSelectedUserDbId(); selectedUserDbId != 0) {
        _userService.invalidateAvailableDrivesRequest(selectedUserDbId);
    }
}

} // namespace KDC
