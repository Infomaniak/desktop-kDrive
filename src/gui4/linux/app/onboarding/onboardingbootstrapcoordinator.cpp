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

#include "onboardingbootstrapcoordinator.h"

#include "app/cache/appcache.h"
#include "app/cache/onboardingstate.h"
#include "app/onboarding/onboardingflowcontroller.h"
#include "app/services/cachepopulator.h"
#include "app/services/userservice.h"

#include <QLoggingCategory>

#include <algorithm>
#include <ranges>

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcOnboardingBootstrapCoordinator, "gui.v4.onboardingbootstrapcoordinator", QtInfoMsg)
} // namespace

OnboardingBootstrapCoordinator::OnboardingBootstrapCoordinator(CachePopulator &cachePopulator, AppCache &appCache,
                                                               OnboardingState &onboardingState, UserService &userService,
                                                               OnboardingFlowController &flowController, QObject *const parent) :
    QObject(parent),
    _appCache(appCache),
    _onboardingState(onboardingState),
    _userService(userService),
    _flowController(flowController) {
    (void) connect(&cachePopulator, &CachePopulator::bootstrapCompleted, this,
                   &OnboardingBootstrapCoordinator::handleBootstrapCompleted);
}

void OnboardingBootstrapCoordinator::handleBootstrapCompleted() {
    if (_flowController.currentStep() != OnboardingFlowController::Login) {
        return;
    }

    if (!_appCache.driveContexts().empty()) {
        return;
    }

    const auto userDbId = onboardingUserDbId();
    if (!userDbId) {
        return;
    }

    qCInfo(lcOnboardingBootstrapCoordinator)
            << "Entering onboarding drive selection after cache bootstrap | userDbId:" << *userDbId;
    _onboardingState.selectUser(*userDbId);
    _flowController.completeLogin(*userDbId);
    if (!_userService.isLoadAvailableDrivesPending(static_cast<qint64>(*userDbId))) {
        _userService.loadAvailableDrives(static_cast<qint64>(*userDbId));
    }
    emit windowActivationRequested();
}

std::optional<UserDbId> OnboardingBootstrapCoordinator::onboardingUserDbId() const {
    const auto users = _appCache.users();
    if (users.empty()) {
        return std::nullopt;
    }

    const auto connectedUserIt = std::ranges::find_if(users, [](const UserInfo &user) { return user.connected(); });
    return connectedUserIt != users.end() ? std::optional<UserDbId>{connectedUserIt->dbId()} : std::nullopt;
}

} // namespace KDC
