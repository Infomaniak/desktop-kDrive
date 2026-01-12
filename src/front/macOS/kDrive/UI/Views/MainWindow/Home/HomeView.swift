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

import kDriveCore
import kDriveCoreUI
import SwiftUI

struct HomeView: View {
    @ObservedObject var mainViewModel: MainViewModel

    static let spacing = AppPadding.padding24

    private var userName: String {
        guard let currentUser = mainViewModel.currentUser,
              let givenName = try? PersonNameComponents.FormatStyle().parseStrategy.parse(currentUser.name).givenName
        else { return "" }

        return givenName
    }

    private var state: SynchroStatusView.State {
        return .synchroInProgress
    }

    var body: some View {
        VStack(alignment: .leading, spacing: AppPadding.padding24) {
            GreetingStatusView(name: userName, synchroStatus: .upToDate)
                .padding(.bottom, AppPadding.padding8)

            GeometryReader { proxy in
                HStack(spacing: HomeView.spacing) {
                    SynchroStatusView(state: state, performAction: didTapStateButton)
                        .frame(maxWidth: (proxy.size.width - HomeView.spacing / 2) * 2 / 3)

                    DriveWebShortcutsView(user: mainViewModel.currentUser, drive: mainViewModel.currentDrive)
                        .frame(maxWidth: (proxy.size.width - HomeView.spacing / 2) * 1 / 3)
                }
            }
        }
        .padding(AppPadding.padding24)
    }

    private func didTapStateButton(for state: SynchroStatusView.State) {
        switch state {
        case .synchroInProgress:
            // TODO: Navigate to activities
            break
        case .synchroPaused:
            // TODO: Enable synchro
            break
        default:
            break
        }
    }
}

#Preview {
    HomeView(mainViewModel: MainViewModel())
}
