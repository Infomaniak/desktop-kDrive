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
import SwiftUI

struct DriveErrorAction {
    let title: String
    let action: () -> Void
}

struct BlockingErrorView: View {
    let blockingError: UIBlockingError

    let isLoading: Bool
    let action: DriveErrorAction?

    init(blockingError: UIBlockingError, isLoading: Bool = false, action: DriveErrorAction?) {
        self.blockingError = blockingError
        self.isLoading = isLoading
        self.action = action
    }

    var body: some View {
        VStack(spacing: 0) {
            DriveLabel(
                drive: blockingError.drive,
                badgeIcon: blockingError.badgeIcon,
                badgeBackgroundColor: blockingError.badgeBackgroundColor,
                badgeColor: blockingError.badgeColor
            )
            .padding(.bottom, AppPadding.padding64)

            VStack(spacing: AppPadding.padding16) {
                Text(blockingError.title)
                    .font(.Tokens.titleEmphasized)
                if let subtitle = blockingError.subtitle {
                    Text(subtitle)
                        .font(.Tokens.body)
                        .fixedSize(horizontal: false, vertical: true)
                }
            }
            .multilineTextAlignment(.center)
            .padding(.bottom, AppPadding.padding24)

            if isLoading {
                ProgressView()
                    .controlSize(.small)
                    .padding(.bottom, AppPadding.padding24)
            }

            if let action {
                Button(action.title, action: action.action)
                    .buttonStyle(.borderedProminent)
            }
        }
        .padding(AppPadding.padding32)
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .background(
            RoundedRectangle(cornerRadius: AppRadius.radius16)
                .fill(ColorToken.Surface.primary.asColor)
        )
        .padding(AppPadding.padding24)
    }
}

#Preview {
    VStack {
        ForEach(SynchroError.allCases, id: \.self) { error in
            BlockingErrorView(
                blockingError: PreviewHelper.blockingErrorFor(syncError: error, isDriveAdmin: true),
                isLoading: true,
                action: DriveErrorAction(title: "Refresh") {}
            )
            .frame(minWidth: 512)
        }
    }
}
