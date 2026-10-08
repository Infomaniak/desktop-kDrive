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

import InfomaniakDI
import kDriveCore
import kDriveCoreUI
import kDriveResources
import SwiftUI

private struct CircleBackground: View {
    let color: Color

    var body: some View {
        if #available(macOS 26.0, *) {
            Circle()
                .fill(color)
                .glassEffect()
        } else {
            Circle()
                .fill(color)
        }
    }
}

struct InstructionView: View {
    let index: Int
    let label: AttributedString

    var body: some View {
        HStack(spacing: AppPadding.padding8) {
            Text("\(index)")
                .font(.Tokens.subheadline)
                .padding(6)
                .foregroundStyle(.white)
                .background {
                    CircleBackground(color: ColorToken.Accent.secondary.asColor)
                }

            Text(label)
                .font(.Tokens.body)
                .foregroundStyle(ColorToken.Text.secondary.asColor)
        }
        .padding(.vertical, AppPadding.padding4)
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
                        ForEach(0 ..< Self.instructions.count, id: \.self) { index in
                            InstructionView(
                                index: index + 1,
                                label: AttributedString(Self.instructions[index].attributedString)
                            )
                        }
                    }

                    Button(KDriveLocalizable.buttonKDriveIsActivated, action: reconnectToLoginAgentAndProceed)
                        .buttonStyle(.borderedProminent)
                }
                .padding(AppPadding.padding48)
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
        .ignoresSafeArea()
        .frame(minWidth: 600, minHeight: 300)
    }

    private func reconnectToLoginAgentAndProceed() {
        // Attempting to (re)connect to the login item agent is the real source of truth for whether
        // background activity is enabled.
        @InjectService var xpcConnectionProvider: XPCConnectionProvider
        Task {
            await xpcConnectionProvider.reconnectToLoginAgent()
        }

        @InjectService var router: MainWindowRouter
        router.navigate(to: .preloading())
    }
}

#Preview {
    EnableBackgroundActivityView()
}
