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

@MainActor
final class LoginViewModel: ObservableObject {
    enum LoginState {
        case idle
        case waitingForWebAuthentication
        case loadingUser
    }

    @LazyInjectService private var loginService: InfomaniakLoginable

    @Published private(set) var loginState: LoginState = .idle
    @Published var isShowingError = false

    private var bindStore = Set<AnyCancellable>()

    private let flowCoordinator: OnboardingFlowCoordinator

    init(flowCoordinator: OnboardingFlowCoordinator) {
        self.flowCoordinator = flowCoordinator
    }

    func startWebAuthenticationLogin(anchor: ASPresentationAnchor?) {
        loginState = .waitingForWebAuthentication
        loginService.asWebAuthenticationLoginFrom(
            anchor: anchor ?? ASPresentationAnchor(),
            useEphemeralSession: true,
            hideCreateAccountButton: true,
            delegate: self
        )
    }

    func openAccountRegistrationProcess() {
        // TODO: Handle account registration
    }

    private func handleConnectedUser(_ user: User?) {
        guard let user else {
            loginState = .idle
            return
        }

        flowCoordinator.navigate(to: .drivesSelection)
    }
}

extension LoginViewModel: InfomaniakLoginDelegate {
    func didCompleteLoginWith(code: String, verifier: String) {
        Task {
            do {
                loginState = .loadingUser
                let userDbId = try await LoginJob().login(code: code, verifier: verifier)

                @InjectService var coherentCacheObservable: CoherentCacheObservable
                coherentCacheObservable.usersPublisher.userPublisher(for: userDbId)
                    .receiveOnMain(store: &bindStore) { [weak self] user in
                        self?.handleConnectedUser(user)
                    }
            } catch {
                loginState = .idle
                handleLoginFailure(error: error)
            }
        }
    }

    func didFailLoginWith(error: any Error) {
        loginState = .idle

        guard (error as? ASWebAuthenticationSessionError)?.code != .canceledLogin else {
            return
        }

        handleLoginFailure(error: error)
    }

    private func handleLoginFailure(error: Error) {
        SentryDebug.loginError(error: error)
        isShowingError = true
    }
}
