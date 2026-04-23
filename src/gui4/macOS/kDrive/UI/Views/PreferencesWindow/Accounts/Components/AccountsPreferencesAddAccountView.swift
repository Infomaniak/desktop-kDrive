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

import kDriveResources
import kDriveCoreUI
import SwiftUI

struct AccountsPreferencesAddAccountView: View {
    var body: some View {
        Section {
            HStack {
                KDriveResources.person.swiftUIImage
                    .resizable()
                    .scaledToFit()
                    .frame(width: 16)
                    .foregroundStyle(.secondary)
                    .overlay(
                        Circle().stroke(.tertiary, lineWidth: 1)
                            .frame(width: 24, height: 24)
                    )
                    .padding(EdgeInsets(top: 0, leading: AppPadding.padding4, bottom: 0, trailing: 0))
                Text(KDriveLocalizable.noAccountConnected)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding(EdgeInsets(top: 0, leading: AppPadding.padding8, bottom: 0, trailing: 0))
                Button(KDriveLocalizable.buttonConnectAccount) {
                    // TODO: Add new user
                }
                .buttonStyle(.bordered)
                .frame(maxWidth: .infinity, alignment: .trailing)
            }
        }
    }
}

#Preview {
    AccountsPreferencesAddAccountView()
}

