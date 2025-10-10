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

class XPCConnection: NSObject, ObservableObject, ClientProtocol {
    private var connection: NSXPCConnection!

    private func establishConnection() {
        connection = NSXPCConnection(serviceName: "dummyXPC ServiceLabel")
        
        connection.remoteObjectInterface = NSXPCInterface(with: XPCServiceProtocol.self)
        connection.exportedObject = self
        connection.exportedInterface = NSXPCInterface(with: ClientProtocol.self)

        connection.interruptionHandler = {
            IKLogger.xpc.log("connection to XPC service has been interrupted")
        }

        connection.invalidationHandler = {
            IKLogger.xpc.log("connection to XPC service has been invalidated")
            self.connection = nil
        }

        connection.resume()

        IKLogger.xpc.log("successfully connected to XPC service")
    }

    public func xpcService() -> XPCServiceProtocol {
        if connection == nil {
            IKLogger.xpc.log("No connection to XPC service, starting …")
            establishConnection()
        }

        return connection.remoteObjectProxyWithErrorHandler { err in
            IKLogger.xpc.error("error getting remote object proxy: \(err)")
        } as! XPCServiceProtocol
    }

    // TODO: For testing only
    func invalidateConnection() {
        guard connection != nil else { IKLogger.xpc.error("no connection to invalidate"); return }

        connection.invalidate()
    }

    // MARK: ClientProtocol

    func dummyClientMethod() {
        DispatchQueue.main.async { IKLogger.xpc.log("dummyClientMethod called") }
    }
}
