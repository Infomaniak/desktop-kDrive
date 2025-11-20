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

public protocol XPCConnectionProvider: Sendable {
    var guiConnection: XPCGuiProtocol { get async throws }
}

extension XPCConnectionManager: XPCConnectionProvider {
    public var connection: NSXPCConnection {
        get throws {
            guard let appConnection else {
                throw XPCError.noAppConnectionAvailable
            }

            return appConnection
        }
    }

    public var guiConnection: XPCGuiProtocol {
        get async throws {
            self.appConnection?.invalidate()
            self.appConnection = nil
            
            try await self.fetchServerEndpointFromLoginItemAgentAndConnect()

            let connection = try self.connection
            return try connection.proxy(from: connection, type: XPCGuiProtocol.self)
        }
    }
}
