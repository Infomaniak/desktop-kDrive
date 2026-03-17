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

import Combine
import kDriveCoreUI
import SwiftUI

enum SynchroConfigurationFlowState {
    case driveSelector
    case configureSynchro(SynchroConfiguration)
    case selectFolders(SynchroConfiguration)
}

struct SynchroConfiguration: Sendable {
    typealias ID = Int

    var id: ID {
        drive.id
    }

    let drive: any UIDriveRepresentation
    let localFolder: URL?
    let blackList: [String]
}

final class SynchroConfigurationFlowViewModel: ObservableObject {
    @Published var state = SynchroConfigurationFlowState.driveSelector
    @Published var configurations = [SynchroConfiguration.ID: SynchroConfiguration]()

    func setupConfigurations(_ newConfigurations: [SynchroConfiguration]) {
        for configuration in newConfigurations {
            configurations[configuration.id] = configuration
        }

        if configurations.count == 1, let configuration = configurations.values.first {
            state = .configureSynchro(configuration)
        }
    }
}

struct SynchroConfigurationFlowView: View {
    @StateObject private var viewModel = SynchroConfigurationFlowViewModel()

    let configurations: [SynchroConfiguration]

    var body: some View {
        ZStack {
            switch viewModel.state {
            case .driveSelector:
                Text("Drive Selector")
            case .configureSynchro(let synchroConfiguration):
                SynchroConfigurationView(configuration: synchroConfiguration)
            case .selectFolders(let synchroConfiguration):
                Text("Hoy")
            }
        }
        .environmentObject(viewModel)
        .onAppear {
            viewModel.setupConfigurations(configurations)
        }
    }
}

#Preview {
    SynchroConfigurationFlowView(configurations: [])
}
