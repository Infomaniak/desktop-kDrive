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

import kDriveCore
import kDriveCoreUI
import kDriveResources
import SwiftUI
import UniformTypeIdentifiers

struct SynchroConfigurationView: View {
    @EnvironmentObject private var viewModel: SynchroConfigurationFlowViewModel

    @State private var synchroLocation: URL?
    @State private var isShowingSynchroLocationError = false

    @State private var isShowingFileImporter = false

    let configuration: SynchroConfiguration

    private var driveLocationTipColor: Color {
        return isShowingSynchroLocationError ? ColorToken.Status.Medium.warning.asColor : ColorToken.Text.tertiary.asColor
    }

    private var exclusionFoldersTip: String {
        if configuration.blackList.isEmpty {
            return KDriveLocalizable.onboardingExclusionSummaryNone
        } else {
            return KDriveLocalizable.onboardingExclusionSummarySome
        }
    }

    var body: some View {
        Form {
            Section {
                HStack {
                    BadgeView(
                        image: KDriveResources.kdriveFoldersStacked.swiftUIImage,
                        color: configuration.drive.color ?? ColorToken.Drive.defaultColor.asColor
                    )

                    Text(configuration.drive.name)
                        .font(.Tokens.bodyEmphasized)
                        .foregroundStyle(ColorToken.Text.secondary.asColor)
                }
            } header: {
                Text(KDriveLocalizable.onBoardingAdvancedSettingsDriveTitle)
                    .font(.Tokens.headline)
                    .foregroundStyle(ColorToken.Text.primary.asColor)
            }

            Section {
                VStack(alignment: .leading, spacing: AppPadding.padding16) {
                    VStack(alignment: .leading, spacing: AppPadding.padding8) {
                        Text(KDriveLocalizable.labelSyncLocation)
                            .font(.Tokens.headline)
                        Text(KDriveLocalizable.onboardingAdvancedSettingsDriveCustomizeLocation)
                            .font(.Tokens.body)
                    }
                    .foregroundStyle(ColorToken.Text.primary.asColor)

                    HStack {
                        Button(KDriveLocalizable.buttonSelectFolder) {
                            isShowingFileImporter = true
                        }
                        .buttonStyle(.borderedProminent)
                        .fileImporter(
                            isPresented: $isShowingFileImporter,
                            allowedContentTypes: [.directory],
                            onCompletion: handleSelectedDirectory
                        )

                        if let synchroLocation {
                            Text(synchroLocation.path)
                                .font(.Tokens.subheadline)
                                .foregroundStyle(ColorToken.Text.tertiary.asColor)
                        }
                    }

                    Text(KDriveLocalizable.onboardingAdvancedSettingsDriveCustomizeLocationTip)
                        .font(.Tokens.subheadline)
                        .foregroundStyle(driveLocationTipColor)
                }
            }

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

                        Text(exclusionFoldersTip)
                            .font(.Tokens.subheadline)
                            .foregroundStyle(driveLocationTipColor)
                    }

                    Text(KDriveLocalizable.onboardingAdvancedSettingsDriveExclusionTip)
                        .font(.Tokens.subheadline)
                        .foregroundStyle(ColorToken.Text.tertiary.asColor)
                }
            }
        }
        .groupedFormatStyle()
        .toolbar {
            ToolbarItem(placement: .confirmationAction) {
                LoadingButton(action: viewModel.saveConfiguration) {
                    Text(KDriveLocalizable.buttonValidate)
                }
            }

            ToolbarItem(placement: .cancellationAction) {
                Button(KDriveLocalizable.buttonCancel, role: .cancel) {
                    viewModel.cancelConfiguration()
                }
            }
        }
        .onAppear {
            synchroLocation = configuration.localFolder
        }
        .task {
            guard synchroLocation == nil else { return }

            guard let localPath = try? await SyncCreationService().preferredLocalPath(for: configuration.drive.name) else {
                return
            }

            synchroLocation = localPath
            viewModel.updateConfiguration(configuration.id, localFolder: localPath)
        }
    }

    private func handleSelectedDirectory(_ result: Result<URL, Error>) {
        isShowingSynchroLocationError = false

        guard case .success(let selectedURL) = result else {
            return
        }

        let oldValue = synchroLocation
        synchroLocation = selectedURL

        Task {
            let isPathValid = try? await UtilityJobs().isPathValidFor(path: selectedURL.path)
            if isPathValid == false {
                synchroLocation = oldValue
                isShowingSynchroLocationError = true
            }
        }
    }
}

#Preview {
    SynchroConfigurationView(configuration: SynchroConfiguration(drive: PreviewHelper.drive1, localFolder: nil, blackList: []))
        .environmentObject(SynchroConfigurationFlowViewModel(onConfirm: nil, onCancel: nil))
}
