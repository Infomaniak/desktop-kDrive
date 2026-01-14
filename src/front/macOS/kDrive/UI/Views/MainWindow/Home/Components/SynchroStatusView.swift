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

extension SynchroStatus {
    var state: SynchroStatusView.State {
        switch self {
        case .upToDate:
            return .synchroIdle
        case .inProgress:
            return .synchroInProgress
        case .paused:
            return .synchroPaused
        }
    }
}

struct SynchroStatusView: View {
    struct State: Equatable {
        let id: Int
        let animation: ThemedAnimation
        let title: String
        let description: String
        let buttonTitle: String?

        static func == (lhs: State, rhs: State) -> Bool {
            return lhs.id == rhs.id
        }

        static let synchroIdle = State(
            id: 1,
            animation: .kDriveCheckmark,
            title: KDriveLocalizable.synchroStatusUpToDateTitle,
            description: KDriveLocalizable.synchroStatusUpToDateDescription,
            buttonTitle: nil
        )

        static let synchroInProgress = State(
            id: 2,
            animation: .cloudSync,
            title: KDriveLocalizable.synchroStatusInProgressTitle,
            description: KDriveLocalizable.synchroStatusInProgressDescription,
            buttonTitle: "Voir les activités"
        )

        static let synchroPaused = State(
            id: 3,
            animation: .cloudPause,
            title: KDriveLocalizable.synchroStatusPausedTitle,
            description: KDriveLocalizable.synchroStatusPausedDescription,
            buttonTitle: "Réactiver la synchronisation"
        )

        static let offline = State(
            id: 4,
            animation: .offline,
            title: KDriveLocalizable.synchroStatusOfflineTitle,
            description: KDriveLocalizable.synchroStatusOfflineDescription,
            buttonTitle: nil
        )
    }

    let state: SynchroStatusView.State
    let performAction: (SynchroStatusView.State) -> Void

    var body: some View {
        VStack(spacing: AppPadding.padding32) {
            ThemedLottieView(animation: state.animation)

            VStack(spacing: AppPadding.padding8) {
                Text(state.title)
                    .font(.Tokens.title3Emphasized)

                Text(state.description)
                    .font(.Tokens.body)

                if let buttonTitle = state.buttonTitle {
                    Button(buttonTitle) {
                        performAction(state)
                    }
                    .buttonStyle(.borderless)
                    .foregroundStyle(.tint)
                }
            }
            .foregroundStyle(ColorToken.Text.primary.asColor)
            .multilineTextAlignment(.center)
            .frame(maxWidth: 300)
        }
        .padding(AppPadding.padding16)
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .background(ColorToken.Surface.primary.asColor)
        .clipShape(RoundedRectangle(cornerRadius: AppRadius.radius16))
    }
}

#Preview("Idle") {
    SynchroStatusView(state: .synchroInProgress) { _ in }
}

#Preview("In Progress") {
    SynchroStatusView(state: .synchroInProgress) { _ in }
}

#Preview("Pause") {
    SynchroStatusView(state: .synchroPaused) { _ in }
}

#Preview("Offline") {
    SynchroStatusView(state: .offline) { _ in }
}
