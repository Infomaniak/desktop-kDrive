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

import kDriveCoreUI
import kDriveResources
import SwiftUI

struct AdvancedPreferencesNetworkDetailView: View {
    @Binding var proxyType: UIProxyType?
    @Binding var hostName: String
    @Binding var port: Int
    @Binding var authType: UIProxyAuthType
    @Binding var username: String
    @Binding var password: String
    @Binding var authRequired: Bool

    let numberFormatter = NumberFormatter()

    var body: some View {
        Picker(KDriveLocalizable.proxyType, selection: $proxyType) {
            Text("HTTP(S)").tag(UIProxyType.http)
        }
        .disabled(true)

        TextField(KDriveLocalizable.proxyHost, text: $hostName)
            .textFieldStyle(.roundedBorder)

        TextField(KDriveLocalizable.proxyPort, value: $port, formatter: numberFormatter)
            .textFieldStyle(.roundedBorder)

        Toggle(KDriveLocalizable.proxyNeedAuth, isOn: $authRequired)

        if authRequired {
            TextField(KDriveLocalizable.proxyUser, text: $username)
                .textFieldStyle(.roundedBorder)
                .frame(alignment: .leading)

            SecureField(KDriveLocalizable.proxyPassword, text: $password)
                .textFieldStyle(.roundedBorder)
                .autocorrectionDisabled(true)
                .onAppear(perform: getAuthRequired)
        }
    }

    func getAuthRequired() {
        authRequired = authType != .noAuth
        if authRequired {
            if case UIProxyAuthType.needsAuth(user: let user, password: let password) = authType {
                username = user
                self.password = password
            }
        }
    }
}

#Preview {
    AdvancedPreferencesNetworkDetailView(
        proxyType: .constant(.http),
        hostName: .constant(""),
        port: .constant(0),
        authType: .constant(.noAuth),
        username: .constant(""),
        password: .constant(""),
        authRequired: .constant(false),
    )
}
