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

@preconcurrency @objc open class XPCConnectionManager: NSObject, @unchecked Sendable {
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
        DispatchQueue.main.async {
            IKLogger.xpc.log("[KD] Set timer to retry to connect to login agent")
            Timer.scheduledTimer(withTimeInterval: 10, repeats: false) { [weak self] _ in
                self?.connectToLoginAgent()
            }
        }
    }

    func scheduleRetryToConnectToServer() {
        DispatchQueue.main.async {
            IKLogger.xpc.log("[KD] Set timer to retry to connect to server")
            Timer.scheduledTimer(withTimeInterval: 10, repeats: false) { [weak self] _ in
                self?.fetchServerEndpointFromLoginItemAgentAndConnect()
            }
        }
    }

    func connectToLoginAgent() {
        guard loginItemAgentConnection == nil else {
            IKLogger.xpc.log("[KD] Already connected to item agent")
            return
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

        fetchServerEndpointFromLoginItemAgentAndConnect()
    }

    func fetchServerEndpointFromLoginItemAgentAndConnect() {
        guard let loginItemAgentConnection,
              let loginItemProxy = loginItemAgentConnection.remoteObjectProxy as? XPCLoginItemProtocol else {
            return
        }

        // Get server endpoint from login item agent
        IKLogger.xpc.log("[KD] Get server gui endpoint from login item agent")
        loginItemProxy.serverGuiEndpoint { [weak self] endpoint in
            IKLogger.xpc.log("[KD] Server gui endpoint received \(String(describing: endpoint))")
            guard let endpoint else {
                IKLogger.xpc.error("[KD] endpoint nil")
                return
            }
            self?.connectToServer(endpoint: endpoint)
        }
    }

    func connectToServer(endpoint: NSXPCListenerEndpoint) {
        guard appConnection == nil else {
            IKLogger.xpc.log("[KD] Already connected to app")
            return
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
    func dummyServerQuery() {
        guard let appConnection else {
            IKLogger.xpc.error("[KD] no connection")
            return
        }

        IKLogger.xpc.log("[KD] Start communication with app server")

        let remoteObject = appConnection.remoteObjectProxyWithErrorHandler { error in
            IKLogger.xpc.error("[KD] Error during remote object proxy call: \(error)")
        } as? XPCGuiProtocol

        guard let remoteObject else {
            IKLogger.xpc.log("[KD] remote object nil")
            return
        }

        let json = """
        { "num": 1,
            "params": {
            "code": "YWJhYQ==",
            "codeVerifier": "abFmJiYg==" } }
        """
        let dummyData = json.data(using: .utf8)!
        IKLogger.xpc.error("[KD] sending bytes: \(dummyData.count)")

        remoteObject.sendQuery(dummyData) { data in
            guard let data else {
                IKLogger.xpc.log("[KD] recv answer NIL")
                return
            }

            IKLogger.xpc.log("[KD] recv answer of length: \(data.count)")
            let recv = String(data: data, encoding: .utf8)!
            IKLogger.xpc.log("[KD] recv: \(recv)")
        }
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
        connectToServer(endpoint: endPoint)
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
