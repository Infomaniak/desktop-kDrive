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

public struct UISynchroModeCell: View {
    let mode: UISynchroMode

    public init(mode: UISynchroMode) {
        self.mode = mode
    }

    public var body: some View {
        HStack(alignment: .top) {
            BadgeView(image: mode.icon, color: ColorToken.Action.primary.asColor)

            VStack(alignment: .leading) {
                Text(mode.title)
                Text(mode.description)
                    .foregroundStyle(.secondary)
                    .font(.callout)
            }
        }
    }
}

#Preview {
    UISynchroModeCell(mode: .availableOffline)
}
