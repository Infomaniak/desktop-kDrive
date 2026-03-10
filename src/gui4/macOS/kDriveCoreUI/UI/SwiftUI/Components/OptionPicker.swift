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
import SwiftUI

public struct OptionPicker<Option: PreferenceOption>: View {
    let label: String
    let options: [Option]
    let selection: Binding<Option>

    public init(_ label: String, options: [Option], selection: Binding<Option>) {
        self.label = label
        self.options = options
        self.selection = selection
    }

    public var body: some View {
        Picker(label, selection: selection) {
            ForEach(options) { option in
                Text(option.label)
                    .tag(option)
            }
        }
    }
}

@available(macOS 14.0, *)
#Preview {
    @Previewable @State var selection = UINotificationState.always
    OptionPicker("My Picker", options: UINotificationState.allCases, selection: $selection)
}
