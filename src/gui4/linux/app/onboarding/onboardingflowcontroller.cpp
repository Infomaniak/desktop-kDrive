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

#include "onboardingflowcontroller.h"

#include "app/appconstants.h"

#include <QDesktopServices>

#include <type_traits>

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcOnboardingFlowController, "gui.v4.onboardingflow", QtInfoMsg)
}

namespace {
[[nodiscard]] qint32 stepToIndex(const OnboardingFlowController::Step step) {
    return static_cast<qint32>(static_cast<std::underlying_type_t<OnboardingFlowController::Step>>(step));
}

} // namespace

OnboardingFlowController::OnboardingFlowController(QObject *const parent) :
    QObject(parent) {}

qint32 OnboardingFlowController::currentStepIndex() const {
    return stepToIndex(_currentStep);
}

qint32 OnboardingFlowController::stepCount() const {
    return stepCountValue;
}

bool OnboardingFlowController::loginInProgress() const {
    return _loginState == WaitingForWebAuthentication || _loginState == LoadingUser;
}

bool OnboardingFlowController::loginFailed() const {
    return _loginState == LoginError;
}

QString OnboardingFlowController::loginStatusText() const {
    switch (_loginState) {
        case LoginIdle:
            return {};
        case WaitingForWebAuthentication:
            return tr("Waiting for browser authentication...");
        case LoadingUser:
            return tr("Loading your kDrive account...");
        case LoginError:
            return tr("Unable to connect. Please try again.");
    }
    return {};
}

QString OnboardingFlowController::title() const {
    switch (_currentStep) {
        case Login:
            return tr("Bienvenue dans kDrive");
        case DriveSelection:
            return tr("Choisir un kDrive");
        case Synchronization:
            return tr("Configurer la synchronisation");
        case Ready:
            return tr("kDrive est prêt");
    }
    return {};
}

void OnboardingFlowController::requestLogin() {
    if (_currentStep != Login || loginInProgress()) {
        return;
    }

    qCInfo(lcOnboardingFlowController) << "Onboarding login requested";
    emit loginRequested();
}

void OnboardingFlowController::requestAccountCreation() {
    if (_currentStep != Login || loginInProgress()) {
        return;
    }

    const auto url = AppConstants::Login::signupUri();
    qCInfo(lcOnboardingFlowController) << "Opening onboarding account creation URL:" << url;
    if (!QDesktopServices::openUrl(url)) {
        qCWarning(lcOnboardingFlowController) << "Failed to open onboarding account creation URL:" << url;
    }
}

void OnboardingFlowController::cancel() {
    qCInfo(lcOnboardingFlowController) << "Onboarding cancel requested";
    emit cancelRequested();
}

void OnboardingFlowController::restart() {
    setCurrentStep(Login);
    setLoginState(LoginIdle);
}

void OnboardingFlowController::setCurrentStep(const Step step) {
    const auto index = stepToIndex(step);
    if (index < 0 || index >= stepCountValue) {
        qCWarning(lcOnboardingFlowController) << "Invalid onboarding step ignored:" << index;
        return;
    }

    if (_currentStep == step) {
        return;
    }

    qCInfo(lcOnboardingFlowController) << "Onboarding step changed | from:" << currentStepIndex() << "/ to:" << index;
    _currentStep = step;
    emit currentStepChanged();
}

void OnboardingFlowController::handleLoginTokenSucceeded(const qint64 userDbId) {
    qCInfo(lcOnboardingFlowController) << "Onboarding login token succeeded | userDbId:" << userDbId;
    setLoginState(LoadingUser);
}

void OnboardingFlowController::handleLoginFailed(const QString &error, const QString &errorDescription) {
    qCWarning(lcOnboardingFlowController) << "Onboarding login failed | error:" << error << "/ description:" << errorDescription;
    setLoginState(LoginError);
}

void OnboardingFlowController::completeLogin(const qint64 userDbId) {
    if (_currentStep != Login) {
        return;
    }

    qCInfo(lcOnboardingFlowController) << "Onboarding user cache ready | userDbId:" << userDbId;
    setLoginState(LoginIdle);
    setCurrentStep(DriveSelection);
}

void OnboardingFlowController::setLoginState(const LoginState loginState) {
    if (_loginState == loginState) {
        return;
    }

    _loginState = loginState;
    emit loginStateChanged();
}

} // namespace KDC
