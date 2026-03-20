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

struct ConfigurableSynchroView: View {
    @EnvironmentObject private var viewModel: SynchroConfigurationFlowViewModel

    let configuration: SynchroConfiguration

    private var selectedLocationLabel: AttributedString {
        let folderPath: String
        if let folderURL = configuration.location.localFolder, !configuration.location.isDefaultLocation {
            folderPath = folderURL.path
        } else {
            folderPath = KDriveLocalizable.buttonDefaultLocation
        }

        var label = AttributedString(KDriveLocalizable.onboardingAdvancedSettingsDriveSelectionLocation(folderPath))
        label.font = .Tokens.subheadline
        label.foregroundColor = ColorToken.Text.tertiary.asColor

        if let range = label.range(of: folderPath) {
            label[range].font = .Tokens.subheadlineEmphasized
        }

        return label
    }

    private var synchronizedFoldersLabel: AttributedString {
        let synchronizedFolders: String
        if configuration.blackList.isEmpty {
            synchronizedFolders = KDriveLocalizable.onboardingExclusionSummaryNone
        } else {
            synchronizedFolders = KDriveLocalizable.onboardingExclusionSummarySome
        }

        var label = AttributedString(KDriveLocalizable.onboardingAdvancedSettingsDriveSelectionExclusion(synchronizedFolders))
        label.font = .Tokens.subheadline
        label.foregroundColor = ColorToken.Text.tertiary.asColor

        if let range = label.range(of: synchronizedFolders) {
            label[range].font = .Tokens.subheadlineEmphasized
        }

        return label
    }

    var body: some View {
        HStack {
            HStack(alignment: .top) {
                BadgeView(
                    image: KDriveResources.kdriveFoldersStacked.swiftUIImage,
                    color: configuration.drive.color ?? ColorToken.Drive.defaultColor.asColor
                )

                VStack(alignment: .leading, spacing: AppPadding.padding8) {
                    Text(configuration.drive.name)
                        .font(.Tokens.bodyEmphasized)

                    VStack(alignment: .leading, spacing: AppPadding.padding2) {
                        Text(selectedLocationLabel)
                        Text(synchronizedFoldersLabel)
                    }
                    .font(.Tokens.body)
                    .lineLimit(1)
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                .foregroundStyle(ColorToken.Text.primary.asColor)
            }

            Button(KDriveLocalizable.buttonConfigure) {
                viewModel.navigate(to: .configureSynchro(configuration))
            }
        }
    }
}

#Preview {
    ConfigurableSynchroView(configuration: SynchroConfiguration(drive: PreviewHelper.drive1, blackList: []))
        .padding()
}
