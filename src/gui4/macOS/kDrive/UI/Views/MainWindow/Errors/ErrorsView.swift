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

import kDriveCoreUI
import kDriveResources
import SwiftUI

struct ErrorsView: View {
    @ObservedObject var mainViewModel: MainViewModel

    @ObservedSynchroErrors private var synchroErrors

    private var errorsCount: Int {
        return synchroErrors.flatMap(\.value).count
    }

    var body: some View {
        ScrollView {
            if errorCount == 0 {
                IKContentUnavailableView(
                    image: KDriveResources.mountainsTreesSun.swiftUIImage,
                    title: KDriveLocalizable.labelNoErrorsToFix,
                    action: .init(
                        title: KDriveLocalizable.buttonSeeActivities,
                        action: navigateBackToActivities
                    )
                )
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
                .padding(AppPadding.page)
            } else {
                VStack(spacing: AppPadding.padding12) {
                    ErrorsHeaderView(synchroDbId: mainViewModel.currentSynchro?.dbId, errorsCount: errorsCount)
                        .padding([.horizontal, .top], AppPadding.page)
                    ErrorsListView(errors: synchroErrors)
                }
            }
        }
    }

    private func navigateBackToActivities() {}
}

#Preview {
    ErrorsView(mainViewModel: MainViewModel())
}
