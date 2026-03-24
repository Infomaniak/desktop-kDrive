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
import SwiftUI

public extension View {
    func genericErrorAlert(isPresented: Binding<Bool>) -> some View {
        modifier(GenericErrorAlertViewModifier(isPresented: isPresented))
    }
}

struct GenericErrorAlertViewModifier: ViewModifier {
    @Binding var isPresented: Bool

    func body(content: Content) -> some View {
        content
            .alert(KDriveLocalizable.unexpectedErrorTeachingTipTitle, isPresented: $isPresented) {} message: {
                Text(KDriveLocalizable.unexpectedErrorTeachingTipContent)
            }
    }
}
