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
import OrderedCollections
import SwiftUI

struct SynchroConfigurationPickerView: View {
    @EnvironmentObject private var viewModel: SynchroConfigurationFlowViewModel

    var body: some View {
        Form {
            Section {} header: {
                VStack(alignment: .leading, spacing: AppPadding.padding8) {
                    Text(KDriveLocalizable.onboardingAdvancedSettingsDriveSelectionTitle)
                        .font(.Tokens.headline)
                    Text(KDriveLocalizable.onboardingAdvancedSettingsDriveSelectionDescription)
                        .font(.Tokens.body)
                }
                .foregroundStyle(ColorToken.Text.primary.asColor)
            }

            ForEach(Array(viewModel.configurations.values)) { configuration in
                Section {
                    ConfigurableSynchroView(configuration: configuration)
                }
            }
        }
        .groupedFormatStyle()
        .toolbar {
            ToolbarItem(placement: .confirmationAction) {
                LoadingButton {
                    await viewModel.onConfirm?(Array(viewModel.configurations.values))
                } label: {
                    Text(KDriveLocalizable.buttonValidate)
                }
            }

            ToolbarItem(placement: .cancellationAction) {
                Button(KDriveLocalizable.buttonCancel, role: .cancel) {
                    viewModel.onCancel?()
                }
            }
        }
    }
}

#Preview {
    let viewModel = SynchroConfigurationFlowViewModel(onConfirm: nil, onCancel: nil)
    viewModel.setupInitialConfigurations([
        SynchroConfiguration(drive: PreviewHelper.drive1, blackList: []),
        SynchroConfiguration(drive: PreviewHelper.drive2, blackList: ["hey"])
    ])

    return SynchroConfigurationPickerView()
        .environmentObject(viewModel)
}
