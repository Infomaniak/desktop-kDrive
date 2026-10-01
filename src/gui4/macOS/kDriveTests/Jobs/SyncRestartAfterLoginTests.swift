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

import Combine
import Foundation
@testable import InfomaniakDI
@testable import kDrive
@testable import kDriveCore
import Testing

extension SharedDITests {
    @MainActor
    @Suite(.timeLimit(.minutes(1)))
    struct SyncRestartAfterLoginTests {}
}

extension SharedDITests.SyncRestartAfterLoginTests {
    /// Fake server answering the login with `loggedUserDbId` and reporting every SYNC_START_AFTER_LOGIN user DB ID.
    private struct FakeServer: XPCConnectionProvider {
        let loggedUserDbId: Int32
        let startedUserDbIds: AsyncStream<Int32>.Continuation

        var guiConnectionState: XPCConnectionState {
            .connected
        }

        var guiConnectionStatePublisher: AnyPublisher<XPCConnectionState, Never> {
            Just(.connected).eraseToAnyPublisher()
        }

        var loginItemAgentConnectionState: XPCLoginItemAgentConnectionState {
            .connected
        }

        var loginItemAgentConnectionStatePublisher: AnyPublisher<XPCLoginItemAgentConnectionState, Never> {
            Just(.connected).eraseToAnyPublisher()
        }

        func reconnectToLoginAgent() async {}

        func sendQuery(_ requestData: Data) async throws -> Data {
            let request = try JSONDecoder().decode(RequestMessage<EmptyQuery>.self, from: requestData)
            switch request.num {
            case .LOGIN_REQUESTTOKEN:
                return reply(id: request.id, params: #"{"userDbId":\#(loggedUserDbId)}"#)
            case .SYNC_START_AFTER_LOGIN:
                let startRequest = try JSONDecoder().decode(RequestMessage<UserQuery>.self, from: requestData)
                startedUserDbIds.yield(startRequest.body.userDbId)
            default:
                Issue.record("Unexpected request: \(request.num)")
            }
            return reply(id: request.id, params: "{}")
        }

        /// Raw JSON `CallbackMessage` with `code: .Ok` and `cause: .Unknown`.
        private func reply(id: Int32, params: String) -> Data {
            Data(#"{"cause":0,"code":0,"id":\#(id),"params":\#(params)}"#.utf8)
        }
    }

    @Test("Successful login asks the server to restart the synchronizations of the logged user")
    func loginRestartsSynchronizations() async {
        // GIVEN
        let loggedUserDbId = Int32.random(in: 20001 ... 30000)
        let (startedUserDbIds, continuation) = AsyncStream<Int32>.makeStream()
        let server = FakeServer(loggedUserDbId: loggedUserDbId, startedUserDbIds: continuation)

        let resolver = SimpleResolver.sharedResolver
        let identifier = resolver.buildIdentifier(type: XPCQueryFetcherProtocol.self)
        let previousFactory = resolver.factories[identifier]
        let previousService = resolver.store[identifier]
        defer {
            resolver.factories[identifier] = previousFactory
            resolver.store[identifier] = previousService
        }
        // Replacing a factory alone does not evict the previously resolved singleton.
        resolver.store.removeValue(forKey: identifier)
        resolver.store(factory: Factory(type: XPCQueryFetcherProtocol.self) { _, _ in
            XPCQueryFetcher(xpcConnectionProvider: server)
        })

        let flowCoordinator = OnboardingFlowCoordinator(user: nil, steps: nil, initialStep: .login)
        let viewModel = LoginViewModel(flowCoordinator: flowCoordinator)

        // WHEN
        viewModel.didCompleteLoginWith(code: "code", verifier: "verifier")

        // THEN
        #expect(await startedUserDbIds.first { _ in true } == loggedUserDbId)
    }
}
