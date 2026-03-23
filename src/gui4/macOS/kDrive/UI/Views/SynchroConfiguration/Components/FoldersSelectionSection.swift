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

struct FolderFoldersSelectionSection: View {
    @EnvironmentObject private var viewModel: SynchroConfigurationFlowViewModel

    let configuration: SynchroConfiguration

    private var exclusionFoldersIcon: Image {
        if configuration.blackList.isEmpty {
            return KDriveResources.checkmarkCircle.swiftUIImage
        } else {
            return KDriveResources.pencil.swiftUIImage
        }
    }

    private var exclusionFoldersTip: String {
        if configuration.blackList.isEmpty {
            return KDriveLocalizable.onboardingExclusionSummaryNone
        } else {
            return KDriveLocalizable.onboardingExclusionSummarySome
        }
    }

    var body: some View {
        Section {
            VStack(alignment: .leading, spacing: AppPadding.padding16) {
                VStack(alignment: .leading, spacing: AppPadding.padding8) {
                    Text(KDriveLocalizable.labelSyncFolder)
                        .font(.Tokens.headline)
                    Text(KDriveLocalizable.onboardingAdvancedSettingsDriveExclusionDescription)
                        .font(.Tokens.body)
                }
                .foregroundStyle(ColorToken.Text.primary.asColor)

                HStack {
                    Button(KDriveLocalizable.buttonSelectFolders) {
                        viewModel.navigate(to: .selectFolders(configuration))
                    }
                    .buttonStyle(.borderedProminent)

                    Label {
                        Text(exclusionFoldersTip)
                            .font(.Tokens.subheadline)
                    } icon: {
                        exclusionFoldersIcon
                    }
                    .foregroundStyle(ColorToken.Text.tertiary.asColor)
                }

                Text(KDriveLocalizable.onboardingAdvancedSettingsDriveExclusionTip)
                    .font(.Tokens.subheadline)
                    .foregroundStyle(ColorToken.Text.tertiary.asColor)
            }
        }
    }
}

#Preview {
    FolderFoldersSelectionSection(configuration: SynchroConfiguration(drive: PreviewHelper.drive1, blackList: []))
        .environmentObject(SynchroConfigurationFlowViewModel(onConfirm: nil, onCancel: nil))
}
