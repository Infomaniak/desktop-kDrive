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

#include "onboardinglogincoordinator.h"

#include "app/cache/appcache.h"
#include "app/cache/onboardingstate.h"
#include "app/onboarding/onboardingflowcontroller.h"
#include "app/services/commservice.h"
#include "app/services/userservice.h"

#include <QLoggingCategory>

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcOnboardingLoginCoordinator, "gui.v4.onboardinglogincoordinator", QtInfoMsg)
} // namespace

OnboardingLoginCoordinator::OnboardingLoginCoordinator(OnboardingFlowController &flowController, CommService &commService,
                                                       UserService &userService, AppCache &appCache,
                                                       OnboardingState &onboardingState, QObject *const parent) :
    QObject(parent),
    _flowController(flowController),
    _commService(commService),
    _userService(userService),
    _appCache(appCache),
    _onboardingState(onboardingState) {
    (void) connect(&_flowController, &OnboardingFlowController::loginRequested, &_oauthLoginService,
                   &OAuthLoginService::startAuthorization);
    (void) connect(&_commService, &CommService::authorizationCodeReceived, &_oauthLoginService,
                   &OAuthLoginService::handleAuthorizationCode);
    (void) connect(&_oauthLoginService, &OAuthLoginService::authorizationCodeReady, &_flowController,
                   &OnboardingFlowController::handleAuthorizationCodeReady);
    (void) connect(&_oauthLoginService, &OAuthLoginService::authorizationCodeReady, this,
                   [this](const QString &, const QString &) { clearPendingLogin(); });
    (void) connect(&_oauthLoginService, &OAuthLoginService::authorizationCodeReady, &_userService,
                   &UserService::requestLoginToken);
    (void) connect(&_oauthLoginService, &OAuthLoginService::authorizationFailed, &_flowController,
                   &OnboardingFlowController::handleLoginFailed);

    (void) connect(&_userService, &UserService::loginTokenSucceeded, &_flowController,
                   &OnboardingFlowController::handleLoginTokenSucceeded);
    (void) connect(&_userService, &UserService::loginTokenSucceeded, this,
                   [this](const qint64 userDbId) { loadAvailableDrivesWhenUserIsCached(userDbId); });
    (void) connect(&_userService, &UserService::loginTokenFailed, &_flowController, &OnboardingFlowController::handleLoginFailed);
    (void) connect(&_userService, &UserService::availableDrivesLoadFailed, this,
                   &OnboardingLoginCoordinator::handleAvailableDrivesLoadFailed);

    (void) connect(&_appCache, &AppCache::usersChanged, this, [this] {
        if (!_pendingUserDbId.has_value()) {
            return;
        }

        loadAvailableDrivesWhenUserIsCached(*_pendingUserDbId);
    });
    (void) connect(&_appCache, &AppCache::availableDrivesChanged, this,
                   &OnboardingLoginCoordinator::completeLoginWhenAvailableDrivesAreCached);
}

void OnboardingLoginCoordinator::clearPendingLogin() {
    _pendingUserDbId.reset();
    _pendingAvailableDrivesUserDbId.reset();
}

void OnboardingLoginCoordinator::loadAvailableDrivesWhenUserIsCached(const UserDbId userDbId) {
    if (!_appCache.user(userDbId).has_value()) {
        qCInfo(lcOnboardingLoginCoordinator) << "Waiting for logged-in user cache update | userDbId:" << userDbId;
        _pendingUserDbId = userDbId;
        return;
    }

    _pendingUserDbId.reset();
    _pendingAvailableDrivesUserDbId = userDbId;
    _onboardingState.selectUser(userDbId);
    _userService.loadAvailableDrives(userDbId);
}

void OnboardingLoginCoordinator::completeLoginWhenAvailableDrivesAreCached(const UserDbId userDbId) {
    if (_pendingAvailableDrivesUserDbId != userDbId) {
        return;
    }

    _pendingAvailableDrivesUserDbId.reset();
    _flowController.completeLogin(userDbId);
}

void OnboardingLoginCoordinator::handleAvailableDrivesLoadFailed(const qint64 userDbId) {
    if (_pendingAvailableDrivesUserDbId != static_cast<UserDbId>(userDbId)) {
        return;
    }

    _pendingAvailableDrivesUserDbId.reset();
    _flowController.handleLoginFailed(QString(), QString());
}

} // namespace KDC
