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

public extension View {
    /// Renders a `Menu` as a compact borderless button on macOS 12.
    ///
    /// On macOS 12, a `Menu` ignores `.buttonStyle(.plain)` and a borderless menu stretches to the available width,
    /// which misplaces icon-only menus. Does nothing on macOS 13 and later.
    @ViewBuilder
    func legacyBorderlessMenu() -> some View {
        if #available(macOS 13.0, *) {
            self
        } else {
            menuStyle(.borderlessButton)
                .menuIndicator(.hidden)
                .fixedSize()
        }
    }
}
