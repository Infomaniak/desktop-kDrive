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

import kDriveCore
import kDriveCoreUI
import kDriveResources
import SwiftUI

struct SelectSynchroFoldersView: View {
    @EnvironmentObject private var viewModel: SynchroConfigurationFlowViewModel

    @State private var blackList = Set<String>()

    let userDbId: Int
    let configuration: SynchroConfiguration

    var body: some View {
        FoldersToSynchroView(
            blackList: $blackList,
            initialBlackList: Set(configuration.blackList),
            userDbId: userDbId,
            driveDbId: configuration.drive.id
        )
        .padding()
        .toolbar {
            ToolbarItem(placement: .confirmationAction) {
                Button(KDriveLocalizable.buttonValidate) {
                    viewModel.updateConfiguration(configuration.id, blackList: Array(blackList))
                    viewModel.navigate(to: .configureSynchro(configuration))
                }
                .keyboardShortcut(.defaultAction)
            }

            ToolbarItem(placement: .cancellationAction) {
                Button(KDriveLocalizable.buttonCancel, role: .cancel) {
                    viewModel.navigate(to: .configureSynchro(configuration))
                }
                .keyboardShortcut(.cancelAction)
            }
        }
        .onAppear {
            blackList = Set(configuration.blackList)
        }
    }
}

#Preview {
    SelectSynchroFoldersView(
        userDbId: PreviewHelper.user.dbId,
        configuration: SynchroConfiguration(drive: PreviewHelper.drive1, localFolder: .init(), blackList: [])
    )
}
