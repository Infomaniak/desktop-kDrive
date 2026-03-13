/*
 Infomaniak kDrive - Desktop
 Copyright (C) 2023-2025 Infomaniak Network SA

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

import InfomaniakDI
import kDriveCore
import kDriveCoreUI
import kDriveResources
import SwiftUI

struct DirectionIndicator: StatusIndicator {
    let icon: Image
    let hint: String
    let color: Color

    static let up = DirectionIndicator(
        icon: KDriveResources.computerArrowUp.swiftUIImage,
        hint: KDriveLocalizable.syncedFromComputer,
        color: ColorToken.Text.tertiary.asColor
    )
    static let down = DirectionIndicator(
        icon: KDriveResources.cloudArrowDown.swiftUIImage,
        hint: KDriveLocalizable.syncedFromKDriveWeb,
        color: ColorToken.Text.tertiary.asColor
    )
}

struct StateIndicator: StatusIndicator {
    let icon: Image
    let hint: String
    let color: Color

    static let synchronized = StateIndicator(
        icon: KDriveResources.checkmarkCircle.swiftUIImage,
        hint: KDriveLocalizable.syncSuccessTooltip,
        color: ColorToken.Status.Medium.success.asColor
    )
    static let inProgress = StateIndicator(
        icon: KDriveResources.circularArrowsClockwise.swiftUIImage,
        hint: KDriveLocalizable.syncInProgressTooltip,
        color: .blue
    )
    static let failed = StateIndicator(
        icon: KDriveResources.warning.swiftUIImage,
        hint: KDriveLocalizable.syncErrorTooltip,
        color: ColorToken.Status.Medium.warning.asColor
    )
}

struct ActivitiesTableStatusView: View {
    let context: UISynchroNodeContext

    private var direction: DirectionIndicator {
        guard let nodeDirection = context.node.direction else {
            return .up
        }

        switch nodeDirection {
        case .up:
            return .up
        case .down:
            return .down
        }
    }

    private var status: StateIndicator {
        guard let nodeStatus = context.node.status else {
            return .synchronized
        }

        switch nodeStatus {
        case .idle, .done:
            return .synchronized
        case .syncing:
            return .inProgress
        case .error:
            return .failed
        }
    }

    private var shouldDisplayOptionButton: Bool {
        switch status {
        case .synchronized:
            return context.node.instruction != .remove
        case .failed:
            return true
        default:
            return false
        }
    }

    var body: some View {
        HStack(spacing: AppPadding.padding8) {
            StatusIndicatorView(indicator: direction)
            StatusIndicatorView(indicator: status)

            if shouldDisplayOptionButton {
                Menu {
                    menuContent
                } label: {
                    Label {
                        Text(KDriveLocalizable.buttonShowOption)
                    } icon: {
                        KDriveResources.dotsVertical.swiftUIImage
                            .resizable(at: AppIconSize.iconSize12)
                    }
                    .labelStyle(.iconOnly)
                    .tint(ColorToken.Text.primary.asColor)
                }
                .buttonStyle(.plain)
                .frame(maxWidth: .infinity, alignment: .trailing)
            }
        }
    }

    @ViewBuilder
    private var menuContent: some View {
        if status == .synchronized {
            Section {
                Button(action: openInFinder) {
                    Label { Text(KDriveLocalizable.buttonOpenInFinder) } icon: { KDriveResources.finder.swiftUIImage }
                }
                Button(action: openInBrowser) {
                    Label { Text(KDriveLocalizable.buttonOpenInBrowser) } icon: {
                        KDriveResources.kdriveFoldersStacked.swiftUIImage
                    }
                }
            }

            Section {
                Button(action: copyShareLink) {
                    Label { Text(KDriveLocalizable.buttonCopyShareLink) } icon: { KDriveResources.squareArrowUp.swiftUIImage }
                }
            }
        } else if status == .failed {
            Button(action: navigateToErrorsView) {
                Label { Text(KDriveLocalizable.buttonFixErrors) } icon: { KDriveResources.warning.swiftUIImage }
            }
        }
    }

    private func openInFinder() {
        @InjectService var nodeURLGenerator: NodeURLGenerator
        let pathToLink = context.node.type == .directory ? context.node.path : context.node.parentFolder
        let url = nodeURLGenerator.localURL(for: pathToLink.path, synchroPath: context.synchro.localPath)

        NSWorkspace.shared.open(url)
    }

    private func openInBrowser() {
        @InjectService var nodeURLGenerator: NodeURLGenerator
        let url = nodeURLGenerator.remoteURL(for: context.node.remoteID, driveId: context.drive.driveId)

        NSWorkspace.shared.open(url)
    }

    private func copyShareLink() {
        @InjectService var loadingIndicatorShower: SidebarNotificationPresenting
        loadingIndicatorShower.show(SidebarNotificationState(text: .init(text: KDriveLocalizable.copyingLink), showLoader: true))

        Task { @MainActor in
            @InjectService var nodeURLGenerator: NodeURLGenerator
            let url = try await nodeURLGenerator.shareURL(for: context.node.remoteID, driveDbId: context.drive.dbId)

            let pasteboard = NSPasteboard.general
            pasteboard.clearContents()
            pasteboard.setString(url.absoluteString, forType: .string)

            loadingIndicatorShower.show(
                SidebarNotificationState(
                    icon: .init(icon: KDriveResources.checkmarkCircle.image, tint: ColorToken.Status.Medium.success.asNSColor),
                    text: .init(text: KDriveLocalizable.linkCopiedToClipboardTitle),
                    showLoader: false,
                    duration: SidebarNotificationState.defaultDuration
                )
            )
        }
    }

    private func navigateToErrorsView() {}
}

#Preview {
    ActivitiesTableStatusView(context: PreviewHelper.synchroNodeContext)
        .padding()
}
