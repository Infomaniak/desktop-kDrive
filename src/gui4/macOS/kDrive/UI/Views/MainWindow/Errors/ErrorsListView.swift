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
import SwiftUI

struct ErrorsListView: View {
    @StateObject private var synchroErrorManager = SynchroErrorManager()

    let errors: [UISynchroErrorCategory: [SynchroError]]
    let isAdmin: Bool

    static let maximumConflictsToDisplay = 5

    @State private var computedErrors: [UISynchroErrorCategory: [SynchroError]] = [:]
    @State private var categories: [UISynchroErrorCategory] = []

    var body: some View {
        Form {
            ForEach(categories) { category in
                Section {
                    if category == .conflicts {
                        ManyConflictsCellView(errors: computedErrors[category, default: []], manager: synchroErrorManager)
                    } else {
                        ForEach(computedErrors[category, default: []]) { error in
                            ErrorCellFactory().make(error: error, isAdmin: isAdmin, manager: synchroErrorManager)
                        }
                    }
                } header: {
                    Text(category.title)
                }
            }
        }
        .groupedFormatStyle()
        .sheet(item: $synchroErrorManager.isShowingLocalAccessSheet) { error in
            LocalAccessErrorSheet(synchroErrorManager: synchroErrorManager, error: error)
        }
        .sheet(item: $synchroErrorManager.isShowingActivateOfflineSynchroSheet) { error in
            SystemUnableToStartVFSReasonSheet(error: error)
        }
        .sheet(item: $synchroErrorManager.isShowingVersionSelectorSheet) { conflictsToResolve in
            ConflictVersionSelectorView(errors: conflictsToResolve.errors)
        }
        .sheet(item: $synchroErrorManager.isShowingResolutionTipsSheet) { type in
            switch type {
            case .invalidSyncDirAccess(let error):
                InvalidSyncDirAccessReasonsSheet(synchroErrorManager: synchroErrorManager, error: error)
            case .systemSyncDirAccess(let error):
                SystemSyncDirAccessReasonsSheet(synchroErrorManager: synchroErrorManager, error: error)
            case .systemSyncDirDiskMissing:
                SystemSyncDirDiskMissingReasonsSheet()
            case .dataSyncDirChanged:
                DataSyncDirChangedReasonsSheet(synchroErrorManager: synchroErrorManager)
            }
        }
        .onAppear(perform: recomputeErrors)
        .onChange(of: errors) { _ in
            recomputeErrors()
        }
    }

    private func recomputeErrors() {
        let filesErrors = errors[.filesToCheck, default: []]
        guard !filesErrors.isEmpty else {
            computedErrors = errors
            categories = errors.keys.sorted()
            return
        }

        var conflicts: [SynchroError] = []
        var nonConflicts: [SynchroError] = []
        conflicts.reserveCapacity(filesErrors.count)
        nonConflicts.reserveCapacity(filesErrors.count)

        for error in filesErrors {
            if error.kind == .conflict {
                conflicts.append(error)
            } else {
                nonConflicts.append(error)
            }
        }

        var result = errors
        if conflicts.count >= Self.maximumConflictsToDisplay {
            result[.conflicts] = conflicts
            if nonConflicts.isEmpty {
                result.removeValue(forKey: .filesToCheck)
            } else {
                result[.filesToCheck] = nonConflicts
            }
        }

        computedErrors = result
        categories = result.keys.sorted()
    }
}

#Preview {
    ErrorsListView(errors: [:], isAdmin: false)
}
