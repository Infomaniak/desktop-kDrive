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

import InfomaniakDI
import kDriveCore
import kDriveCoreUI
import kDriveResources
import OrderedCollections
import SwiftUI

struct AdvancedSynchroView: View {
    let drive: UIDrive

    @State private var advancedSynchros = [UISynchro]()
    @State private var userDbId: Int?
    @State private var driveId: Int?

    @State private var synchroToDelete: UISynchro?
    @State private var isShowingAddSynchroSheet = false
    @State private var isShowingGenericError = false

    var body: some View {
        Form {
            Section {} header: {
                VStack(alignment: .leading, spacing: AppPadding.padding4) {
                    Text(KDriveLocalizable.addAdvancedSyncDialogTitle)
                        .font(.Tokens.bodyEmphasized)
                        .foregroundStyle(ColorToken.Text.primary.asColor)
                    Text(KDriveLocalizable.advancedSyncDescription)
                        .font(.Tokens.subheadline)
                        .foregroundStyle(ColorToken.Text.tertiary.asColor)
                }
            }

            ForEach(advancedSynchros) { synchro in
                AdvancedSynchroCellView(synchro: synchro, userDbId: userDbId, driveId: driveId) {
                    synchroToDelete = synchro
                }
            }

            Section {} header: {
                Button(KDriveLocalizable.buttonAddAdvancedSync) {
                    isShowingAddSynchroSheet = true
                }
                .buttonStyle(.borderedProminent)
            }
        }
        .groupedFormatStyle()
        .task {
            await fetchSynchros()
        }
        .sheet(isPresented: $isShowingAddSynchroSheet) {
            AddAdvancedSynchroView(drive: drive, completion: handleSynchroIsAdded)
        }
        .sheet(item: $synchroToDelete) { synchro in
            RemoveSynchroConfirmationView(synchroDbId: synchro.dbId, completion: handleSynchroIsDeleted)
        }
        .genericErrorAlert(isPresented: $isShowingGenericError)
    }

    private func fetchSynchros() async {
        @InjectService var coherentCache: CoherentCache
        guard let cachedDrive = await coherentCache.getDrive(driveDbId: Int32(drive.dbId)) else {
            return
        }

        userDbId = Int(cachedDrive.userDbId)
        driveId = Int(cachedDrive.driveId)

        let freshAdvancedSynchros = cachedDrive.synchros.values
            .map { UISynchro(synchro: $0) }
            .filter { $0.targetNodeId != nil }

        withAnimation {
            advancedSynchros = freshAdvancedSynchros
        }
    }

    private func handleSynchroIsAdded() {
        Task {
            _ = try? await SyncJobs().availableSync()
            await fetchSynchros()
        }
    }

    private func handleSynchroIsDeleted(_ error: Error?) {
        guard error == nil else {
            isShowingGenericError = true
            return
        }

        Task {
            await fetchSynchros()
        }
    }
}

#Preview {
    AdvancedSynchroView(drive: PreviewHelper.drive1)
}
