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
    // XPC invokes the Objective-C entry points off-actor; all mutable connection state is isolated to MainActor.
    @LazyInjectService var signalProcessor: SignalProcessing
    @MainActor @LazyInjectService var coherentCache: CoherentCache
    @MainActor @LazyInjectService var settingsCache: SettingsCaching
    @MainActor @LazyInjectService var vfsConversionCache: VFSConversionCaching

    @MainActor
    @Published private(set) var guiConnectionState: XPCConnectionState = .notConnected

    @MainActor
    @Published private(set) var loginItemAgentConnectionState: XPCLoginItemAgentConnectionState = .connecting

    private static let retryDelayNanoseconds: UInt64 = 10_000_000_000

    // Single-flight handles: at most one retry loop of each kind runs at a time.
    @MainActor private(set) var loginAgentRetryTask: Task<Void, Never>?
    @MainActor private(set) var serverRetryTask: Task<Void, Never>?

    enum LifecycleState {
        case running
        case shuttingDown
        case stopped
    }

    @MainActor private(set) var lifecycleState: LifecycleState = .running

    let machServiceName: String

    @MainActor var loginItemAgentConnection: NSXPCConnection?
    @MainActor var appConnection: NSXPCConnection?

    enum XPCError: Error, Equatable {
        case noAppConnectionAvailable
        case noLoginItemAgentConnection
        case serverGUIEndpointWasNil
        case shuttingDown
    }

    override init() {
        guard let loginItemAgentMachName = Bundle.main.object(forInfoDictionaryKey: "LoginItemAgentMachName") as? String else {
            fatalError("Malformed info.plist, missing LoginItemAgentMachName")
        }

        IKLogger.xpc.log("[KD] mach name: \(loginItemAgentMachName)")
        machServiceName = loginItemAgentMachName

        super.init()

        Task { @MainActor in
            guard lifecycleState == .running else { return }
            IKLogger.xpc.log("[KD] initial connection to login item agent")
            do {
                try await connectToLoginAgent()
            } catch XPCError.serverGUIEndpointWasNil {
                IKLogger.xpc.debug("[KD] Login item agent reachable; waiting for server registration")
            } catch {
                IKLogger.xpc.error("[KD] initial connectToLoginAgent FAILED \(error)")
            }
        }
    }

    @MainActor
    func scheduleRetryToConnectToLoginAgent() {
        guard lifecycleState == .running else { return }
        guard loginAgentRetryTask == nil else {
            IKLogger.xpc.log("[KD] Login item agent retry loop already running")
            return
        }
        loginAgentRetryTask = Task { [weak self] in
            guard let self else { return }
            await retryToConnectToLoginAgentLoop()
        }
    }

    @MainActor
    func scheduleRetryToConnectToServer() {
        guard lifecycleState == .running else { return }
        guard serverRetryTask == nil else {
            IKLogger.xpc.log("[KD] Server retry loop already running")
            return
        }
        serverRetryTask = Task { [weak self] in
            guard let self else { return }
            await retryToConnectToServerLoop()
        }
    }

    /// Keeps trying to reach the login item agent until the connection is (re)established.
    @MainActor
    private func retryToConnectToLoginAgentLoop() async {
        defer { loginAgentRetryTask = nil }
        while lifecycleState == .running && !Task.isCancelled {
            IKLogger.xpc.log("[KD] Set timer to retry to connect to login agent")
            try? await Task.sleep(nanoseconds: Self.retryDelayNanoseconds)
            guard lifecycleState == .running && !Task.isCancelled else { break }

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
    }

    /// Polls the login item agent for the server endpoint until the server has registered it.
    @MainActor
    private func retryToConnectToServerLoop() async {
        defer { serverRetryTask = nil }
        while lifecycleState == .running && !Task.isCancelled {
            IKLogger.xpc.log("[KD] Set timer to retry to connect to server")
            try? await Task.sleep(nanoseconds: Self.retryDelayNanoseconds)
            guard lifecycleState == .running && !Task.isCancelled else { break }

            do {
                try await fetchServerEndpointFromLoginItemAgentAndConnectIfNeeded()
                guard lifecycleState == .running else { break }
                IKLogger.xpc.info("[KD] Server transport restored through the login item agent; initializing client caches")
                notifyLoginItemAgentConnectionState(.connected)
                break
            } catch {
                IKLogger.xpc.log("[KD] Server still unreachable, will retry: \(error)")
            }
        }
    }

    @MainActor
    func connectToLoginAgent() async throws {
        guard lifecycleState == .running else { throw XPCError.shuttingDown }
        guard loginItemAgentConnection == nil else {
            IKLogger.xpc.log("[KD] Already connected to item agent")
            throw XPCError.noLoginItemAgentConnection
        }

        initializeLoginItemAgentConnection()

        do {
            try await fetchServerEndpointFromLoginItemAgentAndConnectIfNeeded()
        } catch XPCError.serverGUIEndpointWasNil {
            notifyLoginItemAgentConnectionState(.connecting)
            scheduleRetryToConnectToServer()
            throw XPCError.serverGUIEndpointWasNil
        }

        notifyLoginItemAgentConnectionState(.connected)
    }

    @MainActor
    func initializeLoginItemAgentConnection() {
        IKLogger.xpc.log("[KD] Initialize connection with login item agent")
        let connection = NSXPCConnection(machServiceName: machServiceName, options: [])

        loginItemAgentConnection = connection

        IKLogger.xpc.log("[KD] Set exported interface for connection with login agent")
        connection.exportedInterface = NSXPCInterface(with: XPCLoginItemRemoteProtocol.self)
        connection.exportedObject = self

        IKLogger.xpc.log("[KD] Set remote object interface for connection with login agent")
        connection.remoteObjectInterface = NSXPCInterface(with: XPCLoginItemProtocol.self)

        IKLogger.xpc.log("[KD] Set connection handlers for connection with login item agent")
        connection.interruptionHandler = { [weak self, weak connection] in
            Task { @MainActor in
                self?.loginAgentConnectionLost(connection)
            }
        }

        connection.invalidationHandler = { [weak self, weak connection] in
            Task { @MainActor in
                self?.loginAgentConnectionLost(connection)
            }
        }

        IKLogger.xpc.log("[KD] Resume connection with login item agent")
        connection.resume()
    }

    @MainActor
    func reconnectToLoginAgent() async {
        guard lifecycleState == .running else { return }
        IKLogger.xpc.info("[KD] Reconnect to login item agent requested")
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

    @MainActor
    private func notifyLoginItemAgentConnectionState(_ state: XPCLoginItemAgentConnectionState) {
        guard lifecycleState == .running else { return }
        loginItemAgentConnectionState = state
    }

    @MainActor
    func fetchServerEndpointFromLoginItemAgentAndConnectIfNeeded() async throws {
        guard appConnection == nil else {
            return
        }
        guard lifecycleState == .running else { throw XPCError.shuttingDown }

        let endpoint = try await getServerEndpoint()
        try connectToServer(endpoint: endpoint)
    }

    @MainActor
    func getServerEndpoint(shutdownDeadline: DispatchTime? = nil) async throws -> NSXPCListenerEndpoint {
        guard lifecycleState == .running || (lifecycleState == .shuttingDown && shutdownDeadline != nil) else {
            throw XPCError.shuttingDown
        }
        guard let loginItemAgentConnection else {
            throw XPCError.noLoginItemAgentConnection
        }

        IKLogger.xpc.log("[KD] Get server gui endpoint from login item agent")

        return try await XPCContinuation<NSXPCListenerEndpoint>.perform(deadline: shutdownDeadline) { continuation in
            do {
                let loginItemProxy = try loginItemAgentConnection.proxy(errorHandler: { error in
                    IKLogger.xpc.error("[KD] Failed to get server gui endpoint: \(error)")
                    continuation.resume(throwing: error)
                }, type: XPCLoginItemProtocol.self)
                loginItemProxy.serverGuiEndpoint { endpoint in
                    IKLogger.xpc.log("[KD] Server gui endpoint received: \(endpoint != nil)")
                    if let endpoint {
                        continuation.resume(returning: endpoint)
                    } else {
                        IKLogger.xpc.debug("[KD] Server endpoint not registered yet")
                        continuation.resume(throwing: XPCError.serverGUIEndpointWasNil)
                    }
                }
            } catch {
                continuation.resume(throwing: error)
            }
        }
    }

    @MainActor
    func connectToServer(endpoint: NSXPCListenerEndpoint, forShutdown: Bool = false) throws {
        guard lifecycleState == .running || (lifecycleState == .shuttingDown && forShutdown) else {
            throw XPCError.shuttingDown
        }
        guard appConnection == nil else {
            IKLogger.xpc.log("[KD] Already connected to app")
            throw XPCError.noLoginItemAgentConnection
        }

        guiConnectionState = .notConnected

        IKLogger.xpc.log("[KD] Setup connection with app")
        let newConnection = NSXPCConnection(listenerEndpoint: endpoint)
        appConnection = newConnection

        IKLogger.xpc.log("[KD] Set server -> gui interface")
        newConnection.exportedInterface = NSXPCInterface(with: XPCGuiRemoteProtocol.self)
        newConnection.exportedObject = self

        IKLogger.xpc.log("[KD] Set gui -> server interface")
        newConnection.remoteObjectInterface = NSXPCInterface(with: XPCGuiProtocol.self)

        IKLogger.xpc.log("[KD] Setup connection handlers for connection with app")
        newConnection.interruptionHandler = { [weak self, weak newConnection] in
            Task { @MainActor in
                await self?.serverConnectionLost(newConnection, interrupted: true)
            }
        }

        newConnection.invalidationHandler = { [weak self, weak newConnection] in
            Task { @MainActor in
                await self?.serverConnectionLost(newConnection, interrupted: false)
            }
        }

        newConnection.resume()

        // A quit-only transport must not trigger cache initialization or publish client readiness.
        guard !forShutdown else { return }

        let connectionId = ObjectIdentifier(newConnection)
        Task {
            guard lifecycleState == .running else { return }
            IKLogger.xpc.info("[KD] Client cache initialization started")
            do {
                try await coherentCache.clearAndRefresh()
            } catch {
                IKLogger.xpc.error("[KD] Client cache initialization failed phase=coherentCache clientReady=false")
                return
            }
            guard lifecycleState == .running else { return }
            let settingsReady: Bool
            do {
                try await settingsCache.refresh()
                settingsReady = true
            } catch {
                settingsReady = false
                IKLogger.xpc.warning("[KD] Client settings refresh failed; retaining last-known configuration")
            }
            await MainActor.run { [weak self] in
                guard let self, lifecycleState == .running,
                      let conn = appConnection, ObjectIdentifier(conn) == connectionId else {
                    IKLogger.xpc.debug("[KD] Client cache initialization discarded: connection no longer current")
                    return
                }
                IKLogger.xpc.info("[KD] Client cache initialization completed clientReady=true settingsReady=\(settingsReady)")
                guiConnectionState = .connected
            }
        }
    }

    @MainActor
    func loginAgentConnectionLost(_ connection: NSXPCConnection?) {
        guard lifecycleState == .running, let connection, loginItemAgentConnection === connection else { return }
        IKLogger.xpc.warning("[KD] Connection with login item agent lost; scheduling reconnection")
        loginItemAgentConnection = nil
        connection.interruptionHandler = nil
        connection.invalidationHandler = nil
        connection.invalidate()
        notifyLoginItemAgentConnectionState(.disconnected)
        scheduleRetryToConnectToLoginAgent()
    }

    @MainActor
    func serverConnectionLost(_ connection: NSXPCConnection?, interrupted: Bool) async {
        guard lifecycleState == .running, let connection, appConnection === connection else { return }
        IKLogger.xpc.warning("[KD] Server connection lost (possible server crash)")
        appConnection = nil
        connection.interruptionHandler = nil
        connection.invalidationHandler = nil
        connection.invalidate()
        scheduleRetryToConnectToServer()
        await vfsConversionCache.clear()
        guard lifecycleState == .running, appConnection == nil else { return }
        guiConnectionState = interrupted ? .serverCrashed : .error
    }
}

extension XPCConnectionManager: XPCConnectionLifecycle {
    @MainActor
    func beginShutdown() {
        guard lifecycleState == .running else { return }
        lifecycleState = .shuttingDown
        loginAgentRetryTask?.cancel()
        serverRetryTask?.cancel()
    }

    @MainActor
    func finishShutdown() {
        beginShutdown()
        guard lifecycleState != .stopped else { return }
        lifecycleState = .stopped
        for connection in [appConnection, loginItemAgentConnection].compactMap({ $0 }) {
            connection.interruptionHandler = nil
            connection.invalidationHandler = nil
            connection.invalidate()
        }
        appConnection = nil
        loginItemAgentConnection = nil
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
        Task { @MainActor [weak self] in
            try? self?.connectToServer(endpoint: endPoint)
        }
    }
}

extension XPCConnectionManager: XPCGuiRemoteProtocol {
    func processSignal(_ msg: Data) {
        signalProcessor.enqueue(msg)
    }
}
