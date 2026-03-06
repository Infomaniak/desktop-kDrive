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

struct UserSection: View {
    @State private var isShowingDisconnectUserAlert = false

    @State private var isShowingErrorAlert = false
    @State private var error: DomainError?

    let user: UIUser

    let synchronizedDrives: [UIDrive]
    let availableDrives: [UIAvailableDrive]

    enum DomainError: LocalizedError {
        case impossibleToDeleteUser

        var errorDescription: String? {
            switch self {
            case .impossibleToDeleteUser:
                return KDriveLocalizable.errorDeletingAccount
            }
        }
    }

    var body: some View {
        Section {
            UserHeaderCellView(avatar: user.avatar, name: user.name, email: user.email)

            ForEach(synchronizedDrives) { drive in
                AccountDriveCellView(drive: drive, isSynchronized: true)
            }

            ForEach(availableDrives) { drive in
                AccountDriveCellView(drive: drive, isSynchronized: false)
            }

            Button(KDriveLocalizable.buttonDisconnectAccount, role: .destructive) {
                isShowingDisconnectUserAlert = true
            }
            .buttonStyle(.borderless)
            .tint(.red)
            .alert(KDriveLocalizable.dialogRemoveAccountTitle, isPresented: $isShowingDisconnectUserAlert) {
                Button(KDriveLocalizable.buttonKeepAccount, role: .cancel) {}
                Button(KDriveLocalizable.buttonLogOut, role: .destructive) {
                    logOutAccount()
                }
            } message: {
                Text(KDriveLocalizable.dialogRemoveAccountContentMac(user.name))
            }
            .alert(isPresented: $isShowingErrorAlert, error: error) {}
        }
    }

    private func logOutAccount() {
        Task {
            do {
                try await UserJobs().userDelete(dbId: Int32(user.dbId))
            } catch {
                self.error = DomainError.impossibleToDeleteUser
                isShowingErrorAlert = true
            }
        }
    }
}

#Preview {
    UserSection(
        user: PreviewHelper.user,
        synchronizedDrives: [PreviewHelper.drive1],
        availableDrives: [PreviewHelper.availableDrive1]
    )
}
