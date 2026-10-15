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

import kDriveCoreUI
import SwiftUI

protocol StatusIndicator: Sendable, Equatable {
    var icon: Image { get }
    var hint: String { get }
    var color: Color { get }
}

public struct CircularProgressViewStyle: ProgressViewStyle {
    private static let size: CGFloat = 12
    private static let lineWidth: CGFloat = 2

    public init() {}

    public func makeBody(configuration: Configuration) -> some View {
        ZStack {
            Circle()
                .stroke(ColorToken.Surface.quaternary.asColor, lineWidth: Self.lineWidth)

            Circle()
                .trim(from: 0, to: configuration.fractionCompleted ?? 0)
                .stroke(
                    ColorToken.Action.primary.asColor,
                    style: StrokeStyle(lineWidth: Self.lineWidth, lineCap: .round)
                )
                .rotationEffect(.degrees(-90))
        }
        .padding(Self.lineWidth / 2)
        .frame(width: Self.size, height: Self.size)
        .animation(.easeInOut, value: configuration.fractionCompleted)
    }
}

struct ProgressIndicatorView: View {
    let progress: Int

    var body: some View {
        ProgressView(value: Double(progress), total: 100)
            .progressViewStyle(CircularProgressViewStyle())
            .controlSize(.mini)
            .help("\(progress)%")
    }
}

struct StatusIndicatorView: View {
    let indicator: any StatusIndicator

    var body: some View {
        indicator.icon
            .resizable(at: AppIconSize.iconSize12)
            .foregroundStyle(indicator.color)
            .help(indicator.hint)
            .accessibilityLabel(indicator.hint)
    }
}

#Preview {
    StatusIndicatorView(indicator: DirectionIndicator.up)
        .padding()
}
