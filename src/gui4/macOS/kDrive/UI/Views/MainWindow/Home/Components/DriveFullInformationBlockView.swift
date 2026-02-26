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

import InfomaniakDI
import kDriveCore
import kDriveCoreUI
import kDriveResources
import SwiftUI

struct DriveFullInformationBlockView: View {
    @Environment(\.openURL) private var openURL

    @State private var isDismissed = false

    let quotaError: UISynchroError

    private var button: InformationBlockButton? {
        guard let actionTitle = quotaError.actionTitle else { return nil }

        return InformationBlockButton(title: actionTitle) {
            openURL(URL(string: "<URL>")!)
        }
    }

    var body: some View {
        if !isDismissed {
            Button {
                @InjectService var router: MainViewRouter
                router.setCurrentTab(.storage)
            } label: {
                InformationBlockView(
                    icon: quotaError.badgeIcon,
                    title: quotaError.title,
                    subtitle: quotaError.subtitle ?? "",
                    button: button
                ) {
                    isDismissed = true
                }
            }
            .buttonStyle(.plain)
        }
    }
}

#Preview("Not admin") {
    DriveFullInformationBlockView(quotaError: PreviewHelper.blockingErrorFor(syncError: .quota, isDriveAdmin: false))
}

#Preview("Admin") {
    DriveFullInformationBlockView(quotaError: PreviewHelper.blockingErrorFor(syncError: .quota, isDriveAdmin: true))
}
