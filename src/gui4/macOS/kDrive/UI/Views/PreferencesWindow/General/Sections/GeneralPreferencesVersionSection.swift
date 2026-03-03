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

import AppKit
import kDriveCoreUI
import kDriveResources
import SwiftUI

enum BetaOption: String, Identifiable, CaseIterable, PreferenceOption {
    var id: String {
        rawValue
    }

    case doNotJoin
    case beta
    case `internal`

    var description: String {
        switch self {
        case .doNotJoin:
            "Off"
        case .beta:
            "Beta"
        case .internal:
            "Internal"
        }
    }

    var label: String {
        switch self {
        case .doNotJoin:
            return "Ne pas rejoindre"
        case .beta:
            return "Version beta publique"
        case .internal:
            return "Version beta interne"
        }
    }

    init(_ distributionChannel: UIDistributionChannel) {
        switch distributionChannel {
        case .prod:
            self = .doNotJoin
        case .next:
            self = .doNotJoin
        case .beta:
            self = .beta
        case .internal:
            self = .internal
        case .legacy:
            self = .doNotJoin
        }
    }
}

struct GeneralPreferencesVersionSection: View {
    @ObservedObject var repository: PreferencesRepository

    @State private var isShowingDistributionChannelSheet = false
    @State private var betaOption = BetaOption.doNotJoin

    var body: some View {
        Section {
            HStack {
                VStack(alignment: .leading) {
                    Text(KDriveLocalizable.updateSettings)
                    Text(KDriveLocalizable.updateAvailable("3.7.6"))
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
                .frame(maxWidth: .infinity, alignment: .leading)

                Button(KDriveLocalizable.buttonUpdate) {
                    // TODO: Update app
                }
            }

            Toggle(KDriveLocalizable.automaticUpdatesSetting, isOn: .constant(true))

            IKLabeledContent(KDriveLocalizable.betaSettings) {
                HStack {
                    Text(betaOption.description)
                        .foregroundStyle(.secondary)

                    InformationButton {
                        isShowingDistributionChannelSheet = true
                    }
                }
            }

            IKLabeledContent(KDriveLocalizable.aboutKDrive) {
                InformationButton {
                    NSApplication.shared.orderFrontStandardAboutPanel(nil)
                }
            }
        }
        .onAppear {
            betaOption = BetaOption(repository.parametersInfo.distributionChannel)
        }
        .onChange(of: repository.parametersInfo) { newValue in
            betaOption = BetaOption(newValue.distributionChannel)
        }
        .sheet(isPresented: $isShowingDistributionChannelSheet) {
            DistributionChannelView(repository: repository)
        }
    }
}

#Preview {
    GeneralPreferencesVersionSection(repository: PreferencesRepository())
}
