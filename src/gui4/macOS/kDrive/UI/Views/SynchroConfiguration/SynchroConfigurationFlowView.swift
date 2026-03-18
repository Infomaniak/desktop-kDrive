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
import kDriveCoreUI
import SwiftUI

struct SynchroConfigurationFlowView: View {
    @StateObject private var viewModel: SynchroConfigurationFlowViewModel

    let userDbId: Int
    let configurations: [SynchroConfiguration]

    init(
        userDbId: Int,
        configurations: [SynchroConfiguration],
        onConfirm: (([SynchroConfiguration]) -> Void)? = nil,
        onCancel: (() -> Void)? = nil
    ) {
        self._viewModel = StateObject(wrappedValue: SynchroConfigurationFlowViewModel(onConfirm: onConfirm, onCancel: onCancel))

        self.userDbId = userDbId
        self.configurations = configurations
    }

    var body: some View {
        ZStack {
            switch viewModel.state {
            case .driveSelector:
                SynchroConfigurationPickerView()
            case .configureSynchro(let synchroConfiguration):
                SynchroConfigurationView(configuration: synchroConfiguration)
            case .selectFolders(let synchroConfiguration):
                SelectSynchroFoldersView(userDbId: userDbId, configuration: synchroConfiguration)
            }
        }
        .environmentObject(viewModel)
        .onAppear {
            viewModel.setupInitialConfigurations(configurations)
        }
    }
}

#Preview {
    SynchroConfigurationFlowView(userDbId: PreviewHelper.user.dbId, configurations: [])
}
