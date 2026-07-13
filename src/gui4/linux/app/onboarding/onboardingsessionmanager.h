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

#include "app/onboarding/onboardingentrydecision.h"
#include "app/onboarding/onboardingsession.h"

#include <QObject>

#include <cstdint>
#include <optional>

namespace KDC {

class AppCache;
class CachePopulator;
class CommService;
class ServiceEventBus;
class UserService;

/**
 * Process-long owner of the nullable Linux v4 onboarding session.
 *
 * The manager decides the initial route from the bootstrapped cache and publishes one stable QML boundary. Session shutdown is
 * deliberately two-phase: first unpublish the session so QML can unload, then defer its destruction to the event loop.
 */
class OnboardingSessionManager final : public QObject {
        Q_OBJECT
        Q_PROPERTY(OnboardingSession *activeSession READ activeSession NOTIFY activeSessionChanged)

    public:
        explicit OnboardingSessionManager(CachePopulator &cachePopulator, AppCache &appCache, CommService &commService,
                                          UserService &userService, ServiceEventBus &serviceEventBus, QObject *parent = nullptr);

        [[nodiscard]] OnboardingSession *activeSession() const { return _activeSession; }

        /**
         * Opens the onboarding window only when an onboarding session provides displayable content.
         */
        Q_INVOKABLE void openOnboardingWindow();

    signals:
        void activeSessionChanged();
        void openOnboardingWindowRequested();
        void closeOnboardingWindowRequested();
        void onboardingCompleted();

    private:
        enum class LifecycleState : uint8_t {
            Determining,
            Inactive,
            Active,
            Stopping,
        };

        void ensureSession();
        void openWindowIfDisplayable();
        void handleBootstrapCompleted();
        void startSession(OnboardingSession::EntryPoint entryPoint, std::optional<UserDbId> selectedUserDbId);
        void stopSession(bool closeWindow);
        void handleRetiringSessionDestroyed();
        CachePopulator &_cachePopulator;
        AppCache &_appCache;
        CommService &_commService;
        UserService &_userService;
        ServiceEventBus &_serviceEventBus;
        OnboardingSession *_activeSession = nullptr;
        LifecycleState _state{LifecycleState::Determining};
        uint64_t _nextGeneration{1};
        bool _bootstrapCompleted{false};
        bool _restartRequested{false};
        bool _windowActivationPending{false};
};

} // namespace KDC
