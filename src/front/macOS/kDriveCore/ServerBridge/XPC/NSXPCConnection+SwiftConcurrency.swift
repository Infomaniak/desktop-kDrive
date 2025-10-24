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

public extension NSXPCConnection {
    /// Async/Await wrapping of remoteObjectProxyWithErrorHandler
    func asyncProxy<Interface>(
        from connection: NSXPCConnection,
        type: Interface.Type
    ) async throws -> Interface where Interface: AnyObject {
        try await withCheckedThrowingContinuation { continuation in
            let proxy = connection.remoteObjectProxyWithErrorHandler { error in
                continuation.resume(throwing: error)
            }

            guard let typedProxy = proxy as? Interface else {
                continuation.resume(
                    throwing: NSError(domain: "XPCError", code: -1, userInfo: [
                        NSLocalizedDescriptionKey: "Failed to cast proxy to \(Interface.self)"
                    ])
                )
                return
            }

            continuation.resume(returning: typedProxy)
        }
    }
}
