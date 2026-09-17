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

import Combine
import InfomaniakDI
import kDriveCore
import kDriveCoreUI
import kDriveResources
import OrderedCollections
import SwiftUI

struct AdvancedSynchroView: View {
    @InjectService private var cacheObservable: CoherentCacheObservable

    let drive: UIDrive

    @State private var isLoading = true

    @State private var advancedSynchros = [UISynchro]()
    @State private var userDbId: Int?
    @State private var driveId: Int?

    @State private var synchroToDelete: UISynchro?
    @State private var isShowingAddSynchroSheet = false
    @State private var isShowingGenericError = false

    private var drivePublisher: AnyPublisher<Drive?, Never> {
        cacheObservable.usersPublisher.drivePublisher(driveDbId: Int32(drive.dbId))
    }

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

            if isLoading {
                AdvancedSynchroCellView(synchro: PreviewHelper.synchro, userDbId: nil, driveId: nil) {}
                    .redacted(reason: .placeholder)
            } else {
                ForEach(advancedSynchros) { synchro in
                    AdvancedSynchroCellView(synchro: synchro, userDbId: userDbId, driveId: driveId) {
                        synchroToDelete = synchro
                    }
                }
            }

            Section {} header: {
                Button(KDriveLocalizable.buttonAddAdvancedSync) {
                    @InjectService var matomo: MatomoUtils
                    matomo.track(eventWithCategory: .driveAdvancedSyncsPage, name: "create")

                    isShowingAddSynchroSheet = true
                }
                .buttonStyle(.borderedProminent)
            }
        }
        .groupedFormatStyle()
        .task {
            withAnimation { isLoading = true }
            await fetchSynchros()
            withAnimation { isLoading = false }
        }
        .onReceive(drivePublisher.receive(on: RunLoop.main), perform: updateSynchros)
        .sheet(isPresented: $isShowingAddSynchroSheet) {
            AddAdvancedSynchroFlowView(drive: drive, completion: handleSynchroIsAdded)
        }
        .sheet(item: $synchroToDelete) { synchro in
            RemoveSynchroConfirmationView(synchroDbId: synchro.dbId, completion: handleSynchroIsDeleted)
        }
        .genericErrorAlert(isPresented: $isShowingGenericError)
    }

    private func fetchSynchros() async {
        @InjectService var coherentCache: CoherentCache
        let cachedDrive = await coherentCache.getDrive(driveDbId: Int32(drive.dbId))
        updateSynchros(cachedDrive)
    }

    private func updateSynchros(_ cachedDrive: Drive?) {
        userDbId = cachedDrive.map { Int($0.userDbId) }
        driveId = cachedDrive.map { Int($0.driveId) }

        let freshAdvancedSynchros = cachedDrive?.synchros.values
            .map { UISynchro(synchro: $0) }
            .filter { $0.targetNodeId != nil } ?? []

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
    }
}

#Preview {
    AdvancedSynchroView(drive: PreviewHelper.drive1)
}
