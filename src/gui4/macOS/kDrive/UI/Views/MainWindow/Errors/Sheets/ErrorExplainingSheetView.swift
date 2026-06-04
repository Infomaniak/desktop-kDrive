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

struct ErrorExplainingSheetView: View {
    let title: String
    let description: String
    let explanations: [Explanation]
    let mainAction: Action

    struct Explanation: Identifiable {
        let id: Int
        let title: String
        let description: AttributedString
    }

    struct Action {
        let title: String
        let handler: @MainActor () -> Void
    }

    var body: some View {
        VStack(alignment: .leading, spacing: AppPadding.padding16) {
            VStack(alignment: .leading, spacing: AppPadding.padding8) {
                Text(title)
                    .foregroundStyle(ColorToken.Text.primary.asColor)
                    .font(.Tokens.title3Emphasized)

                Text(description)
                    .foregroundStyle(ColorToken.Text.primary.asColor)
                    .font(.Tokens.body)
            }

            ForEach(explanations) { explanation in
                GroupBox {
                    VStack(alignment: .leading, spacing: AppPadding.padding8) {
                        Text(explanation.title)
                            .foregroundStyle(ColorToken.Text.primary.asColor)
                            .font(.Tokens.bodyEmphasized)

                        HStack(spacing: AppPadding.padding4) {
                            KDriveResources.arrowRight.swiftUIImage
                                .resizable(at: AppIconSize.iconSize12)
                                .foregroundStyle(ColorToken.Text.tertiary.asColor)

                            Text(explanation.description)
                                .foregroundStyle(ColorToken.Text.primary.asColor)
                                .font(.Tokens.callout)
                        }
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding(AppPadding.padding8)
                }
            }
        }
        .padding()
        .toolbar {
            ToolbarItem(placement: .cancellationAction) {
                Button(KDriveLocalizable.buttonClose, role: .cancel) {}
            }
            ToolbarItem(placement: .primaryAction) {
                Button(mainAction.title, action: mainAction.handler)
                    .buttonStyle(.borderedProminent)
            }
        }
    }
}

#Preview {
    ErrorExplainingSheetView(
        title: "My Title",
        description: "My Description",
        explanations: [
            .init(id: 0, title: "Title", description: "Description"),
            .init(id: 1, title: "Title", description: "Description")
        ],
        mainAction: .init(title: "My Button") {}
    )
}
