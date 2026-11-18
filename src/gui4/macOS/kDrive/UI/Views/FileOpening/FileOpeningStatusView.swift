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

/// Content of the file opening pop-up window: while the item is being synchronized, an explanation, a card
/// identifying the item through its icon, name, and parent folder path once it has been fetched from the server, and
/// a cancellation cross; while it is being downloaded, a "Download" title, the same card, a progress gauge with the
/// cross, and the downloaded amount.
struct FileOpeningStatusView: View {
    let itemName: String
    let itemPath: String
    let phase: FileOpeningStatusPhase
    let receivedBytes: Int64
    let totalBytes: Int64
    let onCancel: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            // Window title, horizontally centered in the title bar strip, where the traffic lights sit: the native
            // title is left-aligned next to the traffic lights since macOS 26, while the design requires it centered
            // (the native title is hidden and the title bar made transparent by the presenter).
            Text(KDriveLocalizable.fileOpeningWindowTitle)
                .font(Font.Tokens.headline)
                .foregroundStyle(ColorToken.Text.primary.asColor)
                .frame(maxWidth: .infinity)
                .frame(height: 28)

            VStack(alignment: .leading, spacing: AppPadding.padding16) {
                if phase == .hydrating {
                    downloadContent
                } else {
                    waitingSyncContent
                }
            }
            .padding(AppPadding.padding16)
        }
        .frame(width: 392)
        // Extend to the very top of the window, so that the title row covers the title bar strip instead of being
        // pushed below it by the title bar safe area inset.
        .ignoresSafeArea(edges: .top)
    }

    // MARK: - Waiting for synchronization phase

    @ViewBuilder
    private var waitingSyncContent: some View {
        Text(KDriveLocalizable.fileOpeningWaitingSyncMessage)
            .font(.body)
            .foregroundStyle(ColorToken.Text.primary.asColor)

        if !itemName.isEmpty {
            itemCard
        }

        HStack {
            Spacer(minLength: 0)
            cancelButton
        }
    }

    // MARK: - Downloading phase

    @ViewBuilder
    private var downloadContent: some View {
        Text(KDriveLocalizable.fileOpeningDownloadTitle)
            .font(Font.Tokens.title2Emphasized)
            .foregroundStyle(ColorToken.Text.primary.asColor)

        itemCard

        HStack(spacing: AppPadding.padding12) {
            ProgressView(value: progressFraction)
                .progressViewStyle(DownloadGaugeStyle())
            cancelButton
        }

        if totalBytes > 0, receivedBytes >= 0 {
            Text(KDriveLocalizable.fileOpeningDownloadProgress(
                Self.byteCountFormatter.string(fromByteCount: receivedBytes),
                Self.byteCountFormatter.string(fromByteCount: totalBytes)
            ))
            .font(.subheadline)
            .foregroundStyle(ColorToken.Text.secondary.asColor)
        }
    }

    // MARK: - Item card

    /// Card identifying the item through its icon, name, and parent folder path, shared by both phases.
    private var itemCard: some View {
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

    private var fileIcon: NSImage {
        let pathExtension = (itemName as NSString).pathExtension
        let fileType = UTType(filenameExtension: pathExtension) ?? .data
        let icon = NSWorkspace.shared.icon(for: fileType)
        icon.size = NSSize(width: 40, height: 40)
        return icon
    }

    // MARK: - Cancellation

    private var cancelButton: some View {
        Button(action: onCancel) {
            Image(systemName: "xmark")
                .font(.system(size: 10, weight: .bold))
                .foregroundStyle(ColorToken.Surface.primary.asColor)
                .frame(width: 24, height: 24)
                .background(Circle().fill(ColorToken.Text.primary.asColor))
        }
        .buttonStyle(.plain)
        .help(KDriveLocalizable.buttonCancel)
        .accessibilityLabel(KDriveLocalizable.buttonCancel)
    }
}

/// Gauge made of rounded capsules, matching the file opening mockup.
private struct DownloadGaugeStyle: ProgressViewStyle {
    private let height: CGFloat = 8

    func makeBody(configuration: Configuration) -> some View {
        GeometryReader { geometry in
            let completedWidth = geometry.size.width * CGFloat(configuration.fractionCompleted ?? 0)
            ZStack(alignment: .leading) {
                Capsule().fill(ColorToken.Surface.tertiary.asColor)
                Capsule()
                    .fill(ColorToken.Accent.primary.asColor)
                    .frame(width: max(completedWidth, height))
            }
        }
        .frame(height: height)
    }
}
