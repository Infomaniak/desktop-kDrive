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

import AppKit
@testable import kDrive
import kDriveCore
import Testing

@MainActor
@Suite(.timeLimit(.minutes(1)))
struct ApplicationShutdownTests {
    @MainActor
    private final class ConnectionLifecycle: XPCConnectionLifecycle {
        var steps: [String] = []

        func beginShutdown() { steps.append("begin") }
        func finishShutdown() { steps.append("finish") }
    }

    @MainActor
    private final class ShutdownReference {
        var value: ApplicationShutdown?
    }

    @Test func recursiveTerminationSendsOneQuitAndRepliesOnlyAfterTeardown() async {
        let lifecycle = ConnectionLifecycle()
        let started = AsyncStream<Void>.makeStream()
        let release = AsyncStream<Void>.makeStream()
        let completed = AsyncStream<Void>.makeStream()
        var quitCount = 0
        var replyCount = 0
        let reference = ShutdownReference()
        defer {
            started.continuation.finish()
            release.continuation.finish()
            completed.continuation.finish()
            reference.value = nil
        }
        let shutdown = ApplicationShutdown(connectionLifecycle: lifecycle, quitServer: {
            quitCount += 1
            #expect(lifecycle.steps == ["begin"])
            #expect(reference.value?.requestTermination(notifyServer: true) == .terminateLater)
            started.continuation.yield(())
            var iterator = release.stream.makeAsyncIterator()
            await iterator.next()
        }, reply: {
            replyCount += 1
            #expect(lifecycle.steps == ["begin", "finish"])
            #expect(reference.value?.requestTermination(notifyServer: true) == .terminateNow)
            completed.continuation.yield(())
        })
        reference.value = shutdown

        #expect(shutdown.requestTermination(notifyServer: true) == .terminateLater)
        #expect(shutdown.requestTermination(notifyServer: true) == .terminateLater)
        var startedIterator = started.stream.makeAsyncIterator()
        await startedIterator.next()
        #expect(quitCount == 1)
        #expect(replyCount == 0)
        #expect(shutdown.requestTermination(notifyServer: true) == .terminateLater)

        release.continuation.yield(())
        var completedIterator = completed.stream.makeAsyncIterator()
        await completedIterator.next()
        #expect(quitCount == 1)
        #expect(replyCount == 1)
        #expect(shutdown.requestTermination(notifyServer: true) == .terminateNow)
    }

    @Test func serverHandoffTearsDownWithoutSendingQuitOrReplyingLater() {
        let lifecycle = ConnectionLifecycle()
        let shutdown = ApplicationShutdown(connectionLifecycle: lifecycle, quitServer: {
            Issue.record("Server handoff must not quit the server")
        }, reply: {
            Issue.record("Immediate termination must not reply asynchronously")
        })

        #expect(shutdown.requestTermination(notifyServer: false) == .terminateNow)
        #expect(shutdown.requestTermination(notifyServer: true) == .terminateNow)
        #expect(lifecycle.steps == ["begin", "finish"])
    }
}
