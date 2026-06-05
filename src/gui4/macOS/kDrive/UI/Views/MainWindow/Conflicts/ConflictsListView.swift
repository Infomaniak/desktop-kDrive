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

struct ConflictCellView: View {
    let path: String
    let action: () -> Void

    private var fileTypeRepresentation: FileTypeRepresentation {
        let fileExtension = URL(fileURLWithPath: path).pathExtension
        return FileTypeRepresentation(filenameExtension: fileExtension)
    }

    var body: some View {
        HStack {
            HStack(spacing: AppPadding.padding4) {
                FileTypeView(fileTypeRepresentation: fileTypeRepresentation)
                    .frame(size: AppIconSize.iconSize12)

                Text(path)
                    .frame(maxWidth: .infinity, alignment: .leading)
            }

            Button(KDriveLocalizable.conflictErrorAction, action: action)
        }
    }
}

struct ConflictsListView: View {
    @State private var search = ""

    let errors: [SynchroError]

    private var filteredErrors: [SynchroError] {
        guard !search.isEmpty else {
            return errors
        }

        return errors.filter { $0.metadata.path.localizedCaseInsensitiveContains(search) }
    }

    var body: some View {
        Form {
            Section {
                ForEach(filteredErrors) { error in
                    ConflictCellView(path: error.metadata.path) {
                        resolveConflict(error)
                    }
                }
            } header: {
                HStack {
                    TextField(
                        KDriveLocalizable.conflictsSearchBoxPlaceholder,
                        text: $search,
                        prompt: Text(KDriveLocalizable.conflictsSearchBoxPlaceholder)
                    )
                    .labelsHidden()
                    .textFieldStyle(.roundedBorder)
                    .frame(maxWidth: 300)
                    .frame(maxWidth: .infinity, alignment: .leading)

                    Button(KDriveLocalizable.buttonStartConflictsResolution, action: startConflictsResolution)
                        .buttonStyle(.borderedProminent)
                }
            }
        }
        .groupedFormatStyle()
    }

    private func startConflictsResolution() {}

    private func resolveConflict(_ error: SynchroError) {}
}

#Preview {
    ConflictsListView(errors: Array(repeating: PreviewHelper.synchroError, count: 4))
}
