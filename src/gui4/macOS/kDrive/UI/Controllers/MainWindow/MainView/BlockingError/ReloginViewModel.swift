/*
 Infomaniak kDrive - Desktop
 Copyright (C) 2023-2026 Infomaniak Network SA

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

enum ReloginResult: Equatable {
    case success
    case cancelled
    case userMismatch(email: String)
    case failure
}

/// Re-authenticates the user of a synchro directly in the browser, without going through the onboarding.
@MainActor
final class ReloginViewModel: ObservableObject {
    private struct PendingRelogin {
        let expectedUserDbId: Int32
        let continuation: CheckedContinuation<ReloginResult, Never>
    }

    @LazyInjectService private var loginService: InfomaniakLoginable
    @LazyInjectService private var coherentCache: CoherentCache

    @Published private var pendingRelogin: PendingRelogin?

    var isInProgress: Bool {
        pendingRelogin != nil
    }

    func relogin(synchroDbId: Int32, anchor: ASPresentationAnchor?) async -> ReloginResult {
        guard !isInProgress else {
            return .cancelled
        }

        guard let expectedUserDbId = await coherentCache.getSynchroContext(synchroDbId)?.drive.userDbId else {
            IKLogger.general.error("[KD] Relogin impossible, synchro not found synchroDbId=\(synchroDbId)")
            return .failure
        }

        return await withCheckedContinuation { continuation in
            pendingRelogin = PendingRelogin(expectedUserDbId: expectedUserDbId, continuation: continuation)
            loginService.asWebAuthenticationLoginFrom(
                anchor: anchor ?? ASPresentationAnchor(),
                useEphemeralSession: false,
                hideCreateAccountButton: true,
                delegate: self
            )
        }
    }

    private func finish(with result: ReloginResult) {
        pendingRelogin?.continuation.resume(returning: result)
        pendingRelogin = nil
    }

    private func completeRelogin(code: String, verifier: String) async -> ReloginResult {
        guard let expectedUserDbId = pendingRelogin?.expectedUserDbId else {
            return .failure
        }

        do {
            let userDbId = try await LoginJob().login(code: code, verifier: verifier)
            guard userDbId == expectedUserDbId else {
                IKLogger.general.info("[KD] Relogin with another account userDbId=\(userDbId) expected=\(expectedUserDbId)")
                let email = await coherentCache.getUser(dbId: expectedUserDbId)?.email ?? ""
                return .userMismatch(email: email)
            }

            try await SyncJobs().syncStartAfterLoginJob(userDbId: userDbId)
            return .success
        } catch {
            SentryDebug.loginError(error: error)
            return .failure
        }
    }
}

extension ReloginViewModel: InfomaniakLoginDelegate {
    func didCompleteLoginWith(code: String, verifier: String) {
        Task {
            let result = await completeRelogin(code: code, verifier: verifier)
            finish(with: result)
        }
    }

    func didFailLoginWith(error: any Error) {
        guard (error as? ASWebAuthenticationSessionError)?.code != .canceledLogin else {
            finish(with: .cancelled)
            return
        }

        SentryDebug.loginError(error: error)
        finish(with: .failure)
    }
}
