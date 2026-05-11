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

struct AdvancedPreferencesNetworkView: View {
    @State private var proxyType: UIProxyType? = .system
    @State private var hostName = ""
    @State private var port = 0
    @State private var authType: UIProxyAuthType = .noAuth
    @State private var isLoading = false
    @State private var username = ""
    @State private var password = ""
    @State private var authRequired = false
    @State private var proxyConfiguration = UIProxyConfiguration(type: .none, hostName: "", port: 0, authType: .noAuth)

    let repository: PreferencesRepository

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
                        authRequired: $authRequired,
                        repository: repository
                    )

                    LoadingButton(isLoading: $isLoading, action: saveChanges) {
                        Text(KDriveLocalizable.buttonSave)
                    }
                    .buttonStyle(.borderedProminent)
                    .frame(maxWidth: .infinity, alignment: .trailing)
                }
            }
        }
        .groupedFormatStyle()
        .onAppear(perform: getValues)
        .onChange(of: proxyType) { newValue in
            updateProxyType(newValue: newValue)
        }
    }

    func getValues() {
        let proxyConfiguration = repository.parametersInfo.proxyConfiguration
        if let type = proxyConfiguration.type {
            proxyType = type
        }
        hostName = proxyConfiguration.hostName
        port = proxyConfiguration.port
        authType = proxyConfiguration.authType

        switch proxyConfiguration.authType {
        case .needsAuth(let user, let password):
            authRequired = true
            username = user
            self.password = password
        case .noAuth:
            authRequired = false
            username = ""
            password = ""
        }
    }

    func updateProxyType(newValue: UIProxyType?) {
        if newValue != .http {
            hostName = ""
            port = 0
            authType = .noAuth
            authRequired = false
            username = ""
            password = ""

            proxyConfiguration = UIProxyConfiguration(type: proxyType, hostName: hostName, port: port, authType: authType)
            updateValue(\.$proxyConfiguration, \.proxyConfiguration, newValue: proxyConfiguration)
        }
    }

    func saveChanges() {
        isLoading.toggle()
        let authType: UIProxyAuthType = authRequired ? .needsAuth(user: username, password: password) : .noAuth
        proxyConfiguration = UIProxyConfiguration(type: proxyType, hostName: hostName, port: port, authType: authType)
        updateValue(\.$proxyConfiguration, \.proxyConfiguration, newValue: proxyConfiguration)
        isLoading = false
    }

    private func updateValue<T: Equatable>(
        _ stateKeyPath: KeyPath<Self, Binding<T>>,
        _ repositoryKeyPath: WritableKeyPath<UIParametersInfo, T>,
        newValue: T
    ) {
        Task {
            guard newValue != repository.parametersInfo[keyPath: repositoryKeyPath] else { return }

            do {
                try await repository.update(repositoryKeyPath, value: newValue)
            } catch {
                self[keyPath: stateKeyPath].wrappedValue = repository.parametersInfo[keyPath: repositoryKeyPath]
            }
        }
    }
}

#Preview {
    AdvancedPreferencesNetworkView(repository: PreferencesRepository())
}
