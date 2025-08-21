/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import SwiftUI

public struct IKBackport<Content: View> {
    public let view: Content
}

public extension View {
    var ikBackport: IKBackport<Self> { .init(view: self) }
}

// MARK: - toolbar(removing:)

public extension IKBackport {
    enum ToolbarDefaultItemKind {
        case sidebarToggle

        @available(macOS 14.0, *)
        var originalType: SwiftUI.ToolbarDefaultItemKind {
            switch self {
            case .sidebarToggle:
                return .sidebarToggle
            }
        }
    }

    @ViewBuilder func toolbar(removing defaultItemKind: IKBackport.ToolbarDefaultItemKind?) -> some View {
        if #available(macOS 14.0, *) {
            view.toolbar(removing: defaultItemKind?.originalType)
        } else {
            view
        }
    }
}

// MARK: - searchable

public extension IKBackport {
    @ViewBuilder func searchable(text: Binding<String>, isPresented: Binding<Bool>, placement: SearchFieldPlacement = .automatic, prompt: Text? = nil) -> some View {
        if #available(macOS 14.0, *) {
            view.searchable(text: text, isPresented: isPresented, placement: placement, prompt: prompt)
        } else {
            view.searchable(text: text, placement: placement, prompt: prompt)
        }
    }
}
