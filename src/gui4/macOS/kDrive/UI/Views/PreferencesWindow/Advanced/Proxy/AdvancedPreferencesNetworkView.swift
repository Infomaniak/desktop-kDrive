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

struct AdvancedPreferencesNetworkView: View {
    @State private var proxyType: UIProxyType = .none
    @State private var hostName = ""
    @State private var port = 0
    @State private var authType: UIProxyAuthType = .noAuth
    @State private var isLoadingSaveButton = false
    @State private var username = ""
    @State private var password = ""
    @State private var isAuthenticationRequired = false
    @State private var proxyConfiguration = UIProxyConfiguration(type: UIProxyType.none, hostName: "", port: 0, authType: .noAuth)

    let repository: PreferencesRepository

    private let minPort = 0
    private let maxPort = 65535

    private var isConfigurationInvalid: Bool {
        return hostName.isEmpty || port <= minPort || port > maxPort || ((username.isEmpty || password.isEmpty) &&
            isAuthenticationRequired)
    }

    var body: some View {
        Form {
            Section {
                AdvancedPreferencesNetworkHeaderView(selectedProxyType: $proxyType)

                if proxyType == .http {
                    AdvancedPreferencesNetworkDetailView(
                        proxyType: $proxyType,
                        hostName: $hostName,
                        port: $port,
                        authType: $authType,
                        username: $username,
                        password: $password,
                        isAuthenticationRequired: $isAuthenticationRequired
                    )

                    LoadingButton(isLoading: $isLoadingSaveButton, action: saveHTTPProxyChanges) {
                        Text(KDriveLocalizable.buttonSave)
                    }
                    .buttonStyle(.borderedProminent)
                    .frame(maxWidth: .infinity, alignment: .trailing)
                    .disabled(isConfigurationInvalid)
                }
            }
        }
        .groupedFormatStyle()
        .onAppear(perform: getInitialValues)
        .onChange(of: proxyType) { newValue in
            updateProxyType(newValue: newValue)
        }
    }

    func getInitialValues() {
        let proxyConfiguration = repository.parametersInfo.proxyConfiguration

        if let type = proxyConfiguration.type {
            proxyType = type
        }
        hostName = proxyConfiguration.hostName
        port = proxyConfiguration.port
        authType = proxyConfiguration.authType

        switch proxyConfiguration.authType {
        case .needsAuth(let user, let password):
            isAuthenticationRequired = true
            username = user
            self.password = password
        case .noAuth:
            isAuthenticationRequired = false
            username = ""
            password = ""
        }
    }

    private func updateProxyType(newValue: UIProxyType?) {
        guard newValue != .http else {
            return
        }

        hostName = ""
        port = 0
        authType = .noAuth
        isAuthenticationRequired = false
        username = ""
        password = ""

        proxyConfiguration = UIProxyConfiguration(type: proxyType, hostName: hostName, port: port, authType: authType)
        updateRepositoryValue(
            \.$proxyConfiguration,
            \.proxyConfiguration,
            newValue: proxyConfiguration,
            repository: repository
        )
    }

    private func saveHTTPProxyChanges() {
        isLoadingSaveButton = true
        defer { isLoadingSaveButton = false }

        guard !isConfigurationInvalid else { return }

        let authType: UIProxyAuthType = isAuthenticationRequired ? .needsAuth(user: username, password: password) : .noAuth
        proxyConfiguration = UIProxyConfiguration(type: proxyType, hostName: hostName, port: port, authType: authType)
        updateRepositoryValue(\.$proxyConfiguration, \.proxyConfiguration, newValue: proxyConfiguration, repository: repository)
    }
}

#Preview {
    AdvancedPreferencesNetworkView(repository: PreferencesRepository())
}
