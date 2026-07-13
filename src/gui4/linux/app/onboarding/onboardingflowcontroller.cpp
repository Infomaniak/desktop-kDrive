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

#include <QCoreApplication>
#include <QDesktopServices>
#include <QMetaEnum>

Q_LOGGING_CATEGORY(lcOnboardingFlowController, "gui.v4.onboardingflow", QtInfoMsg)

namespace KDC {

namespace {
[[nodiscard]] qint32 stepToIndex(const OnboardingFlowController::Step step) {
    return static_cast<uint8_t>(step);
}

} // namespace

OnboardingFlowController::OnboardingFlowController(QObject *const parent) :
    QObject(parent) {}

bool OnboardingFlowController::loginInProgress() const {
    return _loginState == WaitingForWebAuthentication || _loginState == LoadingUser;
}

bool OnboardingFlowController::waitingForWebAuthentication() const {
    return _loginState == WaitingForWebAuthentication;
}

bool OnboardingFlowController::loginFailed() const {
    return _loginState == LoginError;
}

bool OnboardingFlowController::driveSelectionActive() const {
    return _currentStep == DriveSelection;
}

bool OnboardingFlowController::synchronizationActive() const {
    return _currentStep == Synchronization;
}

bool OnboardingFlowController::readyActive() const {
    return _currentStep == Ready;
}

QString OnboardingFlowController::title() const {
    switch (_currentStep) {
        case Login:
            return qtTrId("onboardingLoginTitle");
        case DriveSelection:
            return qtTrId("onboardingDriveSelectionTitle");
        case Synchronization:
            return qtTrId("onboardingSynchronizationInProgressTitle");
        case Ready:
            return qtTrId("onboardingSynchronizationAppReadyTitle");
    }
    return {};
}

void OnboardingFlowController::requestLogin() {
    if (_currentStep != Login || loginInProgress()) {
        return;
    }

    qCInfo(lcOnboardingFlowController) << "Onboarding login requested";
    setLoginState(WaitingForWebAuthentication);
    emit loginRequested();
}

void OnboardingFlowController::requestAccountCreation() const {
    if (_currentStep != Login || loginInProgress()) {
        return;
    }

    const auto url = AppConstants::Login::signupUri();
    qCInfo(lcOnboardingFlowController) << "Opening onboarding account creation URL:" << url;
    if (!QDesktopServices::openUrl(url)) {
        qCWarning(lcOnboardingFlowController) << "Failed to open onboarding account creation URL:" << url;
    }
}

void OnboardingFlowController::requestDriveOffers() const {
    if (_currentStep != DriveSelection) {
        return;
    }

    const auto url = AppConstants::Onboarding::driveOffersUri();
    qCInfo(lcOnboardingFlowController) << "Opening onboarding drive offers URL:" << url;
    if (!QDesktopServices::openUrl(url)) {
        qCWarning(lcOnboardingFlowController) << "Failed to open onboarding drive offers URL:" << url;
    }
}

void OnboardingFlowController::requestFreeDriveOrder() const {
    if (_currentStep != DriveSelection) {
        return;
    }

    const auto url = AppConstants::Onboarding::freeDriveOrderUri();
    qCInfo(lcOnboardingFlowController) << "Opening onboarding free drive order URL:" << url;
    if (!QDesktopServices::openUrl(url)) {
        qCWarning(lcOnboardingFlowController) << "Failed to open onboarding free drive order URL:" << url;
    }
}

void OnboardingFlowController::requestAdvancedSettings() {
    if (_currentStep != DriveSelection) {
        return;
    }

    qCInfo(lcOnboardingFlowController) << "Onboarding advanced settings requested";
    emit advancedSettingsRequested();
}

void OnboardingFlowController::requestDriveSelectionContinue() {
    if (_currentStep != DriveSelection) {
        return;
    }

    qCInfo(lcOnboardingFlowController) << "Onboarding drive selection continue requested";
    emit driveSelectionContinueRequested();
}

void OnboardingFlowController::retrySynchronization() {
    if (_currentStep != Synchronization || !_synchronizationFailed) {
        return;
    }

    qCInfo(lcOnboardingFlowController) << "Onboarding synchronization retry requested";
    _synchronizationFailed = false;
    emit synchronizationFailedChanged();
    emit synchronizationRetryRequested();
}

void OnboardingFlowController::openSynchronizedFolders() {
    if (_currentStep != Ready || !_readyActionEnabled) {
        return;
    }

    qCInfo(lcOnboardingFlowController) << "Opening synchronized folders from onboarding";
    _readyActionEnabled = false;
    emit readyActionEnabledChanged();
    emit synchronizedFoldersOpenRequested();
}

void OnboardingFlowController::cancel() {
    qCInfo(lcOnboardingFlowController) << "Onboarding cancel requested";
    emit cancelRequested();
}

void OnboardingFlowController::setCurrentStep(const Step step) {
    if (const auto index = stepToIndex(step); index < 0 || index >= stepCountValue) {
        qCWarning(lcOnboardingFlowController) << "Invalid onboarding step ignored:" << index;
        return;
    }

    if (_currentStep == step) {
        return;
    }

    const auto stepMetaEnum = QMetaEnum::fromType<Step>();
    qCInfo(lcOnboardingFlowController) << "Onboarding step changed |" << stepMetaEnum.valueToKey(_currentStep) << "=>"
                                       << stepMetaEnum.valueToKey(step);

    _currentStep = step;
    emit currentStepChanged();
}

void OnboardingFlowController::handleAuthorizationCodeReady() {
    if (_currentStep != Login) {
        return;
    }

    if (_loginState == LoginIdle || _loginState == WaitingForWebAuthentication || _loginState == LoginError) {
        setLoginState(LoadingUser);
    }
}

void OnboardingFlowController::beginSynchronization() {
    if (_synchronizationFailed) {
        _synchronizationFailed = false;
        emit synchronizationFailedChanged();
    }
    setCurrentStep(Synchronization);
}

void OnboardingFlowController::failSynchronization() {
    if (_currentStep != Synchronization || _synchronizationFailed) {
        return;
    }

    qCWarning(lcOnboardingFlowController) << "Onboarding synchronization creation failed";
    _synchronizationFailed = true;
    emit synchronizationFailedChanged();
}

void OnboardingFlowController::completeSynchronization() {
    if (_currentStep != Synchronization) {
        return;
    }

    if (_synchronizationFailed) {
        _synchronizationFailed = false;
        emit synchronizationFailedChanged();
    }
    if (!_readyActionEnabled) {
        _readyActionEnabled = true;
        emit readyActionEnabledChanged();
    }
    setCurrentStep(Ready);
}

void OnboardingFlowController::completeOnboarding() {
    if (_currentStep != Ready || _onboardingCompleted) {
        return;
    }

    qCInfo(lcOnboardingFlowController) << "Onboarding completed";
    _onboardingCompleted = true;
    emit completed();
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
