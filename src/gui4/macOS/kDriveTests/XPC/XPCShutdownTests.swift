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

import Foundation
@testable import kDriveCore
import Testing

@MainActor
@Suite(.timeLimit(.minutes(1)))
struct XPCShutdownTests {
    @Test func shutdownCancelsRetriesAndRejectsLateConnectionAttempts() async throws {
        let manager = XPCConnectionManager()
        defer { manager.finishShutdown() }
        manager.scheduleRetryToConnectToLoginAgent()
        manager.scheduleRetryToConnectToServer()
        let agentRetry = try #require(manager.loginAgentRetryTask)
        let serverRetry = try #require(manager.serverRetryTask)

        // Shutdown wins before either queued retry or the initial connection can run.
        manager.beginShutdown()
        manager.beginShutdown()
        #expect(agentRetry.isCancelled)
        #expect(serverRetry.isCancelled)
        await agentRetry.value
        await serverRetry.value

        manager.scheduleRetryToConnectToLoginAgent()
        manager.scheduleRetryToConnectToServer()
        await manager.reconnectToLoginAgent()
        #expect(manager.loginAgentRetryTask == nil)
        #expect(manager.serverRetryTask == nil)
        #expect(manager.loginItemAgentConnection == nil)
        #expect(manager.appConnection == nil)

        await #expect(throws: XPCConnectionManager.XPCError.shuttingDown) {
            try await manager.connectToLoginAgent()
        }
        await #expect(throws: XPCConnectionManager.XPCError.shuttingDown) {
            try await manager.sendQuery(Data())
        }
        let listener = NSXPCListener.anonymous()
        defer { listener.invalidate() }
        #expect(throws: XPCConnectionManager.XPCError.shuttingDown) {
            try manager.connectToServer(endpoint: listener.endpoint)
        }
        manager.finishShutdown()
        manager.finishShutdown()
        #expect(manager.lifecycleState == .stopped)
    }

    @Test func shutdownIgnoresQueuedConnectionLossAndInvalidatesOnlyAtTheEnd() async {
        let manager = XPCConnectionManager()
        defer { manager.finishShutdown() }
        let listener = NSXPCListener.anonymous()
        defer { listener.invalidate() }
        let server = NSXPCConnection(listenerEndpoint: listener.endpoint)
        let agent = NSXPCConnection(listenerEndpoint: listener.endpoint)
        manager.appConnection = server
        manager.loginItemAgentConnection = agent

        manager.beginShutdown()
        await manager.serverConnectionLost(server, interrupted: true)
        await manager.serverConnectionLost(server, interrupted: false)
        manager.loginAgentConnectionLost(agent)

        // The quit request can still use the existing transport; callbacks must not turn it into a crash/retry.
        #expect(manager.appConnection === server)
        #expect(manager.loginItemAgentConnection === agent)
        #expect(manager.guiConnectionState == .notConnected)
        #expect(manager.serverRetryTask == nil)
        #expect(manager.loginAgentRetryTask == nil)

        manager.finishShutdown()
        await manager.serverConnectionLost(server, interrupted: true)
        manager.loginAgentConnectionLost(agent)
        #expect(manager.appConnection == nil)
        #expect(manager.loginItemAgentConnection == nil)
        #expect(manager.guiConnectionState == .notConnected)
        #expect(manager.lifecycleState == .stopped)
    }
}
