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

/// Content of the file opening pop-up window: an explanation while the item is being synchronized, the name of the
/// item being downloaded along with a progress gauge while it is being downloaded, and a cancellation button in both
/// cases.
struct FileOpeningStatusView: View {
    let itemName: String
    let phase: FileOpeningStatusPhase
    let percent: Int
    let onCancel: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: AppPadding.padding16) {
            Text(message)
                .foregroundStyle(ColorToken.Text.primary.asColor)

            if phase == .hydrating {
                ProgressView(value: Double(percent), total: 100)
            }

            HStack {
                Spacer()
                Button(KDriveLocalizable.buttonCancel, action: onCancel)
            }
        }
        .padding(AppPadding.padding16)
        .frame(width: 340)
    }

    private var message: AttributedString {
        let text = phase == .hydrating
            ? (itemName.isEmpty ? KDriveLocalizable.fileOpeningHydratingMessage
                                : KDriveLocalizable.fileOpeningDownloadingMessage(itemName))
            : KDriveLocalizable.fileOpeningWaitingSyncMessage

        var attributedMessage = AttributedString(text)
        attributedMessage.font = .body
        if phase == .hydrating, !itemName.isEmpty, let itemNameRange = attributedMessage.range(of: itemName) {
            attributedMessage[itemNameRange].font = .body.bold()
        }

        return attributedMessage
    }
}
