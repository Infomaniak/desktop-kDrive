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

struct UpdateRequiredView: View {
    var body: some View {
        VStack(spacing: AppPadding.padding32) {
            KDriveResources.foldersStackArrowsCounterclockwise.swiftUIImage
                .resizable()
                .scaledToFit()
                .frame(maxWidth: 160)

            VStack(spacing: AppPadding.padding16) {
                Text(KDriveLocalizable.updateRequiredTitle)
                    .font(.Tokens.titleEmphasized)
                Text(KDriveLocalizable.updateRequiredDescription)
                    .font(.Tokens.body)
            }
            .multilineTextAlignment(.center)
            .foregroundStyle(ColorToken.Text.primary.asColor)

            Button(KDriveLocalizable.buttonUpdateNow, action: updateApp)
                .buttonStyle(.borderedProminent)
        }
        .padding(AppPadding.padding16)
        .background(ColorToken.Surface.primary.asColor, in: .rect(cornerRadius: AppRadius.radius16))
        .padding(AppPadding.padding24)
    }

    private func updateApp() {

    }
}

#Preview {
    UpdateRequiredView()
}
