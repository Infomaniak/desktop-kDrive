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

struct UpsellBannerView: View {
    @Environment(\.openURL) private var openURL

    @LazyInjectService private var matomo: MatomoUtils

    let offer: UpsellOffer

    var body: some View {
        VStack(alignment: .leading, spacing: AppPadding.padding16) {
            offer.plan.logo

            Text(offer.plan.description)
                .foregroundStyle(offer.plan.foregroundStyle)

            Button(KDriveLocalizable.buttonUpgradeSubscriptionShort, action: didTapUpgrade)
                .foregroundStyle(offer.plan.foregroundStyle)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(AppPadding.padding16)
        .background {
            if offer.plan == .kSuitePro {
                KDriveResources.upsellGradient.swiftUIImage
                    .resizable()
                    .scaledToFill()
                    // The SVG's feGaussianBlur filters are ignored by the asset catalog renderer, so blur is applied here.
                    .blur(radius: 15.72)
                    .background(ColorToken.upsellBackground.asColor)
            }
        }
        .clipShape(RoundedRectangle(cornerRadius: AppRadius.radius10))
        .overlay {
            if offer.plan == .myKSuitePlus {
                RoundedRectangle(cornerRadius: AppRadius.radius10)
                    .strokeBorder(
                        LinearGradient(
                            colors: gradientColors,
                            startPoint: .topLeading,
                            endPoint: .bottomTrailing
                        ),
                        lineWidth: 1
                    )
            }
        }
    }

    private func didTapUpgrade() {
        matomo.track(eventWithCategory: .upsell, name: offer.plan.matomoName)
        openURL(offer.url)
    }
}

private extension UpsellOffer.Plan {
    var logo: Image {
        switch self {
        case .myKSuitePlus:
            return KDriveResources.ksuitePlusLogo.swiftUIImage
        case .kSuitePro:
            return KDriveResources.ksuiteProLogo.swiftUIImage
        }
    }

    var description: String {
        switch self {
        case .myKSuitePlus:
            return KDriveLocalizable.myksuitePlusBannerDescription
        case .kSuitePro:
            return KDriveLocalizable.ksuiteProBannerDescription
        }
    }

    /// The kSuite Pro banner always has a dark background, whereas the my kSuite+ one follows the system appearance.
    var foregroundStyle: Color {
        switch self {
        case .myKSuitePlus:
            return ColorToken.Text.primary.asColor
        case .kSuitePro:
            return .white
        }
    }

    var matomoName: String {
        switch self {
        case .myKSuitePlus:
            return "myKsuitePlus"
        case .kSuitePro:
            return "kSuitePro"
        }
    }
}

private let gradientColors: [Color] = [
    ColorToken.UpsellGradient.lightBlue.asColor,
    ColorToken.UpsellGradient.darkBlue.asColor,
    ColorToken.UpsellGradient.purple.asColor,
    ColorToken.UpsellGradient.pink.asColor,
    ColorToken.UpsellGradient.orange.asColor
]

#Preview("kSuite Pro") {
    UpsellBannerView(offer: UpsellOffer(plan: .kSuitePro, url: URL(string: "https://shop.infomaniak.com")!))
        .padding()
}

#Preview("my kSuite+") {
    UpsellBannerView(offer: UpsellOffer(plan: .myKSuitePlus, url: URL(string: "https://shop.infomaniak.com")!))
        .padding()
}
