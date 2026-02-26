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
import kDriveCoreUI
import kDriveResources
import Lottie
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

    var animationLoopMode: Lottie.LottieLoopMode {
        switch self {
        case .synchroIsUpToDate:
            return .playOnce
        default:
            return .loop
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

    var body: some View {
        VStack(spacing: AppPadding.padding32) {
            ThemedLottieView(animation: state.animation, loopMode: state.animationLoopMode)
                .opacity(state.isRedacted ? 0 : 1)
                .overlay {
                    if state.isRedacted {
                        redactedAnimation
                    }
                }

            VStack(spacing: AppPadding.padding8) {
                Text(state.title)
                    .font(.Tokens.title3Emphasized)

                Text(state.description)
                    .font(.Tokens.body)

                if let buttonTitle = state.buttonTitle {
                    Button(buttonTitle) {
                        didTapStateButton(for: state)
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
        .background {
            GeometryReader { proxy in
                RadialGradient(
                    colors: [
                        ColorToken.Action.primary.asColor.opacity(0.4),
                        ColorToken.Surface.primary.asColor
                    ],
                    center: UnitPoint(x: 0.5, y: -0.5),
                    startRadius: 0,
                    endRadius: max(proxy.size.width, proxy.size.height)
                )
            }
        }
        .clipShape(.rect(cornerRadius: AppRadius.radius16))
    }

    /// We use a random image to serve as a redacted view
    @ViewBuilder private var redactedAnimation: some View {
        KDriveResources.checkmark.swiftUIImage
            .resizable()
    }

    private func didTapStateButton(for state: HomeState) {
        switch state {
        case .synchroIsRunning:
            @InjectService var router: MainViewRouter
            router.setCurrentTab(.activities)
        case .synchroIsPaused:
            // TODO: Enable synchro
            break
        default:
            break
        }
    }
}

#Preview("Up To Date") {
    SynchroStatusView(state: .synchroIsUpToDate)
}

#Preview("Running") {
    SynchroStatusView(state: .synchroIsRunning)
}

#Preview("Paused") {
    SynchroStatusView(state: .synchroIsPaused)
}

#Preview("Offline") {
    SynchroStatusView(state: .offline)
}

#Preview("Loading") {
    SynchroStatusView(state: .loading)
}
