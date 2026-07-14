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
@testable import kDriveCore
import Testing

/// Collects the values processed by the fake signal handler and lets the test await a target count.
private actor RecordingStore {
    private(set) var values: [Int] = []
    private var awaited: (target: Int, continuation: CheckedContinuation<Void, Never>)?

    func snapshot() -> [Int] {
        values
    }

    /// Non-atomic replace: overwrites the whole array from a snapshot the caller took *before* a
    /// suspension point. If two handlers ran concurrently, the second replace would clobber the
    /// first (lost update) and/or reorder values. Serial processing keeps every value, in order.
    func replace(with newValues: [Int]) {
        values = newValues
        if let awaited, values.count >= awaited.target {
            awaited.continuation.resume()
            self.awaited = nil
        }
    }

    func waitFor(count: Int) async {
        if values.count >= count { return }
        await withCheckedContinuation { continuation in
            awaited = (count, continuation)
        }
    }
}

/// Fake handler that mimics the real signal handlers' read-modify-write over the (actor) cache,
/// including a suspension point between the read and the write.
private struct OrderRecordingSignalHandler: XPCSignalHandlerProtocol {
    let store: RecordingStore

    func handleServerSignal(_ signal: Data?) async {
        guard let signal, let value = try? JSONDecoder().decode(Int.self, from: signal) else { return }

        var current = await store.snapshot() // read
        await Task.yield() // suspension window where interleaving would occur if not serialized
        current.append(value) // modify
        await store.replace(with: current) // write
    }
}

@Suite("SignalProcessor serialization")
struct SignalProcessorTests {
    @Test("Processes every enqueued signal exactly once, in FIFO order")
    func serializesSignalsInOrder() async throws {
        let encoder = JSONEncoder()
        let store = RecordingStore()
        let processor = SignalProcessor(handler: OrderRecordingSignalHandler(store: store))

        let count = 200
        for index in 0 ..< count {
            try processor.enqueue(encoder.encode(index))
        }

        await store.waitFor(count: count)

        // If signals were handled concurrently, the racy read-modify-write above would drop or reorder
        // values. Serial, in-order processing yields exactly 0..<count.
        let values = await store.snapshot()
        #expect(values == Array(0 ..< count))
    }
}
