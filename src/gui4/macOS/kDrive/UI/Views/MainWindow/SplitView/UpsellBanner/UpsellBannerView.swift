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
import kDriveResources
import SwiftUI

struct UpsellBannerView: View {
    let logo: Image
    let text: AttributedString
    let upsellLink: URL

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            logo

            Text(text)
                .foregroundStyle(ColorToken.Text.primary.asColor)

            Button(KDriveLocalizable.buttonUpgradeSubscriptionShort, action: didTapUpgrade)
        }
        .padding(AppPadding.padding16)
        .frame(maxWidth: .infinity, alignment: .leading)
        .overlay {
            RoundedRectangle(cornerRadius: AppRadius.radius10)
                .strokeBorder(
                    LinearGradient(
                        colors: [
                            ColorToken.Gradient.lightBlue.asColor,
                            ColorToken.Gradient.darkBlue.asColor,
                            ColorToken.Gradient.purple.asColor,
                            ColorToken.Gradient.pink.asColor,
                            ColorToken.Gradient.orange.asColor
                        ],
                        startPoint: .topLeading,
                        endPoint: .bottomTrailing
                    ),
                    lineWidth: 1
                )
        }
    }

    private func didTapUpgrade() {
        // TODO:
    }
}

#Preview {
    UpsellBannerView(
        logo: KDriveResources.ksuiteProLogo.swiftUIImage,
        text: "Create and collaborate without limits",
        upsellLink: URL(string: "https://www.test.com/en/test")!
    )
}
