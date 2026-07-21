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

import kDriveCore
import kDriveCoreUI
import kDriveResources
import SwiftUI

struct InstructionView: View {
    let index: Int
    let label: AttributedString

    var body: some View {
        HStack(spacing: AppPadding.padding8) {
            Text("\(index)")
                .font(.Tokens.subheadline)
                .padding(6)
                .foregroundStyle(.white)
                .background(ColorToken.Accent.secondary.asColor, in: .circle)

            Text(label)
                .font(.Tokens.body)
                .foregroundStyle(ColorToken.Text.secondary.asColor)
        }
        .padding(.vertical, AppPadding.padding8)
    }
}

struct EnableBackgroundActivityView: View {
    static let instructions: [MacOSPermission.Instruction] = [
        .openSystemSettings, .openLoginItems, .enableBackgroundActivity
    ]

    var body: some View {
        GeometryReader { proxy in
            HStack {
                VStack(alignment: .leading, spacing: AppPadding.padding24) {
                    VStack(alignment: .leading, spacing: AppPadding.padding8) {
                        Text(KDriveLocalizable.enableBackgroundActivityTitle)
                            .font(.Tokens.largeTitleEmphasized)
                            .foregroundStyle(ColorToken.Text.primary.asColor)
                        Text(KDriveLocalizable.enableBackgroundActivityDescription)
                            .font(.Tokens.body)
                            .foregroundStyle(ColorToken.Text.secondary.asColor)
                    }

                    VStack(alignment: .leading, spacing: 0) {
                        ForEach(0..<Self.instructions.count, id: \.self) { index in
                            InstructionView(
                                index: index + 1,
                                label: AttributedString(Self.instructions[index].attributedString)
                            )

                            if index < Self.instructions.count - 1 {
                                Divider()
                                    .tint(ColorToken.Surface.tertiary.asColor.opacity(0.2))
                                    .frame(height: 2)
                            }
                        }
                    }
                }
                .padding(AppPadding.page)
                .frame(maxHeight: .infinity, alignment: .center)
                .frame(width: proxy.size.width * 0.66)

                VStack {
                    ThemedLottieView(animation: .backgroundActivity)
                }
                .padding()
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .center)
                .background(ColorToken.Surface.secondary.asColor)
            }
        }
    }
}

#Preview {
    EnableBackgroundActivityView()
}
