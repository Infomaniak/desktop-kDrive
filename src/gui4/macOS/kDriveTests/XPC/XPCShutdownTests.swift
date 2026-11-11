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
    @Test(arguments: [true, false])
    func quitBeforeInitialServerConnectionUsesShutdownOnlyTransport(serverReplies: Bool) async throws {
        let server = ShutdownServer(replies: serverReplies)
        let serverListener = NSXPCListener.anonymous()
        let serverDelegate = ShutdownListenerDelegate(interface: XPCGuiProtocol.self, service: server)
        serverListener.delegate = serverDelegate
        serverListener.resume()
        defer { serverListener.invalidate() }

        let agent = ShutdownAgent(endpoint: serverListener.endpoint)
        let agentListener = NSXPCListener.anonymous()
        let agentDelegate = ShutdownListenerDelegate(interface: XPCLoginItemProtocol.self, service: agent)
        agentListener.delegate = agentDelegate
        agentListener.resume()
        defer { agentListener.invalidate() }

        let manager = XPCConnectionManager()
        defer { manager.finishShutdown() }
        manager.beginShutdown()
        let connection = NSXPCConnection(listenerEndpoint: agentListener.endpoint)
        connection.remoteObjectInterface = NSXPCInterface(with: XPCLoginItemProtocol.self)
        manager.loginItemAgentConnection = connection
        connection.resume()
        #expect(manager.appConnection == nil)

        let request = await RequestMessage<EmptyQuery>(num: .UTILITY_QUIT, body: EmptyQuery())
        let fetcher = XPCQueryFetcher(xpcConnectionProvider: manager)
        if serverReplies {
            let reply = try await fetcher.query(request, responseType: CallbackMessage<EmptyResponse>.self)
            #expect(reply.id == request.id)
        } else {
            await #expect(throws: NSXPCConnectionError.timedOut) {
                try await fetcher.query(request, responseType: CallbackMessage<EmptyResponse>.self)
            }
        }
        #expect(manager.appConnection != nil)
        #expect(manager.guiConnectionState == .notConnected)
        #expect(manager.loginAgentRetryTask == nil)
        #expect(manager.serverRetryTask == nil)

        // Even after the quit transport exists, ordinary work must remain disabled.
        await #expect(throws: XPCConnectionManager.XPCError.shuttingDown) {
            try await manager.sendQuery(Data())
        }
        withExtendedLifetime((serverDelegate, agentDelegate)) {}
    }

    @Test func quitWithUnresponsiveAgentTimesOutWithoutConnectingOrRetrying() async {
        let agent = ShutdownAgent(endpoint: nil)
        let listener = NSXPCListener.anonymous()
        let delegate = ShutdownListenerDelegate(interface: XPCLoginItemProtocol.self, service: agent)
        listener.delegate = delegate
        listener.resume()
        defer { listener.invalidate() }
        let manager = XPCConnectionManager()
        defer { manager.finishShutdown() }
        manager.beginShutdown()
        let connection = NSXPCConnection(listenerEndpoint: listener.endpoint)
        connection.remoteObjectInterface = NSXPCInterface(with: XPCLoginItemProtocol.self)
        manager.loginItemAgentConnection = connection
        connection.resume()

        await #expect(throws: NSXPCConnectionError.timedOut) {
            try await manager.sendQuitQuery(Data())
        }
        #expect(manager.appConnection == nil)
        #expect(manager.serverRetryTask == nil)
        #expect(manager.loginAgentRetryTask == nil)
        withExtendedLifetime(delegate) {}
    }

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

private final class ShutdownListenerDelegate: NSObject, NSXPCListenerDelegate {
    let interface: Protocol
    let service: AnyObject

    init(interface: Protocol, service: AnyObject) {
        self.interface = interface
        self.service = service
    }

    func listener(_ listener: NSXPCListener, shouldAcceptNewConnection connection: NSXPCConnection) -> Bool {
        connection.exportedInterface = NSXPCInterface(with: interface)
        connection.exportedObject = service
        connection.resume()
        return true
    }
}

private final class ShutdownAgent: NSObject, XPCLoginItemProtocol {
    let endpoint: NSXPCListenerEndpoint?

    init(endpoint: NSXPCListenerEndpoint?) {
        self.endpoint = endpoint
    }

    func setServerExtEndpoint(_ endPoint: NSXPCListenerEndpoint!) {}
    func serverExtEndpoint(_ callback: ((NSXPCListenerEndpoint?) -> Void)!) {}
    func setServerGuiEndpoint(_ endPoint: NSXPCListenerEndpoint!) {}
    func serverGuiEndpoint(_ callback: ((NSXPCListenerEndpoint?) -> Void)!) {
        // A nil endpoint deliberately models an agent that never answers.
        if let endpoint { callback(endpoint) }
    }
}

private final class ShutdownServer: NSObject, XPCGuiProtocol {
    let replies: Bool

    init(replies: Bool) {
        self.replies = replies
    }

    func processQuery(_ query: Data, callback: @escaping (Data) -> Void) {
        do {
            let request = try JSONDecoder().decode(RequestMock.self, from: query)
            #expect(request.num == .UTILITY_QUIT)
            guard replies else { return }
            let reply: [String: Any] = ["code": 0, "cause": 0, "id": request.id, "params": [:]]
            try callback(JSONSerialization.data(withJSONObject: reply))
        } catch {
            Issue.record(error)
            callback(Data())
        }
    }
}
