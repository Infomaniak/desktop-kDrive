//
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

struct SynchroRulesPreferencesSheet: View {
    @Environment(\.dismiss) private var dismiss
    let item: SynchroRulesItem
    @State private var input = ""
    @State private var isNotified = false

    var body: some View {
        Text(KDriveLocalizable.dialogNewExclusionRuleTitle)

        Text(KDriveLocalizable.excludeRuleDescription)
            .font(.Tokens.callout)
            .foregroundStyle(.secondary)

        TextField(KDriveLocalizable.filesToExclude, text: $input)

        Toggle(isOn: $isNotified) {
            Text(KDriveLocalizable.notifyOnFileExcluded)
        }

        HStack {
            Button(KDriveLocalizable.buttonCancel) {
                dismiss()
            }

            Button(KDriveLocalizable.buttonAddFileExclusionRule) {
                switch item {
                case .files:
                    saveTemplate()
                case .apps:
                    saveApp()
                }
            }
        }
    }

    func saveTemplate() {
        // TODO: Save template
        dismiss()
    }

    func saveApp() {
        // TODO: Save app
        dismiss()
    }
}

#Preview {
    SynchroRulesPreferencesSheet(item: .apps)
}
