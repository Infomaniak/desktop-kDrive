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

struct SynchroFolderSelectionSection: View {
    @EnvironmentObject private var viewModel: SynchroConfigurationFlowViewModel

    @State private var synchroLocation: URL?
    @State private var isDefaultLocation = false
    @State private var isLoading = true

    @State private var isShowingFileImporter = false
    @State private var isShowingSynchroLocationError = false

    @State private var isShowingGenericError = false

    let configuration: SynchroConfiguration

    private var folderTypeLabel: String {
        if isDefaultLocation {
            return KDriveLocalizable.syncFolderDefaultLocation
        } else {
            return KDriveLocalizable.syncFolderCustomLocation
        }
    }

    private var driveLocationTipColor: Color {
        if isShowingSynchroLocationError {
            return ColorToken.Status.Medium.warning.asColor
        } else {
            return ColorToken.Text.tertiary.asColor
        }
    }

    private var synchroLocationPath: String {
        if isLoading {
            return "is/Loading/Path"
        } else {
            return synchroLocation?.path ?? ""
        }
    }

    var body: some View {
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
                    SynchroFolderTypeLabel(text: folderTypeLabel)

                    Text(synchroLocationPath)
                        .truncationMode(.middle)
                        .font(.Tokens.subheadline)
                        .foregroundStyle(ColorToken.Text.tertiary.asColor)
                        .lineLimit(1)
                        .help(synchroLocation?.path ?? "")
                }
                .redacted(reason: isLoading ? [.placeholder] : [])

                HStack {
                    Button(KDriveLocalizable.buttonChangeFolder) {
                        isShowingFileImporter = true
                    }
                    .buttonStyle(.borderedProminent)
                    .fileImporter(
                        isPresented: $isShowingFileImporter,
                        allowedContentTypes: [.directory],
                        onCompletion: handleSelectedDirectory
                    )

                    if !configuration.localFolder.isDefault {
                        Button(KDriveLocalizable.buttonReturnToDefaultFolder) {
                            Task {
                                await setDefaultFolder()
                            }
                        }
                        .buttonStyle(.bordered)
                    }
                }

                Text(KDriveLocalizable.onboardingAdvancedSettingsDriveCustomizeLocationTip)
                    .font(.Tokens.subheadline)
                    .foregroundStyle(driveLocationTipColor)
            }
        }
        .onAppear {
            synchroLocation = configuration.localFolder.url
            isDefaultLocation = configuration.localFolder.isDefault

            if synchroLocation != nil {
                isLoading = false
            }
        }
        .task {
            await setDefaultFolderIfNecessary()
        }
        .genericErrorAlert(isPresented: $isShowingGenericError)
    }

    private func setDefaultFolderIfNecessary() async {
        guard synchroLocation == nil else {
            return
        }

        isLoading = true
        await setDefaultFolder()
        isLoading = false
    }

    private func setDefaultFolder() async {
        guard let localPath = try? await SyncCreationService().preferredLocalPath(for: configuration.drive.name) else {
            return
        }

        synchroLocation = localPath
        isDefaultLocation = true

        viewModel.updateConfiguration(configuration.id, localFolder: .init(url: localPath, isDefault: true))
    }

    private func handleSelectedDirectory(_ result: Result<URL, Error>) {
        Task {
            isLoading = true
            defer { isLoading = false }

            guard case .success(let selectedURL) = result else {
                isShowingGenericError = true
                return
            }

            let isPathValid = try? await UtilityJobs().isPathValidFor(path: selectedURL.path)
            guard isPathValid == true else {
                isShowingSynchroLocationError = true
                return
            }

            synchroLocation = selectedURL
            isDefaultLocation = false
            isShowingSynchroLocationError = false

            viewModel.updateConfiguration(configuration.id, localFolder: .init(url: selectedURL, isDefault: false))
        }
    }
}

#Preview {
    SynchroFolderSelectionSection(configuration: SynchroConfiguration(drive: PreviewHelper.drive1, blackList: []))
}
