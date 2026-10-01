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
    /// Records the SYNC_START_AFTER_LOGIN requests sent to the fake server.
    private final class RequestRecorder: @unchecked Sendable {
        private let lock = NSLock()
        private var storedUserDbId: Int32?

        var startAfterLoginUserDbId: Int32? {
            lock.lock()
            defer { lock.unlock() }
            return storedUserDbId
        }

        func recordStartAfterLogin(userDbId: Int32) {
            lock.lock()
            defer { lock.unlock() }
            storedUserDbId = userDbId
        }
    }

    private struct RestartSyncConnectionProvider: XPCConnectionProvider, @unchecked Sendable {
        let recorder: RequestRecorder
        let expectedUserDbId: Int32

        var guiConnectionState: XPCConnectionState { .connected }
        var guiConnectionStatePublisher: AnyPublisher<XPCConnectionState, Never> { Just(.connected).eraseToAnyPublisher() }
        var loginItemAgentConnectionState: XPCLoginItemAgentConnectionState { .connected }
        var loginItemAgentConnectionStatePublisher: AnyPublisher<XPCLoginItemAgentConnectionState, Never> {
            Just(.connected).eraseToAnyPublisher()
        }

        func reconnectToLoginAgent() async {}

        func sendQuery(_ requestData: Data) async throws -> Data {
            let request = try JSONDecoder().decode(RequestMessage<EmptyQuery>.self, from: requestData)

            switch request.num {
            case .LOGIN_REQUESTTOKEN:
                let loginResponse = LoginResponse(userDbId: expectedUserDbId)
                return try JSONEncoder().encode(CallbackMessage<LoginResponse>(
                    code: .Ok,
                    cause: .Unknown,
                    id: request.id,
                    body: loginResponse
                ))
            case .SYNC_START_AFTER_LOGIN:
                let startRequest = try JSONDecoder().decode(RequestMessage<UserQuery>.self, from: requestData)
                await recorder.recordStartAfterLogin(userDbId: startRequest.body.userDbId)
                return try JSONEncoder().encode(CallbackMessage(
                    code: .Ok,
                    cause: .Unknown,
                    id: request.id,
                    body: EmptyResponse()
                ))
            default:
                Issue.record("Unexpected request sent to the server: \(request.num)")
                return try JSONEncoder().encode(CallbackMessage(
                    code: .Ok,
                    cause: .Unknown,
                    id: request.id,
                    body: EmptyResponse()
                ))
            }
        }
    }

    private final class Fixture {
        let recorder = RequestRecorder()
        let provider: RestartSyncConnectionProvider

        private var restorations: [() -> Void] = []

        init(expectedUserDbId: Int32 = 42) async throws {
            provider = RestartSyncConnectionProvider(recorder: recorder, expectedUserDbId: expectedUserDbId)

            let cache = ServerCoherentCache()
            register(XPCQueryFetcherProtocol.self, service: XPCQueryFetcher(xpcConnectionProvider: provider))
            register(CoherentCache.self, service: cache)
            register(CoherentCacheObservable.self, service: cache)
        }

        deinit {
            restorations.reversed().forEach { $0() }
        }

        private func register<Service>(_ type: Service.Type, service: Service) {
            let resolver = SimpleResolver.sharedResolver
            let identifier = resolver.buildIdentifier(type: type)
            let previousFactory = resolver.factories[identifier]
            let previousService = resolver.store[identifier]
            restorations.append {
                resolver.factories[identifier] = previousFactory
                resolver.store[identifier] = previousService
            }
            // Replacing a factory alone does not evict the previously resolved singleton.
            resolver.store.removeValue(forKey: identifier)
            resolver.store(factory: Factory(type: type) { _, _ in service })
        }
    }

    @MainActor
    private func waitFor(
        _ condition: @autoclosure @MainActor () -> Bool,
        timeout: TimeInterval = 5,
        _ message: @autoclosure @MainActor () -> String = "Condition not met in time"
    ) async {
        let deadline = Date().addingTimeInterval(timeout)
        while !condition() && Date() < deadline {
            await Task.yield()
            try? await Task.sleep(for: .milliseconds(10))
        }
        #expect(condition(), Comment(stringLiteral: message()))
    }

    @Test("Successful login asks the server to restart the user synchronizations")
    func loginTriggersSyncRestart() async throws {
        // GIVEN
        let expectedUserDbId = Int32.random(in: 20001 ... 30000)
        let fixture = try await Fixture(expectedUserDbId: expectedUserDbId)
        let flowCoordinator = OnboardingFlowCoordinator(user: nil, steps: nil, initialStep: .login)
        let viewModel = LoginViewModel(flowCoordinator: flowCoordinator)

        // WHEN
        viewModel.didCompleteLoginWith(code: "123", verifier: "456")

        // THEN
        await waitFor(
            fixture.recorder.startAfterLoginUserDbId == expectedUserDbId,
            "The SYNC_START_AFTER_LOGIN job should be sent with the logged user db id"
        )
    }
}
