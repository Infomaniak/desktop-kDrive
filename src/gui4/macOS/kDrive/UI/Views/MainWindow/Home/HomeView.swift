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

import Combine
import InfomaniakDI
import kDriveCore
import kDriveCoreUI
import OrderedCollections
import SwiftUI

struct HomeView: View {
    static let spacing = AppPadding.padding24

    @InjectService private var synchroStateObserver: SynchroStateObserving

    @StateObject private var networkObserver = NetworkObserver()
    @State private var synchroState = UISynchroState(errorCount: 0, status: .idle)

    @ObservedObject var mainViewModel: MainViewModel

    private var userName: String {
        guard let currentUser = mainViewModel.currentUser,
              let givenName = try? PersonNameComponents.FormatStyle().parseStrategy.parse(currentUser.name).givenName
        else { return "" }

        return givenName
    }

    private var state: HomeState {
        guard networkObserver.isConnected else {
            return .offline
        }

        switch synchroState.status {
        case .idle:
            return .synchroIsUpToDate
        case .starting, .running:
            return .synchroIsRunning
        case .paused, .stopped, .error:
            return .synchroIsPaused
        case .pauseAsked, .stopAsked:
            return .loading
        }
    }

    var body: some View {
        VStack(alignment: .leading, spacing: AppPadding.padding24) {
            GreetingStatusView(name: userName, state: state)
                .padding(.bottom, AppPadding.padding8)

            if synchroState.errorCount > 0 {
                SynchroErrorsInformationBlockView(errorCount: synchroState.errorCount)
            }

            GeometryReader { proxy in
                HStack(spacing: HomeView.spacing) {
                    SynchroStatusView(state: state)
                        .frame(maxWidth: (proxy.size.width - HomeView.spacing / 2) * 2 / 3)

                    DriveWebShortcutsView(avatar: mainViewModel.currentUser?.avatar, drive: mainViewModel.currentDrive)
                        .frame(maxWidth: (proxy.size.width - HomeView.spacing / 2) * 1 / 3)
                }
            }
        }
        .padding(AppPadding.page)
        .onAppear {
            synchroState = synchroStateObserver.synchroState
        }
        .onReceive(synchroStateObserver.synchroStatePublisher) { output in
            withAnimation {
                synchroState = output
            }
        }
    }
}

#Preview {
    HomeView(mainViewModel: MainViewModel())
}
