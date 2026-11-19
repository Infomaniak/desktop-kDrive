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
    @LazyInjectService private var fileOpeningDownloadStateCacheObservable: FileOpeningDownloadStateCacheObservable

    private struct WindowEntry {
        let window: NSWindow
        let hostingController: NSHostingController<FileOpeningStatusView>
    }

    private struct FailureWindowEntry {
        let window: NSWindow
        let hostingController: NSHostingController<FileOpeningFailureView>
        let jobId: Int64
        let phase: FileOpeningStatusPhase
        let itemName: String
        let itemPath: String
        let webUrl: String
        let driveDbId: Int64
        let remoteNodeId: String
        /// Id of the download of the item into the download folder of the user, once it has been started.
        var downloadId: Int64?
    }

    private var bindStore = Set<AnyCancellable>()
    private var windowEntriesByJobId = [Int64: WindowEntry]()
    private var failureWindowEntriesByJobId = [Int64: FailureWindowEntry]()
    private var failureWindowJobIdsByDownloadId = [Int64: Int64]()
    private var lastStatusByJobId = [Int64: FileOpeningStatus]()
    private var dismissedJobIds = Set<Int64>()

    init() {
        fileOpeningStatusCacheObservable.fileOpeningStatusPublisher
            .receive(on: DispatchQueue.main)
            .sink { [weak self] status in
                self?.handleStatus(status)
            }
            .store(in: &bindStore)
        fileOpeningDownloadStateCacheObservable.fileOpeningDownloadStatePublisher
            .receive(on: DispatchQueue.main)
            .sink { [weak self] update in
                self?.handleDownloadState(update)
            }
            .store(in: &bindStore)
    }

    // MARK: - Status handling

    private func handleStatus(_ status: FileOpeningStatus) {
        // The server delivers the signals through a concurrent pool: a status can be reordered after the terminal
        // phases of the same request. Ignore such late statuses: the request is already dismissed, and resurrecting
        // it would present a pop-up window that would never close. The failure phases are delivered after the
        // Finished one and must not be dropped: they open the failure window.
        if status.phase != .finished && status.phase != .failed && status.phase != .notFound &&
            dismissedJobIds.contains(status.jobId) { return }

        switch status.phase {
        case .finished:
            // The file opening request has been processed: close the progress pop-up window, if one is open. The
            // last reported state is kept: the failure phases are delivered after the Finished one and need the
            // identity of the item to feed the failure window.
            dismissedJobIds.insert(status.jobId)
            closeWindow(for: status.jobId)
        case .failed, .notFound:
            // The file opening request failed: close the progress pop-up window, if one is open, and present the
            // failure window. The identity of the item is the last one reported before the failure.
            dismissedJobIds.insert(status.jobId)
            let lastKnownStatus = lastStatusByJobId[status.jobId]
            lastStatusByJobId.removeValue(forKey: status.jobId)
            closeWindow(for: status.jobId)
            presentFailureWindow(status: status, lastKnownStatus: lastKnownStatus)
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

    /// Presents the failure window of a failed request. The identity of the item (its name and its parent folder
    /// path) is the last one reported before the failure, and its server identity (its drive database id and its
    /// remote node id) comes from the failure status itself.
    private func presentFailureWindow(status: FileOpeningStatus, lastKnownStatus: FileOpeningStatus?) {
        let jobId = status.jobId
        guard failureWindowEntriesByJobId[jobId] == nil else { return }

        let itemName = lastKnownStatus?.itemName ?? ""
        let itemPath = lastKnownStatus?.itemPath ?? ""

        IKLogger.general.info("[KD] Presenting the file opening failure window jobId=\(jobId)")
        let hostingController = NSHostingController(
            rootView: FileOpeningFailureView(itemName: itemName, itemPath: itemPath, isNotFound: status.phase == .notFound,
                                             webUrl: status.webUrl, onDownload: { [weak self] in
                                                 self?.startDownload(for: jobId)
                                             }, onOpenWebPage: { [weak self] in
                                                 self?.openWebPage(status.webUrl)
                                             }, onClose: { [weak self] in
                                                 self?.closeFailureWindow(for: jobId)
                                             })
        )
        let window = makeWindow(contentViewController: hostingController, standardTitleBar: status.phase == .notFound)
        configure(window, width: 392, height: failureWindowHeight(phase: status.phase, itemName: itemName))

        let entry = FailureWindowEntry(window: window, hostingController: hostingController, jobId: jobId,
                                       phase: status.phase, itemName: itemName, itemPath: itemPath,
                                       webUrl: status.webUrl, driveDbId: status.driveDbId,
                                       remoteNodeId: status.remoteNodeId, downloadId: nil)
        failureWindowEntriesByJobId[jobId] = entry
        window.makeKeyAndOrderFront(nil)
    }

    /// Returns the failure window content height for the given failure: the not-found variant drops the explanation
    /// and the download button, carries the "kDrive" title in its standard title bar instead of a drawn title row,
    /// and the card appears only when the identity of the item is known. The download states replace the download
    /// button with the progress of the download, or with an error message followed by the fallback button.
    private func failureWindowHeight(phase: FileOpeningStatusPhase, itemName: String,
                                     downloadPhase: FileOpeningFailureView.DownloadPhase = .idle) -> CGFloat {
        if phase == .notFound { return itemName.isEmpty ? 150 : 210 }
        switch downloadPhase {
        case .idle, .downloading:
            return 348
        case .failed:
            return 384
        }
    }

    /// Starts the download of the item of the failure window into the download folder of the user. The identity of
    /// the item is required: when it is unknown, the kDrive web application page of the item is opened instead.
    private func startDownload(for jobId: Int64) {
        guard let entry = failureWindowEntriesByJobId[jobId] else { return }
        guard entry.driveDbId != 0, !entry.remoteNodeId.isEmpty else {
            openWebPage(entry.webUrl)
            return
        }

        IKLogger.general.info("[KD] Downloading the item of the file opening jobId=\(jobId) into the download folder")
        Task {
            do {
                let response = try await FileOpeningJobs().downloadFileOpeningItem(driveDbId: entry.driveDbId,
                                                                                   remoteNodeId: entry.remoteNodeId,
                                                                                   destinationFolderPath: "")
                IKLogger.general
                    .info(
                        "[KD] Downloading the item of the file opening jobId=\(jobId) downloadId=\(response.downloadId) into \(response.finalPath)"
                    )
                setFailureWindowDownloadPhase(jobId: jobId, downloadId: response.downloadId,
                                              phase: .downloading, receivedBytes: 0, totalBytes: -1)
            } catch {
                IKLogger.general
                    .error("[KD] Failed to start the download of the item of the file opening jobId=\(jobId): \(error)")
                SentrySDK.capture(error: error)
                setFailureWindowDownloadPhase(jobId: jobId, downloadId: nil, phase: .failed, receivedBytes: -1,
                                              totalBytes: -1)
            }
        }
    }

    /// Handles a download state signal: updates the progress shown by the failure window, closes the window once the
    /// item has been downloaded, or falls back to the kDrive web application when the download failed.
    private func handleDownloadState(_ update: FileOpeningDownloadStateUpdate) {
        guard let jobId = failureWindowJobIdsByDownloadId[update.downloadId] else { return }

        switch update.state {
        case .downloading:
            setFailureWindowDownloadPhase(jobId: jobId, downloadId: update.downloadId, phase: .downloading,
                                          receivedBytes: update.receivedBytes, totalBytes: update.totalBytes)
        case .done:
            IKLogger.general.info("[KD] Item of the file opening jobId=\(jobId) downloaded into \(update.finalPath)")
            closeFailureWindow(for: jobId)
        case .failed:
            IKLogger.general.error("[KD] Failed to download the item of the file opening jobId=\(jobId)")
            setFailureWindowDownloadPhase(jobId: jobId, downloadId: nil, phase: .failed, receivedBytes: -1,
                                          totalBytes: -1)
        }
    }

    /// Rebuilds the content of the failure window for the given download state, and resizes the window accordingly.
    private func setFailureWindowDownloadPhase(jobId: Int64, downloadId: Int64?, phase: FileOpeningFailureView.DownloadPhase,
                                               receivedBytes: Int64, totalBytes: Int64) {
        guard var entry = failureWindowEntriesByJobId[jobId] else { return }

        if let downloadId {
            failureWindowJobIdsByDownloadId[downloadId] = jobId
        }
        entry.downloadId = downloadId
        entry.hostingController.rootView = FileOpeningFailureView(
            itemName: entry.itemName, itemPath: entry.itemPath, isNotFound: entry.phase == .notFound,
            webUrl: entry.webUrl, downloadPhase: phase, receivedBytes: receivedBytes, totalBytes: totalBytes,
            onDownload: { [weak self] in
                self?.startDownload(for: jobId)
            }, onOpenWebPage: { [weak self] in
                self?.openWebPage(entry.webUrl)
            }, onClose: { [weak self] in
                self?.closeFailureWindow(for: jobId)
            }
        )
        entry.window.setContentSize(NSSize(width: 392, height: failureWindowHeight(phase: entry.phase,
                                                                                   itemName: entry.itemName,
                                                                                   downloadPhase: phase)))
        failureWindowEntriesByJobId[jobId] = entry
    }

    private func closeFailureWindow(for jobId: Int64) {
        guard let entry = failureWindowEntriesByJobId.removeValue(forKey: jobId) else { return }
        if let downloadId = entry.downloadId {
            failureWindowJobIdsByDownloadId.removeValue(forKey: downloadId)
        }
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

    /// Creates a pop-up window. By default it carries the title bar of the file opening flow: the native title is
    /// hidden and the title bar made transparent, so that the view draws the "kDrive" title centered in the title
    /// bar strip. With a standard title bar, the native "kDrive" title is shown in the title bar instead.
    private func makeWindow(contentViewController: NSViewController, standardTitleBar: Bool = false) -> NSWindow {
        let window = NSWindow(contentViewController: contentViewController)
        window.styleMask = standardTitleBar ? [.titled, .closable] : [.titled, .closable, .miniaturizable, .fullSizeContentView]
        window.title = KDriveLocalizable.fileOpeningWindowTitle
        window.titleVisibility = standardTitleBar ? .visible : .hidden
        window.titlebarAppearsTransparent = !standardTitleBar
        window.isMovableByWindowBackground = !standardTitleBar
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
