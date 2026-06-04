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

#include "app/onboarding/oauthloginservice.h"
#include "libcommon/utility/types.h"

#include <QObject>

#include <optional>

namespace KDC {

class AppCache;
class CommService;
class OnboardingFlowController;
class OnboardingState;
class UserService;

/**
 * Coordinates the Linux v4 onboarding login use-case.
 *
 * It bridges the QML-facing flow controller, OAuth browser/callback handling, backend login-token request, cache-driven user
 * availability, and available-drive loading. AppClientLinux owns this object as part of application composition but does not
 * contain the login workflow itself.
 */
class OnboardingLoginCoordinator : public QObject {
        Q_OBJECT

    public:
        explicit OnboardingLoginCoordinator(OnboardingFlowController &flowController, CommService &commService,
                                            UserService &userService, AppCache &appCache, OnboardingState &onboardingState,
                                            QObject *parent = nullptr);

    private:
        void clearPendingLogin();
        void loadAvailableDrivesWhenUserIsCached(UserDbId userDbId);
        void completeLoginWhenAvailableDrivesAreCached(UserDbId userDbId);
        void handleAvailableDrivesLoadFailed(qint64 userDbId);

        OnboardingFlowController &_flowController;
        CommService &_commService;
        UserService &_userService;
        AppCache &_appCache;
        OnboardingState &_onboardingState;
        OAuthLoginService _oauthLoginService{this};
        std::optional<UserDbId> _pendingUserDbId;
        std::optional<UserDbId> _pendingAvailableDrivesUserDbId;
};

} // namespace KDC
