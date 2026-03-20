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

struct SynchroConfigurationPickerView: View {
    @EnvironmentObject private var viewModel: SynchroConfigurationFlowViewModel

    var body: some View {
        Form {
            Section {} header: {
                VStack(alignment: .leading, spacing: AppPadding.padding12) {
                    Text("!Configurer vos kDrive")
                    Text("!Pour chaque kDrive, vous pourrez choisir où il est synchronisé sur votre ordinateur et quels dossiers synchroniser.")
                }
            }

            ForEach(Array(viewModel.configurations.values)) { configuration in
                Section {
                    HStack {
                        HStack(alignment: .top) {
                            BadgeView(
                                image: KDriveResources.kdriveFoldersStacked.swiftUIImage,
                                color: configuration.drive.color ?? ColorToken.Drive.defaultColor.asColor
                            )

                            VStack(alignment: .leading) {
                                HStack {
                                    Text("!Emplacement:")
                                    Text("")
                                }

                                HStack {
                                    Text("!Contenu synchronisé:")
                                    Text("")
                                }
                            }
                            .frame(maxWidth: .infinity, alignment: .leading)

                            Button("!Configurer") {
                                viewModel.navigate(to: .configureSynchro(configuration))
                            }
                        }
                    }
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
        SynchroConfiguration(drive: PreviewHelper.drive1, blackList: [])
    ])

    return SynchroConfigurationPickerView()
        .environmentObject(viewModel)
}
