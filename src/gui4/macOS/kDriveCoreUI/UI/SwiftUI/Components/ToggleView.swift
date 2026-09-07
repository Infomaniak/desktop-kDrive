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

import SwiftUI

public struct ToggleView: View {
    let title: String
    let description: String
    let helperText: String?

    @State private var isShowingAlert = false

    @Binding var isOn: Bool

    public init(title: String, description: String, helperText: String?, isOn: Binding<Bool>) {
        self.title = title
        self.description = description
        self.helperText = helperText
        _isOn = isOn
    }

    public var body: some View {
        HStack(alignment: .center, spacing: AppPadding.padding8) {
            LabelContainerView(title: title, description: description)

            Toggle(title, isOn: $isOn)
                .labelsHidden()

            if let helperText {
                InformationButton {
                    isShowingAlert = true
                }
                .alert(helperText, isPresented: $isShowingAlert) {}
            }
        }
    }
}
