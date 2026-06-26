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

#include "libcommon/utility/types.h"

#include <QObject>

#include <optional>

namespace KDC {

class AppCache;
class CachePopulator;
class OnboardingFlowController;
class OnboardingState;
class UserService;

/**
 * Coordinates onboarding entry after the initial app cache bootstrap.
 *
 * Role: if the app starts with an authenticated user but no configured drive, enter drive selection and request
 * available drives. Non-role: own login, OAuth, or sync creation workflows.
 */
class OnboardingBootstrapCoordinator final : public QObject {
        Q_OBJECT

    public:
        explicit OnboardingBootstrapCoordinator(CachePopulator &cachePopulator, AppCache &appCache,
                                                OnboardingState &onboardingState, UserService &userService,
                                                OnboardingFlowController &flowController, QObject *parent = nullptr);

    signals:
        void windowActivationRequested();

    private:
        void handleBootstrapCompleted();
        [[nodiscard]] std::optional<UserDbId> onboardingUserDbId() const;

        AppCache &_appCache;
        OnboardingState &_onboardingState;
        UserService &_userService;
        OnboardingFlowController &_flowController;
};

} // namespace KDC
