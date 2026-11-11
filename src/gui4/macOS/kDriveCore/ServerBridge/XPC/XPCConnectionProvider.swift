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
    /// Sends UTILITY_QUIT using a bounded transport lookup, without restarting normal connection work.
    func sendQuitQuery(_ requestData: Data) async throws -> Data

    @MainActor var guiConnectionState: XPCConnectionState { get }
    @MainActor var guiConnectionStatePublisher: AnyPublisher<XPCConnectionState, Never> { get }

    @MainActor var loginItemAgentConnectionState: XPCLoginItemAgentConnectionState { get }
    @MainActor var loginItemAgentConnectionStatePublisher: AnyPublisher<XPCLoginItemAgentConnectionState, Never> { get }

    func reconnectToLoginAgent() async
}

public extension XPCConnectionProvider {
    func sendQuitQuery(_ requestData: Data) async throws -> Data {
        try await sendQuery(requestData)
    }
}

public protocol XPCConnectionLifecycle: Sendable {
    /// Stops ordinary connection attempts while allowing the dedicated quit request to obtain a transport.
    @MainActor func beginShutdown()
    @MainActor func finishShutdown()
}

extension XPCConnectionManager: XPCConnectionProvider {
    @MainActor
    public var connection: NSXPCConnection {
        get throws {
            guard lifecycleState != .stopped else { throw XPCError.shuttingDown }
            guard let appConnection else {
                throw XPCError.noAppConnectionAvailable
            }

            return appConnection
        }
    }

    @MainActor
    public func sendQuery(_ requestData: Data) async throws -> Data {
        guard lifecycleState == .running else { throw XPCError.shuttingDown }
        try await fetchServerEndpointFromLoginItemAgentAndConnectIfNeeded()
        guard lifecycleState == .running else { throw XPCError.shuttingDown }

        return try await sendQuery(requestData, over: connection)
    }

    @MainActor
    public func sendQuitQuery(_ requestData: Data) async throws -> Data {
        guard lifecycleState != .stopped else { throw XPCError.shuttingDown }
        let deadline = DispatchTime.now() + 5
        if appConnection == nil {
            if loginItemAgentConnection == nil {
                initializeLoginItemAgentConnection()
            }
            let endpoint = try await getServerEndpoint(shutdownDeadline: deadline)
            try connectToServer(endpoint: endpoint, forShutdown: true)
        }
        return try await sendQuery(requestData, over: connection, deadline: deadline)
    }

    @MainActor
    private func sendQuery(_ requestData: Data, over connection: NSXPCConnection,
                           deadline: DispatchTime? = nil) async throws -> Data {
        return try await XPCContinuation<Data>.perform(deadline: deadline) { continuation in
            do {
                let proxy = try connection.proxy(errorHandler: { error in
                    IKLogger.xpc.error("[KD] Failed to send query to server: \(error)")
                    continuation.resume(throwing: error)
                }, type: XPCGuiProtocol.self)
                proxy.processQuery(requestData) { data in
                    IKLogger.xpc.log("[KD] recv raw callback len: \(data.count)")
                    continuation.resume(returning: data)
                }
            } catch {
                continuation.resume(throwing: error)
            }
        }
    }

    @MainActor
    public var guiConnectionStatePublisher: AnyPublisher<XPCConnectionState, Never> {
        return $guiConnectionState.eraseToAnyPublisher()
    }

    @MainActor
    public var loginItemAgentConnectionStatePublisher: AnyPublisher<XPCLoginItemAgentConnectionState, Never> {
        return $loginItemAgentConnectionState.eraseToAnyPublisher()
    }
}
