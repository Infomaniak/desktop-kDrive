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

struct ErrorsHeaderView: View {
    @State private var isLoading = false

    let synchroDbId: UISynchro.ID?
    let errorsCount: Int

    private var title: AttributedString {
        do {
            return try AttributedString(markdown: KDriveLocalizable.informationBlockSynchroErrorTitle(errorsCount))
        } catch {
            #if DEBUG
            fatalError("Failed decoding AttributedString: \(error)")
            #else
            return AttributedString()
            #endif
        }
    }

    var body: some View {
        HStack {
            Text(title)
                .font(.Tokens.body)
                .foregroundStyle(ColorToken.Text.primary.asColor)
                .frame(maxWidth: .infinity, alignment: .leading)

            LoadingButton(isLoading: $isLoading, action: refreshList) {
                Text(KDriveLocalizable.buttonRefresh)
            }
        }
    }

    private func refreshList() async {
        guard let synchroDbId else { return }
        _ = try? await ErrorJobs().refreshSyncErrors(syncDbId: Int32(synchroDbId))
    }
}

#Preview {
    ErrorsHeaderView(synchroDbId: 0, errorsCount: 42)
}
