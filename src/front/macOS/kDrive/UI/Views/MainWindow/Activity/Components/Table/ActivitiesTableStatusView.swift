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

import kDriveCoreUI
import kDriveResources
import SwiftUI

struct DirectionIndicator: StatusIndicator {
    let icon: Image
    let hint: String
    let color: Color

    static let up = DirectionIndicator(
        icon: KDriveResources.computerArrowUp.swiftUIImage,
        hint: "Synchronisé depuis votre ordinateur",
        color: ColorToken.Text.tertiary.asColor
    )
    static let down = DirectionIndicator(
        icon: KDriveResources.cloudArrowDown.swiftUIImage,
        hint: "Synchronisé depuis kDrive web",
        color: ColorToken.Text.tertiary.asColor
    )
}

struct StateIndicator: StatusIndicator {
    let icon: Image
    let hint: String
    let color: Color

    static let synchronized = StateIndicator(
        icon: KDriveResources.checkmarkCircle.swiftUIImage,
        hint: "Synchronisé",
        color: ColorToken.Status.Medium.success.asColor
    )
    static let inProgress = StateIndicator(
        icon: KDriveResources.circularArrowsClockwise.swiftUIImage,
        hint: "Synchronisation en cours",
        color: .blue
    )
    static let failed = StateIndicator(
        icon: KDriveResources.warning.swiftUIImage,
        hint: "Erreur de synchronisation",
        color: ColorToken.Status.Medium.warning.asColor
    )
}

struct ActivitiesTableStatusView: View {
    let node: UISynchroNode

    private var direction: DirectionIndicator {
        guard let nodeDirection = node.direction else {
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
        guard let nodeStatus = node.status else {
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
        return status == .synchronized || status == .failed
    }

    var body: some View {
        HStack(spacing: AppPadding.padding8) {
            StatusIndicatorView(indicator: direction)
            StatusIndicatorView(indicator: status)

            if shouldDisplayOptionButton {
                Menu {
                    menuContent
                } label: {
                    Label { Text("Show options") } icon: { KDriveResources.dotsVertical.swiftUIImage }
                        .labelStyle(.iconOnly)
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
                Button(action: navigateToErrorsView) {
                    Label { Text("Copier le lien de partage") } icon: { KDriveResources.squareArrowUp.swiftUIImage }
                }
            }
        } else if status == .failed {
            Button(action: navigateToErrorsView) {
                Label { Text(KDriveLocalizable.buttonFixErrors) } icon: { KDriveResources.warning.swiftUIImage }
            }
        }
    }

    private func openInFinder() {
        // TODO: Open in finder
    }

    private func openInBrowser() {
        // TODO: Open in browser
    }

    private func navigateToErrorsView() {
        // TODO: Navigate to correct folder
    }
}

#Preview {
    ActivitiesTableStatusView(
        node: UISynchroNode(
            id: "42",
            type: .file,
            path: URL(fileURLWithPath: "/"),
            direction: .up,
            status: .done
        )
    )
}
