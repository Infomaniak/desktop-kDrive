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

#include "onboardingsessionmanager.h"

#include "app/cache/appcache.h"
#include "app/onboarding/onboardingflowcontroller.h"
#include "app/services/cachepopulator.h"
#include "app/services/commservice.h"
#include "app/services/userservice.h"

#include <QLoggingCategory>

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcOnboardingSessionManager, "gui.v4.onboardingsessionmanager", QtInfoMsg)
} // namespace

OnboardingSessionManager::OnboardingSessionManager(const CachePopulator &cachePopulator, AppCache &appCache,
                                                   CommService &commService, UserService &userService, QObject *const parent) :
    QObject(parent),
    _appCache(appCache),
    _commService(commService),
    _userService(userService) {
    (void) connect(&cachePopulator, &CachePopulator::bootstrapCompleted, this,
                   &OnboardingSessionManager::handleBootstrapCompleted);
}

void OnboardingSessionManager::openOnboardingWindow() {
    if (!_bootstrapCompleted) {
        qCInfo(lcOnboardingSessionManager)
                << "Onboarding window open skipped: cache bootstrap is not complete and no onboarding route is displayable";
        return;
    }

    if (_state == LifecycleState::Stopping) {
        _restartRequested = true;
        _windowActivationPending = true;
        qCInfo(lcOnboardingSessionManager)
                << "Onboarding window open deferred: the current onboarding session is being destroyed";
        return;
    }

    ensureSession();
    openWindowIfDisplayable();
}

void OnboardingSessionManager::ensureSession() {
    if (!_bootstrapCompleted) {
        return;
    }

    if (_state == LifecycleState::Active) {
        return;
    }

    if (_state == LifecycleState::Stopping) {
        _restartRequested = true;
        return;
    }

    const auto decision = determineOnboardingEntry(_appCache);
    switch (decision.entryPoint) {
        using enum OnboardingEntryDecision::EntryPoint;
        case Inactive:
            _state = LifecycleState::Inactive;
            return;
        case Login:
            startSession(OnboardingSession::EntryPoint::Login, std::nullopt);
            return;
        case DriveSelection:
            startSession(OnboardingSession::EntryPoint::DriveSelection, decision.selectedUserDbId);
            return;
    }
}

void OnboardingSessionManager::openWindowIfDisplayable() {
    if (_state == LifecycleState::Active && _activeSession != nullptr) {
        emit openOnboardingWindowRequested();
        return;
    }

    qCInfo(lcOnboardingSessionManager) << "Onboarding window open skipped: onboarding is not required"
                                       << "| configuredDrives:" << _appCache.driveContexts().size();
}

void OnboardingSessionManager::handleBootstrapCompleted() {
    _bootstrapCompleted = true;
    ensureSession();
    if (_activeSession != nullptr) {
        emit openOnboardingWindowRequested();
    }
}

void OnboardingSessionManager::startSession(const OnboardingSession::EntryPoint entryPoint,
                                            const std::optional<UserDbId> selectedUserDbId) {
    if (_state == LifecycleState::Active || _state == LifecycleState::Stopping) {
        return;
    }

    const auto generation = _nextGeneration++;
    auto *const session =
            new OnboardingSession(_appCache, _commService, _userService, entryPoint, selectedUserDbId, generation, this);
    _activeSession = session;
    _state = LifecycleState::Active;

    (void) connect(session, &OnboardingSession::openWindowRequested, this,
                   &OnboardingSessionManager::openOnboardingWindowRequested);
    (void) connect(session->flowController(), &OnboardingFlowController::cancelRequested, this, [this, session] {
        if (_activeSession == session) {
            stopSession(true);
        }
    });
    (void) connect(session->flowController(), &OnboardingFlowController::completed, this, [this, session] {
        if (_activeSession == session) {
            stopSession(true);
        }
    });

    qCInfo(lcOnboardingSessionManager) << "Onboarding session started | generation:" << generation
                                       << "| entryPoint:" << static_cast<uint16_t>(entryPoint);
    emit activeSessionChanged();
}

void OnboardingSessionManager::stopSession(const bool closeWindow) {
    if (_state != LifecycleState::Active || _activeSession == nullptr) {
        return;
    }

    _state = LifecycleState::Stopping;
    auto *const sessionPendingDestruction = _activeSession;
    sessionPendingDestruction->invalidatePendingOperations();
    _activeSession = nullptr;
    (void) disconnect(sessionPendingDestruction->flowController(), nullptr, this, nullptr);
    (void) disconnect(sessionPendingDestruction, nullptr, this, nullptr);
    (void) connect(sessionPendingDestruction, &QObject::destroyed, this,
                   &OnboardingSessionManager::handleRetiringSessionDestroyed);

    qCInfo(lcOnboardingSessionManager) << "Onboarding session unpublished | generation:"
                                       << sessionPendingDestruction->generation();
    emit activeSessionChanged();
    if (closeWindow) {
        emit closeOnboardingWindowRequested();
    }
    sessionPendingDestruction->deleteLater();
}

void OnboardingSessionManager::handleRetiringSessionDestroyed() {
    if (_state != LifecycleState::Stopping) {
        return;
    }

    _state = LifecycleState::Inactive;
    const bool activateAfterRestart = _windowActivationPending;
    _windowActivationPending = false;
    if (!_restartRequested) {
        return;
    }

    _restartRequested = false;
    ensureSession();
    if (activateAfterRestart) {
        openWindowIfDisplayable();
    }
}

} // namespace KDC
