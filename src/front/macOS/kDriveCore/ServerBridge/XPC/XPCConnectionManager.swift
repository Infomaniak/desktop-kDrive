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

import Combine
import Foundation
import InfomaniakDI

@objc final class XPCConnectionManager: NSObject, @unchecked Sendable {
    @InjectService var signalHandler: XPCSignalHandlerProtocol
    @LazyInjectService var coherentCache: CoherentCache

    @MainActor
    @Published private(set) var guiConnectionState: XPCConnectionState = .notConnected

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
        Task {
            IKLogger.xpc.log("[KD] Set timer to retry to connect to login agent")
            try? await Task.sleep(nanoseconds: 10_000_000_000)
            try? await connectToLoginAgent()
        }
    }

    func scheduleRetryToConnectToServer() {
        Task {
            IKLogger.xpc.log("[KD] Set timer to retry to connect to server")
            try? await Task.sleep(nanoseconds: 10_000_000_000)
            try? await fetchServerEndpointFromLoginItemAgentAndConnect()
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
            scheduleRetryToConnectToLoginAgent()
        }

        connection.invalidationHandler = { [weak self] in
            IKLogger.xpc.error("[KD] Connection with login item agent invalidated (no server running)")
            guard let self else { return }
            loginItemAgentConnection = nil
            scheduleRetryToConnectToLoginAgent()
        }

        IKLogger.xpc.log("[KD] Resume connection with login item agent")
        connection.resume()

        try await fetchServerEndpointFromLoginItemAgentAndConnect()
    }

    func fetchServerEndpointFromLoginItemAgentAndConnect() async throws {
        let endpoint = try await getServerEndpoint()
        try connectToServer(endpoint: endpoint)
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
                IKLogger.xpc.log("[KD] Server gui endpoint received \(String(describing: endpoint))")
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
        appConnection = NSXPCConnection(listenerEndpoint: endpoint)

        IKLogger.xpc.log("[KD] Set server -> gui interface")
        appConnection?.exportedInterface = NSXPCInterface(with: XPCGuiRemoteProtocol.self)
        appConnection?.exportedObject = self

        IKLogger.xpc.log("[KD] Set gui -> server interface")
        appConnection?.remoteObjectInterface = NSXPCInterface(with: XPCGuiProtocol.self)

        IKLogger.xpc.log("[KD] Setup connection handlers for connection with app")
        appConnection?.interruptionHandler = { [weak self] in
            IKLogger.xpc.error("[KD] Connection with app interrupted (server crash)")
            guard let self else { return }
            appConnection?.invalidate()
            appConnection = nil
            scheduleRetryToConnectToServer()
            Task { @MainActor [weak self] in
                self?.guiConnectionState = .notConnected
            }
        }

        appConnection?.invalidationHandler = { [weak self] in
            IKLogger.xpc.error("[KD] Connection with app invalidated (no server running)")
            guard let self else { return }
            appConnection?.invalidate()
            appConnection = nil
            scheduleRetryToConnectToServer()
            Task { @MainActor [weak self] in
                self?.guiConnectionState = .error
            }
        }

        appConnection?.resume()

        Task {
            IKLogger.xpc.log("[KD] coherentCache.clearAndRefresh")
            try await coherentCache.clearAndRefresh()
            await MainActor.run {
                guiConnectionState = .connected
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
        Task {
            await signalHandler.handleServerSignal(msg)
        }
    }
}
