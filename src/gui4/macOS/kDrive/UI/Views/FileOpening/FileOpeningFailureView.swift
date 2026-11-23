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
import kDriveCore
import kDriveCoreUI
import kDriveResources
import SwiftUI
import UniformTypeIdentifiers

/// Window title of the file opening pop-up windows: horizontally centered in the title bar strip, where the traffic
/// lights sit. The native title is left-aligned next to the traffic lights since macOS 26, while the design requires
/// it centered: the presenter hides the native title, makes the title bar transparent, and the row extends over it.
struct FileOpeningWindowTitleRow: View {
    var body: some View {
        Text(KDriveLocalizable.fileOpeningWindowTitle)
            .font(Font.Tokens.headline)
            .foregroundStyle(ColorToken.Text.primary.asColor)
            .frame(maxWidth: .infinity)
            .frame(height: 28)
    }
}

/// Card identifying an item through its icon, name, and parent folder path, shared by the pop-up windows of the
/// file opening flow.
struct FileOpeningItemCard: View {
    let itemName: String
    let itemPath: String

    var body: some View {
        HStack(spacing: AppPadding.padding12) {
            Image(nsImage: fileIcon)
                .resizable()
                .frame(width: 40, height: 40)
            VStack(alignment: .leading, spacing: 2) {
                Text(itemName)
                    .font(.headline)
                    .foregroundStyle(ColorToken.Text.primary.asColor)
                    .lineLimit(1)
                    .truncationMode(.middle)
                if !itemPath.isEmpty {
                    Text(itemPath)
                        .font(.subheadline)
                        .foregroundStyle(ColorToken.Text.secondary.asColor)
                        .lineLimit(1)
                        .truncationMode(.middle)
                }
            }
            Spacer(minLength: 0)
        }
        .padding(AppPadding.padding12)
        .background(RoundedRectangle(cornerRadius: AppRadius.radius12).fill(ColorToken.Surface.primary.asColor))
    }

    private var fileIcon: NSImage {
        let pathExtension = (itemName as NSString).pathExtension
        let fileType = UTType(filenameExtension: pathExtension) ?? .data
        let icon = NSWorkspace.shared.icon(for: fileType)
        icon.size = NSSize(width: 40, height: 40)
        return icon
    }
}

/// Content of the failure pop-up window: a title, an explanation inviting the user to download the item from kDrive
/// in a browser, the card identifying the item when its identity is known, and the actions: downloading the item into
/// the download folder of the user, and closing the window. When the item does not exist on the remote side (not
/// found), there is nothing to download: the window shows a dedicated message and offers no download.
struct FileOpeningFailureView: View {
    /// State of the download of the item into the download folder of the user, triggered from the window.
    enum DownloadPhase {
        /// The item can be downloaded into the download folder of the user.
        case idle
        /// The item is being downloaded: the window shows the progress of the download.
        case downloading
        /// The download failed: the button downloads the item into the download folder of the user again.
        case failed
    }

    let itemName: String
    let itemPath: String
    let isNotFound: Bool
    /// True when the identity of the item is known: the button can then trigger its download into the download
    /// folder of the user.
    let canDownload: Bool
    var downloadPhase: DownloadPhase = .idle
    var receivedBytes: Int64 = -1
    var totalBytes: Int64 = -1
    let onDownload: () -> Void
    let onClose: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            // The not-found variant is a standard window whose title bar carries the "kDrive" title: it draws no
            // title row of its own.
            if !isNotFound {
                FileOpeningWindowTitleRow()
            }

            VStack(alignment: .leading, spacing: AppPadding.padding16) {
                Text(isNotFound ? KDriveLocalizable.fileOpeningNotFoundMessage : KDriveLocalizable.fileOpeningFailureTitle)
                    .font(Font.Tokens.title2Emphasized)
                    .foregroundStyle(ColorToken.Text.primary.asColor)

                if !isNotFound {
                    Text(KDriveLocalizable.fileOpeningFailureExplanation)
                        .font(.body)
                        .foregroundStyle(ColorToken.Text.primary.asColor)
                }

                if !itemName.isEmpty {
                    FileOpeningItemCard(itemName: itemName, itemPath: itemPath)
                }

                if !isNotFound && canDownload {
                    switch downloadPhase {
                    case .idle, .failed:
                        actionButton(KDriveLocalizable.fileOpeningDownloadButton,
                                     background: ColorToken.Action.primary.asColor,
                                     foreground: ColorToken.Action.onPrimary.asColor,
                                     action: onDownload)
                    case .downloading:
                        downloadProgress
                    }
                }

                if !isNotFound && downloadPhase == .failed {
                    Text(KDriveLocalizable.fileOpeningDownloadFailureMessage)
                        .font(.subheadline)
                        .foregroundStyle(ColorToken.Text.primary.asColor)
                }

                actionButton(KDriveLocalizable.buttonClose,
                             background: ColorToken.Surface.secondary.asColor,
                             foreground: ColorToken.Text.primary.asColor,
                             action: onClose)
            }
            .padding(AppPadding.padding16)
        }
        .frame(width: 392)
        .ignoresSafeArea(edges: isNotFound ? [] : .top)
    }

    /// Progress of the download of the item into the download folder of the user, as shown by the progress window.
    @ViewBuilder
    private var downloadProgress: some View {
        ProgressView(value: progressFraction)
            .progressViewStyle(DownloadGaugeStyle())

        if totalBytes > 0, receivedBytes >= 0 {
            Text(KDriveLocalizable.fileOpeningDownloadProgress(
                Self.byteCountFormatter.string(fromByteCount: receivedBytes),
                Self.byteCountFormatter.string(fromByteCount: totalBytes)
            ))
            .font(.subheadline)
            .foregroundStyle(ColorToken.Text.secondary.asColor)
        }
    }

    /// Formats the downloaded amounts as localized byte sizes (e.g. "12,5 Mo" in French).
    private static let byteCountFormatter: ByteCountFormatter = {
        let formatter = ByteCountFormatter()
        formatter.countStyle = .file
        return formatter
    }()

    private var progressFraction: Double {
        guard totalBytes > 0, receivedBytes >= 0 else { return 0 }
        return min(max(Double(receivedBytes) / Double(totalBytes), 0), 1)
    }

    private func actionButton(_ title: String, background: Color, foreground: Color, action: @escaping () -> Void)
        -> some View {
        Button(action: action) {
            Text(title)
                .font(.headline)
                .foregroundStyle(foreground)
                .frame(maxWidth: .infinity)
                .padding(.vertical, 10)
        }
        .background(Capsule().fill(background))
        .buttonStyle(.plain)
    }
}
