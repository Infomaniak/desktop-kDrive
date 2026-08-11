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

public enum XPCConnectionState {
    case connected
    case notConnected
    case error
    case serverCrashed
}

public enum XPCLoginItemAgentConnectionState: Sendable, Equatable {
    /// A connection attempt is in progress and has not yet succeeded or failed.
    case connecting
    /// The login item agent is reachable.
    case connected
    /// The login item agent could not be reached (e.g. it was invalidated or is not enabled).
    case disconnected
}

public protocol XPCConnectionProvider: Sendable {
    func sendQuery(_ requestData: Data) async throws -> Data

    var guiConnectionState: XPCConnectionState { get }
    var guiConnectionStatePublisher: AnyPublisher<XPCConnectionState, Never> { get }

    var loginItemAgentConnectionState: XPCLoginItemAgentConnectionState { get }
    var loginItemAgentConnectionStatePublisher: AnyPublisher<XPCLoginItemAgentConnectionState, Never> { get }

    func reconnectToLoginAgent() async
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

    public func sendQuery(_ requestData: Data) async throws -> Data {
        try await fetchServerEndpointFromLoginItemAgentAndConnectIfNeeded()

        let connection = try connection
        return try await withCheckedThrowingContinuation { continuation in
            let continuation = XPCContinuation(continuation)
            do {
                let proxy = try connection.proxy(errorHandler: { error in
                    IKLogger.xpc.error("[KD] Failed to send query to server: \(error)")
                    Task { await continuation.resume(throwing: error) }
                }, type: XPCGuiProtocol.self)
                proxy.processQuery(requestData) { data in
                    IKLogger.xpc.log("[KD] recv raw callback len: \(data.count)")
                    Task { await continuation.resume(returning: data) }
                }
            } catch {
                Task { await continuation.resume(throwing: error) }
            }
        }
    }

    public var guiConnectionStatePublisher: AnyPublisher<XPCConnectionState, Never> {
        return $guiConnectionState.eraseToAnyPublisher()
    }

    public var loginItemAgentConnectionStatePublisher: AnyPublisher<XPCLoginItemAgentConnectionState, Never> {
        return $loginItemAgentConnectionState.eraseToAnyPublisher()
    }
}
