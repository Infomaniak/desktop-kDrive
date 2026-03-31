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
import Foundation
import kDriveCoreUI
import OrderedCollections

enum SynchroConfigurationFlowState {
    case driveSelector
    case configureSynchro(SynchroConfiguration)
    case selectFolders(SynchroConfiguration)
}

struct SynchroConfiguration: Sendable, Identifiable {
    typealias ID = Int

    struct LocalFolder: Sendable {
        var url: URL?
        var isDefault = true
    }

    var id: ID {
        drive.id
    }

    let drive: any UIDriveRepresentation

    var localFolder: LocalFolder
    var blackList: [String]

    init(drive: any UIDriveRepresentation, localFolder: LocalFolder = LocalFolder(), blackList: [String]) {
        self.drive = drive
        self.localFolder = localFolder
        self.blackList = blackList
    }
}

@MainActor
final class SynchroConfigurationFlowViewModel: ObservableObject {
    @Published private(set) var state = SynchroConfigurationFlowState.driveSelector
    @Published private(set) var configurations = OrderedDictionary<SynchroConfiguration.ID, SynchroConfiguration>()

    let onConfirm: (([SynchroConfiguration]) async -> Void)?
    let onCancel: (() -> Void)?

    init(onConfirm: (([SynchroConfiguration]) async -> Void)?, onCancel: (() -> Void)?) {
        self.onConfirm = onConfirm
        self.onCancel = onCancel
    }

    func setupInitialConfigurations(_ newConfigurations: [SynchroConfiguration]) {
        for configuration in newConfigurations {
            configurations[configuration.id] = configuration
        }

        if configurations.count == 1, let singleConfiguration = configurations.values.first {
            state = .configureSynchro(singleConfiguration)
        }
    }

    func updateConfiguration(
        _ id: SynchroConfiguration.ID,
        localFolder: SynchroConfiguration.LocalFolder? = nil,
        blackList: [String]? = nil
    ) {
        guard let configuration = configurations[id] else {
            return
        }

        let updatedConfiguration = SynchroConfiguration(
            drive: configuration.drive,
            localFolder: localFolder ?? configuration.localFolder,
            blackList: blackList ?? configuration.blackList
        )
        configurations[configuration.id] = updatedConfiguration
    }

    func navigate(to state: SynchroConfigurationFlowState) {
        switch state {
        case .driveSelector:
            self.state = .driveSelector
        case .configureSynchro(let synchroConfiguration):
            guard let freshSynchro = configurations[synchroConfiguration.id] else { return }
            self.state = .configureSynchro(freshSynchro)
        case .selectFolders(let synchroConfiguration):
            guard let freshSynchro = configurations[synchroConfiguration.id] else { return }
            self.state = .selectFolders(freshSynchro)
        }
    }

    func saveConfiguration() async {
        if configurations.count == 1 {
            await onConfirm?(Array(configurations.values))
        } else {
            navigate(to: .driveSelector)
        }
    }

    func cancelConfiguration() {
        if configurations.count == 1 {
            onCancel?()
        } else {
            navigate(to: .driveSelector)
        }
    }
}
