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
import InfomaniakDI
import kDriveCore
import SwiftUI

private struct SynchroConversionObserverModifier: ViewModifier {
    @LazyInjectService private var vfsConversionStoreObservable: VFSConversionStoreObservable

    @Binding var isConverting: Bool

    let synchroDbId: Int?

    private var convertingSynchrosPublisher: AnyPublisher<ConvertingSynchros, Never> {
        vfsConversionStoreObservable.convertingSynchrosPublisher
            .receive(on: RunLoop.main)
            .eraseToAnyPublisher()
    }

    func body(content: Content) -> some View {
        content
            .onReceive(convertingSynchrosPublisher) { convertingSynchros in
                guard let synchroDbId else {
                    isConverting = false
                    return
                }
                isConverting = convertingSynchros.contains(Int32(synchroDbId))
            }
    }
}

public extension View {
    func observingSynchroConversion(synchroDbId: Int?, isConverting: Binding<Bool>) -> some View {
        modifier(SynchroConversionObserverModifier(isConverting: isConverting, synchroDbId: synchroDbId))
    }
}
