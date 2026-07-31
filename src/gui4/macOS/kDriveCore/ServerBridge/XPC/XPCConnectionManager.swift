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
import InfomaniakDI

@objc final class XPCConnectionManager: NSObject, @unchecked Sendable {
    @LazyInjectService var signalProcessor: SignalProcessing
    @LazyInjectService var coherentCache: CoherentCache
    @LazyInjectService var settingsCache: SettingsCaching

    @MainActor
    @Published private(set) var guiConnectionState: XPCConnectionState = .notConnected

    @MainActor
    @Published private(set) var loginItemAgentConnectionState: XPCLoginItemAgentConnectionState = .connecting

    private static let retryDelayNanoseconds: UInt64 = 10_000_000_000

    // Single-flight handles: at most one retry loop of each kind runs at a time.
    @MainActor private var loginAgentRetryTask: Task<Void, Never>?
    @MainActor private var serverRetryTask: Task<Void, Never>?

    let machServiceName: String

    var loginItemAgentConnection: NSXPCConnection?
    var appConnection: NSXPCConnection?

    enum XPCError: Error {
        case noAppConnectionAvailable
        case noLoginItemAgentConnection
        case serverGUIEndpointWasNil
    }

    override init() {
        guard let loginItemAgentMachName = Bundle.main.object(forInfoDictionaryKey: "LoginItemAgentMachName") as? String else {
            fatalError("Malformed info.plist, missing LoginItemAgentMachName")
        }

        IKLogger.xpc.log("[KD] mach name: \(loginItemAgentMachName)")
        machServiceName = loginItemAgentMachName

        super.init()

        Task {
            IKLogger.xpc.log("[KD] initial connection to login item agent")
            do {
                try await connectToLoginAgent()
            } catch {
                IKLogger.xpc.error("[KD] initial connectToLoginAgent FAILED \(error)")
            }
        }
    }

    deinit {
        loginItemAgentConnection?.invalidate()
        appConnection?.invalidate()
    }

    func scheduleRetryToConnectToLoginAgent() {
        Task { @MainActor [weak self] in
            guard let self else { return }
            guard loginAgentRetryTask == nil else {
                IKLogger.xpc.log("[KD] Login item agent retry loop already running")
                return
            }
            loginAgentRetryTask = Task.detached { [weak self] in
                guard let self else { return }
                await retryToConnectToLoginAgentLoop()
            }
        }
    }

    func scheduleRetryToConnectToServer() {
        Task { @MainActor [weak self] in
            guard let self else { return }
            guard serverRetryTask == nil else {
                IKLogger.xpc.log("[KD] Server retry loop already running")
                return
            }
            serverRetryTask = Task.detached { [weak self] in
                guard let self else { return }
                await retryToConnectToServerLoop()
            }
        }
    }

    /// Keeps trying to reach the login item agent until the connection is (re)established.
    private func retryToConnectToLoginAgentLoop() async {
        while !Task.isCancelled {
            IKLogger.xpc.log("[KD] Set timer to retry to connect to login agent")
            try? await Task.sleep(nanoseconds: Self.retryDelayNanoseconds)
            guard !Task.isCancelled else { break }

            if loginItemAgentConnection != nil {
                break
            }

            do {
                try await connectToLoginAgent()
                break
            } catch XPCError.serverGUIEndpointWasNil {
                // Agent reachable but server not registered yet; connectToLoginAgent started the server loop.
                break
            } catch {
                IKLogger.xpc.log("[KD] Login item agent still unreachable, will retry: \(error)")
            }
        }

        await MainActor.run { [weak self] in
            self?.loginAgentRetryTask = nil
        }
    }

    /// Polls the login item agent for the server endpoint until the server has registered it.
    private func retryToConnectToServerLoop() async {
        while !Task.isCancelled {
            IKLogger.xpc.log("[KD] Set timer to retry to connect to server")
            try? await Task.sleep(nanoseconds: Self.retryDelayNanoseconds)
            guard !Task.isCancelled else { break }

            do {
                try await fetchServerEndpointFromLoginItemAgentAndConnectIfNeeded()
                IKLogger.xpc.log("[KD] Reconnected to server through the login item agent")
                notifyLoginItemAgentConnectionState(.connected)
                break
            } catch {
                IKLogger.xpc.log("[KD] Server still unreachable, will retry: \(error)")
            }
        }

        await MainActor.run { [weak self] in
            self?.serverRetryTask = nil
        }
    }

    func connectToLoginAgent() async throws {
        guard loginItemAgentConnection == nil else {
            IKLogger.xpc.log("[KD] Already connected to item agent")
            throw XPCError.noLoginItemAgentConnection
        }

        IKLogger.xpc.log("[KD] Initialize connection with login item agent")
        let connection = NSXPCConnection(machServiceName: machServiceName, options: [])

        loginItemAgentConnection = connection

        IKLogger.xpc.log("[KD] Set exported interface for connection with login agent")
        connection.exportedInterface = NSXPCInterface(with: XPCLoginItemRemoteProtocol.self)
        connection.exportedObject = self

        IKLogger.xpc.log("[KD] Set remote object interface for connection with login agent")
        connection.remoteObjectInterface = NSXPCInterface(with: XPCLoginItemProtocol.self)

        IKLogger.xpc.log("[KD] Set connection handlers for connection with login item agent")
        connection.interruptionHandler = { [weak self] in
            IKLogger.xpc.error("[KD] Connection with login item agent interrupted (server crash)")
            guard let self else { return }
            loginItemAgentConnection = nil
            notifyLoginItemAgentConnectionState(.disconnected)
            scheduleRetryToConnectToLoginAgent()
        }

        connection.invalidationHandler = { [weak self] in
            IKLogger.xpc.error("[KD] Connection with login item agent invalidated (no server running)")
            guard let self else { return }
            loginItemAgentConnection = nil
            notifyLoginItemAgentConnectionState(.disconnected)
            scheduleRetryToConnectToLoginAgent()
        }

        IKLogger.xpc.log("[KD] Resume connection with login item agent")
        connection.resume()

        do {
            try await fetchServerEndpointFromLoginItemAgentAndConnectIfNeeded()
        } catch XPCError.serverGUIEndpointWasNil {
            notifyLoginItemAgentConnectionState(.connecting)
            scheduleRetryToConnectToServer()
            throw XPCError.serverGUIEndpointWasNil
        }

        notifyLoginItemAgentConnectionState(.connected)
    }

    public func reconnectToLoginAgent() async {
        IKLogger.xpc.log("[KD] Reconnect to login item agent requested")
        do {
            if loginItemAgentConnection == nil {
                try await connectToLoginAgent()
            } else {
                try await fetchServerEndpointFromLoginItemAgentAndConnectIfNeeded()
                notifyLoginItemAgentConnectionState(.connected)
            }
        } catch XPCError.serverGUIEndpointWasNil {
            IKLogger.xpc.log("[KD] reconnectToLoginAgent: agent reachable, server not ready yet")
            notifyLoginItemAgentConnectionState(.connecting)
            scheduleRetryToConnectToServer()
        } catch {
            IKLogger.xpc.error("[KD] reconnectToLoginAgent FAILED \(error)")
            notifyLoginItemAgentConnectionState(.disconnected)
        }
    }

    private func notifyLoginItemAgentConnectionState(_ state: XPCLoginItemAgentConnectionState) {
        Task { @MainActor [weak self] in
            self?.loginItemAgentConnectionState = state
        }
    }

    func fetchServerEndpointFromLoginItemAgentAndConnectIfNeeded() async throws {
        guard appConnection == nil else {
            return
        }

        let endpoint = try await getServerEndpoint()
        try connectToServer(endpoint: endpoint)
    }

    func getServerEndpoint() async throws -> NSXPCListenerEndpoint {
        guard let loginItemAgentConnection,
              let loginItemProxy = loginItemAgentConnection.remoteObjectProxy as? XPCLoginItemProtocol else {
            throw XPCError.noLoginItemAgentConnection
        }

        IKLogger.xpc.log("[KD] Get server gui endpoint from login item agent")

        return try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<NSXPCListenerEndpoint, Error>) in
            loginItemProxy.serverGuiEndpoint { endpoint in
                IKLogger.xpc.log("[KD] Server gui endpoint received: \(endpoint != nil)")
                if let endpoint {
                    continuation.resume(returning: endpoint)
                } else {
                    IKLogger.xpc.error("[KD] endpoint nil")
                    continuation.resume(throwing: XPCError.serverGUIEndpointWasNil)
                }
            }
        }
    }

    func connectToServer(endpoint: NSXPCListenerEndpoint) throws {
        guard appConnection == nil else {
            IKLogger.xpc.log("[KD] Already connected to app")
            throw XPCError.noLoginItemAgentConnection
        }

        Task { @MainActor in
            guiConnectionState = .notConnected
        }

        IKLogger.xpc.log("[KD] Setup connection with app")
        let newConnection = NSXPCConnection(listenerEndpoint: endpoint)
        appConnection = newConnection

        IKLogger.xpc.log("[KD] Set server -> gui interface")
        newConnection.exportedInterface = NSXPCInterface(with: XPCGuiRemoteProtocol.self)
        newConnection.exportedObject = self

        IKLogger.xpc.log("[KD] Set gui -> server interface")
        newConnection.remoteObjectInterface = NSXPCInterface(with: XPCGuiProtocol.self)

        IKLogger.xpc.log("[KD] Setup connection handlers for connection with app")
        newConnection.interruptionHandler = { [weak self] in
            IKLogger.xpc.error("[KD] Connection with app interrupted (server crash)")
            guard let self else { return }
            appConnection?.invalidate()
            appConnection = nil
            Task { @MainActor [weak self] in
                self?.guiConnectionState = .serverCrashed
            }
        }

        newConnection.invalidationHandler = { [weak self] in
            IKLogger.xpc.error("[KD] Connection with app invalidated (no server running)")
            guard let self else { return }
            appConnection?.invalidate()
            appConnection = nil
            scheduleRetryToConnectToServer()
            Task { @MainActor [weak self] in
                self?.guiConnectionState = .error
            }
        }

        newConnection.resume()

        let connectionId = ObjectIdentifier(newConnection)
        Task {
            IKLogger.xpc.log("[KD] coherentCache.clearAndRefresh")
            try await coherentCache.clearAndRefresh()
            try? await settingsCache.refresh()
            await MainActor.run { [weak self] in
                guard let self, let conn = appConnection else { return }
                let currentId = ObjectIdentifier(conn)
                if currentId == connectionId {
                    guiConnectionState = .connected
                }
            }
        }
    }
}

extension XPCConnectionManager: XPCLoginItemRemoteProtocol {
    func processType(_ callback: @escaping (ProcessType) -> Void) {
        IKLogger.xpc.log("[KD] query processType")
        callback(ProcessType.client)
    }

    func serverIsRunning(_ endPoint: NSXPCListenerEndpoint?) {
        IKLogger.xpc.log("[KD] serverIsRunning")
        guard let endPoint else {
            IKLogger.xpc.error("[KD] server sent a nil endpoint")
            return
        }
        try? connectToServer(endpoint: endPoint)
    }
}

extension XPCConnectionManager: XPCGuiRemoteProtocol {
    func processSignal(_ msg: Data) {
        signalProcessor.enqueue(msg)
    }
}
