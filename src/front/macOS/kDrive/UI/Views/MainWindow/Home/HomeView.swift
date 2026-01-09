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
import SwiftUI

struct HomeView: View {
    @ObservedObject var mainViewModel: MainViewModel

    static let spacing = AppPadding.padding24

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            GreetingStatusView(name: "Valentin", synchroStatus: .upToDate)
                .padding(.bottom, AppPadding.padding32)

            GeometryReader { proxy in
                HStack(spacing: HomeView.spacing) {
                    SynchroStatusView(status: .upToDate)
                        .frame(maxWidth: (proxy.size.width - HomeView.spacing / 2) * 2 / 3)

                    DriveWebShortcutsView(user: mainViewModel.currentUser, drive: mainViewModel.currentDrive)
                        .frame(maxWidth: (proxy.size.width - HomeView.spacing / 2) * 1 / 3)
                }
            }
        }
        .padding(AppPadding.padding24)
    }
}

#Preview {
    HomeView(mainViewModel: MainViewModel())
}
