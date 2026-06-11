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
import Sentry
import SwiftUI

struct BlacklistPreferencesView: View {
    @State private var initialBlacklist: Set<String> = []
    @State private var blackList: Set<String> = []

    @State private var isLoadingButton = false
    @State private var isShowingGenericErrorAlert = false

    let userDbId: UIUser.ID
    let driveId: UIDrive.ID
    let synchroDbId: UISynchro.ID

    var body: some View {
        VStack {
            FoldersToSynchroView(
                blackList: $blackList,
                initialBlackList: initialBlacklist,
                userDbId: userDbId,
                driveDbId: driveId
            )

            GroupBox {
                HStack {
                    Button(KDriveLocalizable.buttonCancel, role: .cancel, action: goBack)
                    LoadingButton(isLoading: $isLoadingButton, action: saveChanges) {
                        Text(KDriveLocalizable.buttonSave)
                    }
                    .buttonStyle(.borderedProminent)
                }
                .frame(maxWidth: .infinity, alignment: .trailing)
                .padding(AppPadding.padding8)
            }
        }
        .padding(AppPadding.page)
        .genericErrorAlert(isPresented: $isShowingGenericErrorAlert)
        .task {
            await fetchBlacklistedNodes()
        }
    }

    private func saveChanges() async {
        do {
            try await BlacklistJobs().setBlacklistedNodeList(syncDbId: Int32(synchroDbId), nodeIdList: Array(blackList))
            goBack()
        } catch {
            SentrySDK.capture(error: error)
        }
    }

    private func goBack() {
        @InjectService var router: PreferencesViewRouter
        router.removeLast()
    }

    private func fetchBlacklistedNodes() async {
        do {
            let blacklistedNodes = try await BlacklistJobs().getBlacklistedNodeList(syncDbId: Int32(synchroDbId))
            initialBlacklist = Set(blacklistedNodes)
        } catch {
            SentrySDK.capture(error: error)
        }
    }
}

#Preview {
    BlacklistPreferencesView(userDbId: 0, driveId: 0, synchroDbId: 0)
}
