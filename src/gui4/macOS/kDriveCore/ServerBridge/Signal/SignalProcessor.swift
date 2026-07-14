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

public protocol SignalProcessing: Sendable {
    /// Enqueues a raw server signal for serialized, in-order processing.
    func enqueue(_ signal: Data)
}

/// Serializes the processing of server-push signals.
///
/// This actor funnels every signal through a single unbounded FIFO `AsyncStream` drained by one
/// consumer task. Because that task awaits each signal to completion before pulling the next, signals
/// are applied one at a time and in the exact order the server emitted them.
public actor SignalProcessor: SignalProcessing {
    private let handler: XPCSignalHandlerProtocol
    private let stream: AsyncStream<Data>
    private let continuation: AsyncStream<Data>.Continuation

    public init(handler: sending XPCSignalHandlerProtocol) {
        self.handler = handler
        (stream, continuation) = AsyncStream.makeStream(of: Data.self, bufferingPolicy: .unbounded)
        Task { await consume() }
    }

    public nonisolated func enqueue(_ signal: Data) {
        continuation.yield(signal)
    }

    private func consume() async {
        for await signal in stream {
            await handler.handleServerSignal(signal)
        }
    }
}
