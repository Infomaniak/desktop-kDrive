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

import SwiftUI

public struct LoadingButton<Content: View>: View {
    @Binding var isLoading: Bool

    let action: () async -> Void
    let label: Content

    public init(isLoading: Binding<Bool>, action: @escaping () async -> Void, @ViewBuilder label: () -> Content) {
        _isLoading = isLoading
        self.action = action
        self.label = label()
    }

    public var body: some View {
        Button {
            Task {
                isLoading = true
                await action()
                isLoading = false
            }
        } label: {
            ZStack {
                ProgressView()
                    .progressViewStyle(.circular)
                    .controlSize(.small)
                    .opacity(isLoading ? 1 : 0)

                label
                    .opacity(isLoading ? 0 : 1)
            }
        }
        .disabled(isLoading)
    }
}

#Preview {
    LoadingButton(isLoading: .constant(true)) {} label: {
        Text("Hello, World!")
    }
}
