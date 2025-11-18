/*
 Infomaniak kDrive - Desktop
 Copyright (C) 2023-2025 Infomaniak Network SA

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import AuthenticationServices
import Combine
import Foundation
import InfomaniakDI
import InfomaniakLogin
import kDriveCore

enum OnboardingStep: Sendable {
    case login(LoginStep = .initial)
    case driveSelection
    case permissions
    case synchronisation

    enum LoginStep: Sendable {
        case initial
        case fail
    }
}

@MainActor
final class OnboardingViewModel: ObservableObject {
    @LazyInjectService private var loginService: InfomaniakLoginable

    @Published private(set) var currentStep = OnboardingStep.login()
    @Published private(set) var isShowingAuthenticationWindow = false

    func startWebAuthenticationLogin(anchor: ASPresentationAnchor?) {
        isShowingAuthenticationWindow = true
        loginService.asWebAuthenticationLoginFrom(
            anchor: anchor ?? ASPresentationAnchor(),
            useEphemeralSession: true,
            hideCreateAccountButton: true,
            delegate: self
        )
    }
}

// MARK: - InfomaniakLoginDelegate

extension OnboardingViewModel: InfomaniakLoginDelegate {
    func didCompleteLoginWith(code: String, verifier: String) {
        Task {
            do {
                try await LoginJob().login(code: code, verifier: verifier)

                // TODO: We need to know how many drives are available
//                let nextStep = getNextStepAfterLogin()
//                currentStep = nextStep
            } catch {
                handleLoginFailure(error: error)
            }

            isShowingAuthenticationWindow = false
        }
    }

    func didFailLoginWith(error: any Error) {
        isShowingAuthenticationWindow = false

        guard (error as? ASWebAuthenticationSessionError)?.code != .canceledLogin else {
            return
        }

        handleLoginFailure(error: error)
    }

    private func handleLoginFailure(error: Error) {
        currentStep = .login(.fail)
        SentryDebug.loginError(error: error)
    }

    private func computeNextStep(for user: User?) -> OnboardingStep {
        return .login()
    }
}
