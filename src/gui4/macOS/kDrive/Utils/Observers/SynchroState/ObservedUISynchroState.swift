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
import InfomaniakDI
import kDriveCoreUI
import SwiftUI

@propertyWrapper
struct ObservedUISynchroState: DynamicProperty {
    @StateObject private var model = UISynchroStateModel()

    var wrappedValue: UISynchroState { model.state }

    var projectedValue: ObservedUISynchroState { self }
}

private final class UISynchroStateModel: ObservableObject {
    @InjectService private var synchroStateObserver: UISynchroStateObserving

    @Published private(set) var state = UISynchroState(errorCount: 0, status: .idle)

    private var cancellable: AnyCancellable?

    init() {
        state = synchroStateObserver.synchroState

        cancellable = synchroStateObserver.synchroStatePublisher
            .receive(on: RunLoop.main)
            .sink { [weak self] output in
                withAnimation {
                    self?.state = output
                }
            }
    }
}
