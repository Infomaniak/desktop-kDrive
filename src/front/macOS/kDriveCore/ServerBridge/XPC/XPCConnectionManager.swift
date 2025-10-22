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

import Foundation

@objc final class XPCConnectionManager: NSObject {
    let machServiceName: String

    var loginItemAgentConnection: NSXPCConnection?
    var appConnection: NSXPCConnection?

    override init() {
        guard let loginItemAgentMachName = Bundle.main.object(forInfoDictionaryKey: "LoginItemAgentMachName") as? String else {
            fatalError("Malformed info.plist, missing LoginItemAgentMachName")
        }

        IKLogger.xpc.log("[KD] mach name: \(loginItemAgentMachName)")
        machServiceName = loginItemAgentMachName
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
            throw NSError(domain: "XPC",
                          code: -1,
                          userInfo: [NSLocalizedDescriptionKey: "missing login item agent connection"])
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
            self.loginItemAgentConnection = nil
            self.scheduleRetryToConnectToLoginAgent()
        }

        connection.invalidationHandler = { [weak self] in
            IKLogger.xpc.error("[KD] Connection with login item agent invalidated (no server running)")
            guard let self else { return }
            self.loginItemAgentConnection = nil
            self.scheduleRetryToConnectToLoginAgent()
        }

        IKLogger.xpc.log("[KD] Resume connection with login item agent")
        connection.resume()

        try await fetchServerEndpointFromLoginItemAgentAndConnect()
    }

    func fetchServerEndpointFromLoginItemAgentAndConnect() async throws {
        let endpoint = try await getServerEndpoint()
        try connectToServer(endpoint: endpoint)
    }

    func getServerEndpoint() async throws -> NSXPCListenerEndpoint {
        guard let loginItemAgentConnection,
              let loginItemProxy = loginItemAgentConnection.remoteObjectProxy as? XPCLoginItemProtocol else {
            throw NSError(domain: "XPC",
                          code: -1,
                          userInfo: [NSLocalizedDescriptionKey: "missing login item agent connection"])
        }

        IKLogger.xpc.log("[KD] Get server gui endpoint from login item agent")

        return try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<NSXPCListenerEndpoint, Error>) in
            loginItemProxy.serverGuiEndpoint { endpoint in
                IKLogger.xpc.log("[KD] Server gui endpoint received \(String(describing: endpoint))")
                if let endpoint = endpoint {
                    continuation.resume(returning: endpoint)
                } else {
                    IKLogger.xpc.error("[KD] endpoint nil")
                    continuation.resume(throwing: NSError(
                        domain: "XPC",
                        code: -1,
                        userInfo: [NSLocalizedDescriptionKey: "Endpoint was nil"]
                    ))
                }
            }
        }
    }

    func connectToServer(endpoint: NSXPCListenerEndpoint) throws {
        guard appConnection == nil else {
            IKLogger.xpc.log("[KD] Already connected to app")
            throw NSError(domain: "XPC",
                          code: -1,
                          userInfo: [NSLocalizedDescriptionKey: "missing login item agent connection"])
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
            self.appConnection = nil
            self.scheduleRetryToConnectToServer()
        }

        appConnection?.invalidationHandler = { [weak self] in
            IKLogger.xpc.error("[KD] Connection with app invalidated (no server running)")
            guard let self else { return }
            self.appConnection = nil
            self.scheduleRetryToConnectToServer()
        }

        appConnection?.resume()
    }

    // TODO: Remove
    func dummyServerQuery() async throws {
        guard let appConnection else {
            IKLogger.xpc.error("[KD] no connection")
            return
        }

        IKLogger.xpc.log("[KD] Start communication with app server")
        let remoteObject = try await appConnection.asyncProxy(from: appConnection, type: XPCGuiProtocol.self)

        let userQuery = LoginQuery(code: "123", codeVerifier: "456")
        let request = await RequestMessage<LoginQuery>(num: RequestNum.LOGIN_REQUESTTOKEN, body: userQuery)
        let requestData = try JSONEncoder().encode(request)

        guard let replyData = await remoteObject.sendQueryAsync(requestData) else {
            IKLogger.xpc.log("[KD] recv answer NIL")
            return
        }

        IKLogger.xpc.log("[KD] recv answer of length: \(replyData.count)")
        let recv = String(data: replyData, encoding: .utf8)!
        IKLogger.xpc.log("[KD] recv RAW json: \(recv)")

        let decodedMessage = try? JSONDecoder().decode(CallbackMessage<LoginResponse>.self, from: replyData)
        IKLogger.xpc.log("[KD] recv decodedMessage: \(String(describing: decodedMessage))")
    }
}

extension XPCConnectionManager: XPCLoginItemRemoteProtocol {
    public func processType(_ callback: @escaping (ProcessType) -> Void) {
        IKLogger.xpc.log("[KD] query processType")
        callback(ProcessType.client)
    }

    public func serverIsRunning(_ endPoint: NSXPCListenerEndpoint?) {
        IKLogger.xpc.log("[KD] serverIsRunning")
        guard let endPoint else {
            IKLogger.xpc.error("[KD] server sent a nil endpoint")
            return
        }
        try? connectToServer(endpoint: endPoint)
    }
}

extension XPCConnectionManager: XPCGuiRemoteProtocol {
    public func sendSignal(_ msg: Data?) {
        guard let msg else {
            IKLogger.xpc.error("[KD] recv sendSignal with nil data")
            return
        }
        IKLogger.xpc.log("[KD] recv signal \(String(data: msg, encoding: .utf8))")
    }
}
