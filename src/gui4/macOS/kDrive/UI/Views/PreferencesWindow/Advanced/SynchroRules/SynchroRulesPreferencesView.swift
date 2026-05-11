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

import InfomaniakDI
import kDriveCoreUI
import kDriveResources
import SwiftUI

enum SynchroRulesItem: String, Identifiable, CaseIterable {
    case files, apps

    var id: String {
        return rawValue
    }

    var preferencesViewDetail: PreferencesViewDetail {
        return .synchroRulesDetail(self)
    }

    var title: String {
        switch self {
        case .files:
            return KDriveLocalizable.filesToExclude
        case .apps:
            return KDriveLocalizable.appsToExclude
        }
    }

    var navigationDescription: String {
        switch self {
        case .files:
            return KDriveLocalizable.excludeRuleFileDescription
        case .apps:
            return KDriveLocalizable.excludeRuleAppDescription
        }
    }
}

struct SynchroRulesPreferencesView: View {
    var body: some View {
        Form {
            ForEach(SynchroRulesItem.allCases) { item in
                FormNavigationCell(title: item.title, description: item.navigationDescription) {
                    @InjectService var router: PreferencesViewRouter
                    router.append(item.preferencesViewDetail)
                }
            }
        }
        .groupedFormatStyle()
    }
}

#Preview {
    SynchroRulesPreferencesView()
}
