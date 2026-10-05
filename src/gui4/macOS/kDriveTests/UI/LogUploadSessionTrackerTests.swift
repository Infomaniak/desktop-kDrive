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

@testable import kDrive
import kDriveCore
import Testing

struct LogUploadSessionTrackerTests {
    @Test("A terminal status replayed from a previous upload is ignored", arguments: [
        KDC.LogUploadState.Success,
        KDC.LogUploadState.Failed,
        KDC.LogUploadState.Canceled
    ])
    func staleTerminalStatusIsIgnored(_ state: KDC.LogUploadState) {
        var tracker = LogUploadSessionTracker()

        let effect = tracker.handle(LogUploadStatus(state: state, percentage: 100))

        #expect(effect == .ignored)
    }

    @Test("A terminal status is handled after observing upload progress")
    func currentTerminalStatusIsHandled() {
        var tracker = LogUploadSessionTracker()

        let progressEffect = tracker.handle(LogUploadStatus(state: .Archiving, percentage: 0))
        let completionEffect = tracker.handle(LogUploadStatus(state: .Success, percentage: 100))

        #expect(progressEffect == .inProgress)
        #expect(completionEffect == .succeeded)
    }

    @Test("Starting a new upload ignores a delayed completion from the previous upload")
    func delayedTerminalStatusIsIgnored() {
        var tracker = LogUploadSessionTracker()
        _ = tracker.handle(LogUploadStatus(state: .Uploading, percentage: 50))

        tracker.prepareForUpload()
        let effect = tracker.handle(LogUploadStatus(state: .Success, percentage: 100))

        #expect(effect == .ignored)
    }
}
