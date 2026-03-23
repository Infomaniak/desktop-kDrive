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
import UniformTypeIdentifiers

struct SynchroFolderSelectionSection: View {
    @State private var isShowingFileImporter = false

    @Binding var synchroLocation: URL?

    let isShowingSynchroLocationError: Bool
    let handleSelectedDirectory: (Result<URL, any Error>) -> Void

    private var driveLocationTipColor: Color {
        if isShowingSynchroLocationError {
            return ColorToken.Status.Medium.warning.asColor
        } else {
            return ColorToken.Text.tertiary.asColor
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
                    Button(KDriveLocalizable.buttonChangeFolder) {
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
    }
}

#Preview {
    SynchroFolderSelectionSection(synchroLocation: .constant(nil), isShowingSynchroLocationError: false) { _ in }
}
