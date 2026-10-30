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

import Cocoa
import Combine
import InfomaniakDI
import kDriveCore
import kDriveResources
import Sentry
import SwiftUI

/// Presents the pop-up window informing the user of the progress of a file opening requested through a
/// "kdrive://open/..." deeplink, and forwards the cancellation requests to the server.
@MainActor
final class FileOpeningModalPresenter {
    @LazyInjectService private var fileOpeningStatusCacheObservable: FileOpeningStatusCacheObservable

    private struct FileOpeningWindowEntry {
        let window: NSWindow
        let hostingController: NSHostingController<FileOpeningStatusView>
    }

    private var bindStore = Set<AnyCancellable>()
    private var windowEntriesByJobId = [Int64: FileOpeningWindowEntry]()

    init() {
        fileOpeningStatusCacheObservable.fileOpeningStatusPublisher
            .receive(on: DispatchQueue.main)
            .sink { [weak self] status in
                self?.handleStatus(status)
            }
            .store(in: &bindStore)
    }

    // MARK: - Status handling

    private func handleStatus(_ status: FileOpeningStatus) {
        if status.phase == .finished {
            // The file opening request has been processed: close the pop-up window, if one is open.
            closeWindow(for: status.jobId)
            return
        }

        let entry = windowEntry(for: status.jobId)
        entry.hostingController.rootView = FileOpeningStatusView(itemName: status.itemName, phase: status.phase,
                                                                 percent: status.percent) { [weak self] in
            self?.cancel(jobId: status.jobId)
        }
        entry.window.makeKeyAndOrderFront(nil)
    }

    private func windowEntry(for jobId: Int64) -> FileOpeningWindowEntry {
        if let entry = windowEntriesByJobId[jobId] {
            return entry
        }

        IKLogger.general.info("[KD] Presenting the file opening progress window jobId=\(jobId)")
        let hostingController = NSHostingController(
            rootView: FileOpeningStatusView(itemName: "", phase: .waitingSync, percent: -1) {}
        )
        let window = NSWindow(contentViewController: hostingController)
        window.styleMask = [.titled, .closable, .miniaturizable]
        window.title = KDriveLocalizable.fileOpeningWindowTitle
        window.isReleasedWhenClosed = false
        window.setContentSize(NSSize(width: 360, height: 120))
        window.center()

        let entry = FileOpeningWindowEntry(window: window, hostingController: hostingController)
        windowEntriesByJobId[jobId] = entry

        return entry
    }

    private func closeWindow(for jobId: Int64) {
        guard let entry = windowEntriesByJobId.removeValue(forKey: jobId) else { return }
        entry.window.orderOut(nil)
    }

    // MARK: - Cancellation

    private func cancel(jobId: Int64) {
        IKLogger.general.info("[KD] Canceling the file opening jobId=\(jobId)")
        Task {
            do {
                try await FileOpeningJobs().cancelFileOpening(jobId: jobId)
                IKLogger.general.info("[KD] File opening cancellation accepted jobId=\(jobId)")
            } catch {
                SentrySDK.capture(error: error)
                IKLogger.general.error("[KD] Failed to cancel the file opening jobId=\(jobId): \(error)")
            }
        }
    }
}
