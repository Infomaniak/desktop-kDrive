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

import kDriveCoreUI
import kDriveResources
import SwiftUI

struct BlacklistPreferencesView: View {
    @State private var initialBlacklist: Set<String> = []
    @State private var blackList: Set<String> = []

    let userId: UIUser.ID
    let driveId: UIDrive.ID

    var body: some View {
        VStack {
            FoldersToSynchroView(
                blackList: $blackList,
                initialBlackList: initialBlacklist,
                userDbId: userId,
                driveDbId: driveId
            )

            GroupBox {
                HStack {
                    Button(KDriveLocalizable.buttonCancel, role: .cancel, action: goBack)
                    Button(KDriveLocalizable.buttonSave, action: saveChanges)
                        .buttonStyle(.borderedProminent)
                }
                .frame(maxWidth: .infinity, alignment: .trailing)
                .padding(AppPadding.padding8)
            }
        }
        .padding(AppPadding.page)
    }

    private func saveChanges() {}

    private func goBack() {}
}

#Preview {
    BlacklistPreferencesView(userId: 0, driveId: 0)
}
