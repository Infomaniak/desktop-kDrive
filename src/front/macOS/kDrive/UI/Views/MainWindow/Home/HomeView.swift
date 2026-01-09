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
        GeometryReader { proxy in
            HStack(spacing: HomeView.spacing) {
                Color.clear
                    .frame(width: (proxy.size.width - HomeView.spacing / 2) * 0.66)

                DriveWebShortcutsView(user: mainViewModel.currentUser, drive: mainViewModel.currentDrive)
                    .frame(width: (proxy.size.width - HomeView.spacing / 2) * 0.33)
            }
        }
        .padding(AppPadding.padding24)
    }
}

#Preview {
    HomeView(mainViewModel: MainViewModel())
}
