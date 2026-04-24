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

struct SynchroErrorsInformationBlockView: View {
    let errorCount: Int

    private var title: AttributedString {
        do {
            return try AttributedString(markdown: KDriveLocalizable.informationBlockSynchroErrorTitle(errorCount))
        } catch {
            #if DEBUG
            fatalError("Failed decoding AttributedString: \(error)")
            #else
            return AttributedString()
            #endif
        }
    }

    var body: some View {
        InformationBlockView(
            title: title,
            subtitle: AttributedString(KDriveLocalizable.informationBlockSynchroErrorSubtitle),
            button: .init(title: KDriveLocalizable.buttonFixErrors) {
                // TODO: Connect to MainViewState implementation
            }
        )
    }
}

#Preview("One error") {
    SynchroErrorsInformationBlockView(errorCount: 1)
        .padding(AppPadding.padding32)
}

#Preview("Multiple errors") {
    SynchroErrorsInformationBlockView(errorCount: 5)
        .padding(AppPadding.padding32)
}
