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

struct ErrorsListView: View {
    let errors: [UISynchroErrorCategory: [SynchroError]]

    private var categories: [UISynchroErrorCategory] {
        return Array(errors.keys)
    }

    var body: some View {
        Form {
            ForEach(categories) { category in
                Section {
                    ForEach(errors[category, default: []], id: \.kind) { error in
                        ErrorCellFactory().make(error: error)
                    }
                } header: {
                    Text(category.title)
                }
            }
        }
        .groupedFormatStyle()
    }
}

#Preview {
    ErrorsListView(errors: [:])
}
