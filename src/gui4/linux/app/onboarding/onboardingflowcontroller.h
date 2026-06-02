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

#include <QLoggingCategory>
#include <QObject>
#include <QString>

#include <cstdint>

Q_DECLARE_LOGGING_CATEGORY(lcOnboardingFlowController)

namespace KDC {

/**
 * UI flow state machine for the Linux v4 onboarding.
 *
 * Role: own the current onboarding step and expose the macOS-aligned onboarding flow contract to QML.
 * Non-role: perform OAuth, fetch drives, or create syncs; those actions stay in service facades.
 */
class OnboardingFlowController final : public QObject {
        Q_OBJECT
        Q_PROPERTY(Step currentStep READ currentStep NOTIFY currentStepChanged)
        Q_PROPERTY(LoginState loginState READ loginState NOTIFY loginStateChanged)
        Q_PROPERTY(qint32 currentStepIndex READ currentStepIndex NOTIFY currentStepChanged)
        Q_PROPERTY(qint32 stepCount READ stepCount CONSTANT)
        Q_PROPERTY(bool loginInProgress READ loginInProgress NOTIFY loginStateChanged)
        Q_PROPERTY(bool waitingForWebAuthentication READ waitingForWebAuthentication NOTIFY loginStateChanged)
        Q_PROPERTY(bool loginFailed READ loginFailed NOTIFY loginStateChanged)
        Q_PROPERTY(QString loginStatusText READ loginStatusText NOTIFY loginStateChanged)
        Q_PROPERTY(QString title READ title NOTIFY currentStepChanged)

    public:
        enum Step : uint8_t {
            Login = 0,
            DriveSelection,
            Synchronization,
            Ready,
        };
        Q_ENUM(Step)

        enum LoginState : uint8_t {
            LoginIdle = 0,
            WaitingForWebAuthentication,
            LoadingUser,
            LoginError,
        };
        Q_ENUM(LoginState)

        explicit OnboardingFlowController(QObject *parent = nullptr);

        [[nodiscard]] Step currentStep() const { return _currentStep; }
        [[nodiscard]] LoginState loginState() const { return _loginState; }
        [[nodiscard]] qint32 currentStepIndex() const;
        [[nodiscard]] qint32 stepCount() const;
        [[nodiscard]] bool loginInProgress() const;
        [[nodiscard]] bool waitingForWebAuthentication() const;
        [[nodiscard]] bool loginFailed() const;
        [[nodiscard]] QString loginStatusText() const;
        [[nodiscard]] QString title() const;

        Q_INVOKABLE void requestLogin();
        Q_INVOKABLE void requestAccountCreation() const;
        Q_INVOKABLE void cancel();
        Q_INVOKABLE void restart();
        Q_INVOKABLE void setCurrentStep(Step step);

    public slots:
        void handleLoginTokenSucceeded(qint64 userDbId);
        void handleLoginFailed(const QString &error, const QString &errorDescription);
        void completeLogin(qint64 userDbId);

    signals:
        void currentStepChanged();
        void loginStateChanged();
        void loginRequested();
        void cancelRequested();
        void completed();

    private:
        static constexpr qint32 stepCountValue{4};

        void setLoginState(LoginState loginState);

        Step _currentStep{Login};
        LoginState _loginState{LoginIdle};
};

} // namespace KDC
