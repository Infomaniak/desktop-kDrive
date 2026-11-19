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

/// Presents the pop-up windows informing the user of the progress of a file opening requested through a
/// "kdrive://open/..." deeplink, and of its failure, and forwards the cancellation requests to the server.
@MainActor
final class FileOpeningModalPresenter {
    @LazyInjectService private var fileOpeningStatusCacheObservable: FileOpeningStatusCacheObservable

    private struct WindowEntry {
        let window: NSWindow
        let hostingController: NSHostingController<FileOpeningStatusView>
    }

    private struct FailureWindowEntry {
        let window: NSWindow
        let hostingController: NSHostingController<FileOpeningFailureView>
    }

    private var bindStore = Set<AnyCancellable>()
    private var windowEntriesByJobId = [Int64: WindowEntry]()
    private var failureWindowEntriesByJobId = [Int64: FailureWindowEntry]()
    private var lastStatusByJobId = [Int64: FileOpeningStatus]()
    private var dismissedJobIds = Set<Int64>()

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
        // The server delivers the signals through a concurrent pool: a status can be reordered after the terminal
        // Finished phase of the same request. Ignore such late statuses: the request is already dismissed, and
        // resurrecting it would present a pop-up window that would never close.
        if status.phase != .finished && dismissedJobIds.contains(status.jobId) { return }

        switch status.phase {
        case .finished:
            // The file opening request has been processed: close the progress pop-up window, if one is open.
            dismissedJobIds.insert(status.jobId)
            lastStatusByJobId.removeValue(forKey: status.jobId)
            closeWindow(for: status.jobId)
        case .failed:
            // The file opening request failed: close the progress pop-up window, if one is open, and present the
            // failure window. The identity of the item is the last one reported before the failure.
            let lastKnownStatus = lastStatusByJobId[status.jobId]
            lastStatusByJobId.removeValue(forKey: status.jobId)
            closeWindow(for: status.jobId)
            presentFailureWindow(jobId: status.jobId, itemName: lastKnownStatus?.itemName ?? "",
                                 itemPath: lastKnownStatus?.itemPath ?? "", webUrl: status.webUrl)
        default:
            lastStatusByJobId[status.jobId] = status
            presentProgressWindow(for: status)
        }
    }

    // MARK: - Progress window

    private func presentProgressWindow(for status: FileOpeningStatus) {
        let entry = windowEntry(for: status.jobId)
        entry.hostingController.rootView = FileOpeningStatusView(itemName: status.itemName, itemPath: status.itemPath,
                                                                 phase: status.phase, receivedBytes: status.receivedBytes,
                                                                 totalBytes: status.totalBytes) { [weak self] in
            self?.cancel(jobId: status.jobId)
        }
        entry.window.setContentSize(NSSize(width: 392, height: contentHeight(for: status)))
        entry.window.makeKeyAndOrderFront(nil)
    }

    private func windowEntry(for jobId: Int64) -> WindowEntry {
        if let entry = windowEntriesByJobId[jobId] {
            return entry
        }

        IKLogger.general.info("[KD] Presenting the file opening progress window jobId=\(jobId)")
        let hostingController = NSHostingController(
            rootView: FileOpeningStatusView(itemName: "", itemPath: "", phase: .waitingSync, receivedBytes: -1,
                                            totalBytes: -1) {}
        )
        let window = makeWindow(contentViewController: hostingController)
        configure(window, width: 392, height: 116)

        let entry = WindowEntry(window: window, hostingController: hostingController)
        windowEntriesByJobId[jobId] = entry

        return entry
    }

    // MARK: - Failure window

    private func presentFailureWindow(jobId: Int64, itemName: String, itemPath: String, webUrl: String) {
        guard failureWindowEntriesByJobId[jobId] == nil else { return }

        IKLogger.general.info("[KD] Presenting the file opening failure window jobId=\(jobId)")
        let hostingController = NSHostingController(
            rootView: FileOpeningFailureView(itemName: itemName, itemPath: itemPath, onDownload: { [weak self] in
                self?.openWebPage(webUrl)
            }, onClose: { [weak self] in
                self?.closeFailureWindow(for: jobId)
            })
        )
        let window = makeWindow(contentViewController: hostingController)
        configure(window, width: 392, height: 348)

        let entry = FailureWindowEntry(window: window, hostingController: hostingController)
        failureWindowEntriesByJobId[jobId] = entry
        window.makeKeyAndOrderFront(nil)
    }

    private func closeFailureWindow(for jobId: Int64) {
        guard let entry = failureWindowEntriesByJobId.removeValue(forKey: jobId) else { return }
        entry.window.orderOut(nil)
    }

    private func openWebPage(_ webUrl: String) {
        guard let url = URL(string: webUrl) else {
            IKLogger.general.error("[KD] Invalid file opening web page url: \(webUrl)")
            SentrySDK.capture(message: "Invalid file opening web page url")
            return
        }

        NSWorkspace.shared.open(url)
    }

    // MARK: - Window management

    /// Creates a pop-up window with the title bar of the file opening flow: the native title is hidden and the title
    /// bar made transparent, so that the view draws the "kDrive" title centered in the title bar strip.
    private func makeWindow(contentViewController: NSViewController) -> NSWindow {
        let window = NSWindow(contentViewController: contentViewController)
        window.styleMask = [.titled, .closable, .miniaturizable, .fullSizeContentView]
        window.title = KDriveLocalizable.fileOpeningWindowTitle
        window.titleVisibility = .hidden
        window.titlebarAppearsTransparent = true
        window.isMovableByWindowBackground = true
        window.isReleasedWhenClosed = false
        return window
    }

    private func configure(_ window: NSWindow, width: CGFloat, height: CGFloat) {
        window.setContentSize(NSSize(width: width, height: height))
        window.center()
    }

    /// Returns the progress window content height for the given status: it includes the 28 pt title row drawn by the
    /// view. The waiting-sync window grows when the item card appears (once the identity of the item has been fetched
    /// from the server).
    private func contentHeight(for status: FileOpeningStatus) -> CGFloat {
        if status.phase == .hydrating { return 244 }
        return status.itemName.isEmpty ? 116 : 220
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
