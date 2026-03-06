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

struct DriveContext: Sendable, Identifiable {
    var id: UIDrive.ID {
        return drive.id
    }

    let drive: UIDrive
    let isSynchronized: Bool
}

struct UserSection: View {
    @State private var isShowingDisconnectUserAlert = false

    @State private var isShowingErrorAlert = false
    @State private var error: DomainError?

    let user: UIUser
    let drives: [DriveContext]

    private var orderedDrives: [DriveContext] {
        return drives.sorted {
            if $0.isSynchronized != $1.isSynchronized {
                return $0.isSynchronized && !$1.isSynchronized
            }
            return $0.drive.name < $1.drive.name
        }
    }

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

            ForEach(drives) { context in
                HStack(spacing: AppPadding.padding8) {
                    DriveBadgeView(color: context.drive.color ?? ColorToken.Drive.defaultColor.asColor)

                    Text(context.drive.name)
                        .font(.Tokens.body)
                        .foregroundStyle(ColorToken.Text.primary.asColor)
                        .frame(maxWidth: .infinity, alignment: .leading)

                    if context.isSynchronized {
                        Text(KDriveLocalizable.syncedDrive)
                            .font(.Tokens.body)
                            .foregroundStyle(ColorToken.Text.tertiary.asColor)

                        Button(KDriveLocalizable.buttonManage) {}
                            .buttonStyle(.bordered)
                    } else {
                        Text(KDriveLocalizable.notSyncedDrive)
                            .font(.Tokens.body)
                            .foregroundStyle(ColorToken.Text.tertiary.asColor)

                        Button(KDriveLocalizable.buttonEnable) {}
                            .buttonStyle(.bordered)
                    }
                }
                .padding(.leading, AppPadding.padding24)
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
        drives: [
            DriveContext(drive: PreviewHelper.drive1, isSynchronized: true),
            DriveContext(drive: PreviewHelper.drive2, isSynchronized: false)
        ]
    )
}
