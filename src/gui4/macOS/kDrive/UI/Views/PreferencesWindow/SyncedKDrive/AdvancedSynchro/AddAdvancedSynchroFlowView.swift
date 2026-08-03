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

import Combine
import kDriveCoreUI
import SwiftUI

@MainActor
final class AddAdvancedSynchroFlowViewModel: ObservableObject {
    enum State: Equatable, Sendable {
        case main
        case selectRemoteFolder
    }

    @Published private(set) var state = State.main

    @Published var localFolder: URL?
    @Published var selectedRemoteFolder: FileTreeItem?

    var isAdvancedSynchroValid: Bool {
        return localFolder != nil && selectedRemoteFolder != nil
    }

    func navigate(to state: State) {
        self.state = state
    }
}

struct AddAdvancedSynchroFlowView: View {
    @StateObject private var viewModel = AddAdvancedSynchroFlowViewModel()

    let drive: UIDrive
    let completion: () -> Void

    var body: some View {
        ZStack {
            switch viewModel.state {
            case .main:
                AddAdvancedSynchroView(drive: drive, completion: completion)
            case .selectRemoteFolder:
                SelectRemoteFolderView(drive: drive)
            }
        }
        .environmentObject(viewModel)
    }
}

#Preview {
    AddAdvancedSynchroFlowView(drive: PreviewHelper.drive1) {}
}
