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

struct QuickConflictsResolutionView: View {
    @State private var isLoadingButton = false

    let errors: [SynchroError]

    private var errorsCount: Int {
        return errors.count
    }

    var body: some View {
        VStack(alignment: .leading, spacing: AppPadding.padding32) {
            VStack(alignment: .leading) {
                Text(KDriveLocalizable.labelManyConflictTitle(errorsCount))
                    .font(.Tokens.body)
                    .foregroundStyle(ColorToken.Text.primary.asColor)
                Text(KDriveLocalizable.labelChooseConflictVersion)
                    .font(.Tokens.callout)
                    .foregroundStyle(ColorToken.Text.tertiary.asColor)
            }

            // TODO: Options

            HStack {
                LoadingButton(isLoading: $isLoadingButton, action: applyQuickChange) {
                    Text(KDriveLocalizable.buttonApply)
                }
            }
        }
    }

    private func applyQuickChange() {

    }

    private func navigateToConflictList() {

    }
}

#Preview {
    QuickConflictsResolutionView(errors: Array(repeating: PreviewHelper.synchroError, count: 4))
}
