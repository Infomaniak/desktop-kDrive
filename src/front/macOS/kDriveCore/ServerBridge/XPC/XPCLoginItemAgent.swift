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

@preconcurrency @objc open class XPCLoginItemAgent: NSObject, @unchecked Sendable {
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
            IKLogger.xpc.log("Set timer to retry to connect to login agent")
            Timer.scheduledTimer(withTimeInterval: 10, repeats: false) { [weak self] _ in
                self?.connectToLoginAgent()
            }
        }
    }

    func connectToLoginAgent() {
        if loginItemAgentConnection != nil {
            IKLogger.xpc.log("Already connected to item agent")
            return
        }

        // Initialize connection with login item agent
        IKLogger.xpc.log("Initialize connection with login item agent")
        let connection = NSXPCConnection(machServiceName: machServiceName, options: [])

        guard connection != nil else {
            IKLogger.xpc.error("Failed to connect to login item agent")
            scheduleRetryToConnectToLoginAgent()
            return
        }

        loginItemAgentConnection = connection

        // Set exported interface
        IKLogger.xpc.log("Set exported interface for connection with login agent")
        connection.exportedInterface = NSXPCInterface(with: XPCLoginItemRemoteProtocol.self)
        connection.exportedObject = self

        // Set remote object interface
        IKLogger.xpc.log("Set remote object interface for connection with login agent")
        connection.remoteObjectInterface = NSXPCInterface(with: XPCLoginItemProtocol.self)

        // Set connection handlers
        IKLogger.xpc.log("Set connection handlers for connection with login item agent")
        connection.interruptionHandler = { [weak self] in
            IKLogger.xpc.error("Connection with login item agent interrupted (server crash)")
            guard let self = self else { return }
            self.loginItemAgentConnection = nil
            self.scheduleRetryToConnectToLoginAgent()
        }

        connection.invalidationHandler = { [weak self] in
            IKLogger.xpc.error("Connection with login item agent invalidated (no server running)")
            guard let self = self else { return }
            self.loginItemAgentConnection = nil
            self.scheduleRetryToConnectToLoginAgent()
        }

        // Resume connection
        IKLogger.xpc.log("Resume connection with login item agent")
        connection.resume()

        // Get server endpoint from login item agent
        IKLogger.xpc.log("Get server ext endpoint from login item agent")
        (connection.remoteObjectProxy as? XPCLoginItemProtocol)?.serverExtEndpoint { [weak self] endpoint in
            IKLogger.xpc.log("Server ext endpoint received \(String(describing: endpoint))")
            if let endpoint = endpoint {
                self?.connectToServer(endpoint: endpoint)
            }
        }
    }

    func connectToServer(endpoint: NSXPCListenerEndpoint?) {
        guard let endpoint = endpoint else {
            IKLogger.xpc.log("Endpoint is nil, unable to connect to it")
            return
        }

        if appConnection != nil {
            IKLogger.xpc.log("Already connected to app")
            return
        }

        // Setup connection with app
        IKLogger.xpc.log("Setup connection with app")
        appConnection = NSXPCConnection(listenerEndpoint: endpoint)

        // Set exported interface
        IKLogger.xpc.log("Set server -> gui interface")
        appConnection?.exportedInterface = NSXPCInterface(with: XPCGuiRemoteProtocol.self)
        appConnection?.exportedObject = self

        // Set remote object interface
        IKLogger.xpc.log("Set gui -> server interface")
        appConnection?.remoteObjectInterface = NSXPCInterface(with: XPCGuiProtocol.self)

        // Set connection handlers
        IKLogger.xpc.log("[KD] Setup connection handlers for connection with app")
        appConnection?.interruptionHandler = { [weak self] in
            IKLogger.xpc.error("[KD] Connection with app interrupted (server crash)")
            guard let self = self else { return }
            self.appConnection = nil
            // TODO: Cleanup
        }

        appConnection?.invalidationHandler = { [weak self] in
            IKLogger.xpc.error("[KD] Connection with app invalidated (no server running)")
            guard let self = self else { return }
            self.appConnection = nil
            // TODO: Cleanup
        }

        // Resume connection
        appConnection?.resume()

        dummyServerQuery(appConnection)
    }

    func dummyServerQuery(_ appConnection: NSXPCConnection?) {
        IKLogger.xpc.log("[KD] Start communication with app server")

        let remoteObject = appConnection?.remoteObjectProxyWithErrorHandler { error in
            IKLogger.xpc.error("[KD] Error during remote object proxy call: \(error)")
        } as? XPCGuiProtocol

        let json = """
        { "num": 1,
            "params": {
            "code": "YWJhYQ==",
            "codeVerifier": "abFmJiYg==" } }
        """

        let dummyData = json.data(using: .utf8)!
        IKLogger.xpc.error("[KD] sending bytes: \(dummyData.count)")

        let queryCallback: @Sendable (Data) -> Void = { answer in
            // Your closure logic here
            IKLogger.xpc.log("[KD] recv answer of length: \(answer.count)")
        }

        remoteObject?.sendQuery(dummyData, callback: queryCallback)
    }
}

// TODO: Move to dedicated types

extension XPCLoginItemAgent: XPCLoginItemRemoteProtocol {
    func processType(_ callback: @escaping (ProcessType) -> Void) {
        IKLogger.xpc.log("query processType")
        callback(ProcessType.client)
    }

    func serverIsRunning(_ endPoint: NSXPCListenerEndpoint?) {
        IKLogger.xpc.log("serverIsRunning :\(String(describing: endPoint))")
        connectToServer(endpoint: endPoint)
    }
}

extension XPCLoginItemAgent: XPCGuiRemoteProtocol {
    func sendSignal(_ msg: Data?) {
        IKLogger.xpc.log("recv signal:\(msg?.count ?? 0)")
    }
}
