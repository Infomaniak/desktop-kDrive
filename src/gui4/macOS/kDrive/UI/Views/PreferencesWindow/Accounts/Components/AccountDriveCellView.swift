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

import InfomaniakDI
import kDriveCore
import kDriveCoreUI
import kDriveResources
import SwiftUI

struct AccountDriveCellView: View {
    @State private var isShowingSynchronizeDriveView = false
    @State private var isShowingGenericError = false

    let userDbId: Int
    let drive: any UIDriveRepresentation
    let isSynchronized: Bool

    private var synchroConfiguration: SynchroConfiguration {
        return SynchroConfiguration(drive: drive, localFolder: .init(), blackList: [])
    }

    var body: some View {
        HStack(spacing: AppPadding.padding8) {
            BadgeView(
                image: KDriveResources.kdriveFoldersStacked.swiftUIImage,
                color: drive.color ?? ColorToken.Drive.defaultColor.asColor
            )

            Text(drive.name)
                .font(.Tokens.body)
                .foregroundStyle(ColorToken.Text.primary.asColor)
                .frame(maxWidth: .infinity, alignment: .leading)

            if isSynchronized {
                Text(KDriveLocalizable.syncedDrive)
                    .font(.Tokens.body)
                    .foregroundStyle(ColorToken.Text.tertiary.asColor)

                Button(KDriveLocalizable.buttonManage, action: manageSynchronizedDrive)
                    .buttonStyle(.bordered)
            } else {
                Text(KDriveLocalizable.notSyncedDrive)
                    .font(.Tokens.body)
                    .foregroundStyle(ColorToken.Text.tertiary.asColor)

                Button(KDriveLocalizable.buttonEnable, action: showSynchronizeDriveSheet)
                    .buttonStyle(.bordered)
                    .sheet(isPresented: $isShowingSynchronizeDriveView) {
                        SynchroConfigurationFlowView(
                            userDbId: userDbId,
                            configurations: [synchroConfiguration],
                            onConfirm: handleUpdatedConfigurations,
                            onCancel: hideSynchronizeDriveSheet
                        )
                    }
            }
        }
        .padding(.leading, AppPadding.padding24)
        .genericErrorAlert(isPresented: $isShowingGenericError)
    }

    private func manageSynchronizedDrive() {
        guard let drive = drive as? UIDrive else { return }

        @InjectService var router: PreferencesViewRouter
        router.append(.syncedKDrive(drive))
    }

    private func showSynchronizeDriveSheet() {
        isShowingSynchronizeDriveView = true
    }

    private func hideSynchronizeDriveSheet() {
        isShowingSynchronizeDriveView = false
    }

    private func handleUpdatedConfigurations(_ configurations: [SynchroConfiguration]) async {
        do {
            if let configuration = configurations.first {
                try await synchronizeDrive(from: configuration)
            }
            hideSynchronizeDriveSheet()
        } catch {
            hideSynchronizeDriveSheet()
            isShowingGenericError = true
        }
    }

    private func synchronizeDrive(from configuration: SynchroConfiguration) async throws {
        @InjectService var cache: CoherentCache
        guard let drive = await cache.getAvailableDrive(driveDb: Int32(configuration.drive.id), userDbId: Int32(userDbId)) else {
            return
        }

        let syncCandidate = NewSyncCandidate(
            origin: .availableDrive(drive),
            remoteFolder: .kDriveRoot,
            localFolder: configuration.localFolder.url,
            blackList: configuration.blackList
        )

        let syncInfo = try await SyncCreationService().create(from: syncCandidate)
        try await SyncJobs().startSync(syncDbId: syncInfo.dbId)
    }
}

#Preview {
    AccountDriveCellView(userDbId: PreviewHelper.user.id, drive: PreviewHelper.drive1, isSynchronized: true)
}
