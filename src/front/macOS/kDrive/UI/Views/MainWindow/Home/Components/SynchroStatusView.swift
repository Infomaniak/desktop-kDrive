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

extension HomeState {
    var animation: ThemedAnimation {
        switch self {
        case .loading, .synchroIsUpToDate:
            return .kDriveCheckmark
        case .synchroIsPaused:
            return .cloudPause
        case .synchroIsRunning:
            return .cloudSync
        case .offline:
            return .offline
        }
    }

    var title: String {
        switch self {
        case .loading, .synchroIsUpToDate:
            return KDriveLocalizable.synchroStatusUpToDateTitle
        case .synchroIsPaused:
            return KDriveLocalizable.synchroStatusPausedTitle
        case .synchroIsRunning:
            return KDriveLocalizable.synchroStatusInProgressTitle
        case .offline:
            return KDriveLocalizable.synchroStatusOfflineTitle
        }
    }

    var description: String {
        switch self {
        case .loading, .synchroIsUpToDate:
            return KDriveLocalizable.synchroStatusUpToDateDescription
        case .synchroIsPaused:
            return KDriveLocalizable.synchroStatusPausedDescription
        case .synchroIsRunning:
            return KDriveLocalizable.synchroStatusInProgressDescription
        case .offline:
            return KDriveLocalizable.synchroStatusOfflineDescription
        }
    }

    var buttonTitle: String? {
        switch self {
        case .synchroIsPaused:
            return KDriveLocalizable.buttonRestartSynchro
        case .synchroIsRunning:
            return KDriveLocalizable.buttonSeeActivities
        case .loading, .synchroIsUpToDate, .offline:
            return nil
        }
    }
}

struct SynchroStatusView: View {
    let state: HomeState
    let performAction: (HomeState) -> Void

    var body: some View {
        VStack(spacing: AppPadding.padding32) {
            ThemedLottieView(animation: state.animation)
                .overlay {
                    if state.isRedacted {
                        RoundedRectangle(cornerRadius: AppRadius.radius16)
                            .fill(Color(nsColor: .lightGray))
                    }
                }

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
        .redacted(reason: state.isRedacted ? .placeholder : [])
        .padding(AppPadding.padding16)
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .background(ColorToken.Surface.primary.asColor)
        .clipShape(RoundedRectangle(cornerRadius: AppRadius.radius16))
    }
}

#Preview("Up To Date") {
    SynchroStatusView(state: .synchroIsUpToDate) { _ in }
}

#Preview("Running") {
    SynchroStatusView(state: .synchroIsRunning) { _ in }
}

#Preview("Paused") {
    SynchroStatusView(state: .synchroIsPaused) { _ in }
}

#Preview("Offline") {
    SynchroStatusView(state: .offline) { _ in }
}

#Preview("Loading") {
    SynchroStatusView(state: .loading) { _ in }
}
