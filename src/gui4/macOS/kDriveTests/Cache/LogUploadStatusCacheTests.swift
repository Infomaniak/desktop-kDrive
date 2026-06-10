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
@testable import kDriveCore
import Testing

@Suite("LogUploadStatusCache Tests")
struct LogUploadStatusCacheTests {
    @Test(.timeLimit(.minutes(1)))
    func publishesLogUploadStatusChanges() async {
        // GIVEN
        let cache = LogUploadStatusCache()
        let expectedStatus = makeStatus(state: KDC.LogUploadState.Uploading, percentage: 42)
        let receivedValues = values(from: cache.logUploadStatusPublisher)

        // WHEN
        await cache.setLogUploadStatus(expectedStatus)

        // THEN
        let receivedStatus = await receivedValues.first(where: { _ in true })
        let cachedStatus = await cache.getLogUploadStatus()
        #expect(receivedStatus == expectedStatus)
        #expect(cachedStatus == expectedStatus)
    }

    @Test(.timeLimit(.minutes(1)))
    func publishesCompletionStatus() async {
        // GIVEN
        let cache = LogUploadStatusCache()
        let expectedStatus = makeStatus(state: KDC.LogUploadState.Success, percentage: 100)
        let receivedValues = values(from: cache.logUploadStatusPublisher)

        // WHEN
        await cache.setLogUploadStatus(expectedStatus)

        // THEN
        let receivedStatus = await receivedValues.first(where: { $0.isCompleted })
        #expect(receivedStatus == expectedStatus)
        #expect(receivedStatus?.isCompleted == true)
    }

    @Test(.timeLimit(.minutes(1)))
    func replaysLatestLogUploadStatusToNewSubscribers() async {
        // GIVEN
        let cache = LogUploadStatusCache()
        let expectedStatus = makeStatus(state: KDC.LogUploadState.Success, percentage: 100)
        await cache.setLogUploadStatus(expectedStatus)
        let receivedValues = values(from: cache.logUploadStatusPublisher)

        // WHEN
        let receivedStatus = await receivedValues.first(where: { _ in true })

        // THEN
        #expect(receivedStatus == expectedStatus)
    }

    private func makeStatus(state: KDC.LogUploadState, percentage: Int32) -> LogUploadStatus {
        let signal = LogUploadStatusSignal(state: state, percentage: percentage)
        return LogUploadStatus(signal: signal)
    }

    private func values(from publisher: LogUploadStatusPublisher) -> AsyncStream<LogUploadStatus> {
        AsyncStream { continuation in
            let cancellable = publisher.sink { status in
                continuation.yield(status)
            }

            continuation.onTermination = { _ in cancellable.cancel() }
        }
    }
}
