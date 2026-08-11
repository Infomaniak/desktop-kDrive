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

enum NSXPCConnectionError: Error {
    case failedToCastProxy(_: String)
}

extension NSXPCConnection {
    func proxy<Interface: AnyObject>(
        errorHandler: @escaping (Error) -> Void,
        type: Interface.Type
    ) throws -> Interface {
        let proxy = remoteObjectProxyWithErrorHandler(errorHandler)
        guard let typedProxy = proxy as? Interface else {
            throw NSXPCConnectionError.failedToCastProxy(String(describing: Interface.self))
        }

        return typedProxy
    }
}

actor XPCContinuation<Value: Sendable> {
    private var continuation: CheckedContinuation<Value, Error>?

    init(_ continuation: CheckedContinuation<Value, Error>) {
        self.continuation = continuation
    }

    func resume(returning value: Value) {
        continuation?.resume(returning: value)
        continuation = nil
    }

    func resume(throwing error: Error) {
        continuation?.resume(throwing: error)
        continuation = nil
    }
}
