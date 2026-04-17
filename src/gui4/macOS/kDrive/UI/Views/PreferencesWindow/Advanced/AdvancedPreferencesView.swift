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

enum AdvancedPreferencesItem: String, Identifiable, CaseIterable {
    var id: String {
        return rawValue
    }

    case synchroRules
    case dataManagement
    case network
    case debugLogs

    var label: String {
        switch self {
        case .synchroRules:
            return KDriveLocalizable.syncRules
        case .dataManagement:
            return KDriveLocalizable.dataManagementSettings
        case .network:
            return KDriveLocalizable.networkSettings
        case .debugLogs:
            return KDriveLocalizable.debugLogsSettings
        }
    }

    var isDisabled: Bool {
        switch self {
		case .dataManagement:
            return false
		case .debugLogs:
            return false
		case .network:
			return false
        default:
            return true
        }
    }

    var preferencesViewDetail: PreferencesViewDetail {
        switch self {
        case .debugLogs:
            return .debug
        case .dataManagement:
            return .dataManagement
        case .network:
            return .network
        default:
            fatalError("Not yet implemented")
        }
    }
}

struct AdvancedPreferencesView: View {
    var body: some View {
        Form {
            ForEach(AdvancedPreferencesItem.allCases) { item in
                FormNavigationCell(label: item.label) {
                    navigate(to: item)
                }
                .disabled(item.isDisabled)
            }
        }
        .groupedFormatStyle()
    }

    private func navigate(to item: AdvancedPreferencesItem) {
        @InjectService var preferencesViewRouter: PreferencesViewRouter
        preferencesViewRouter.append(item.preferencesViewDetail)
    }
}

#Preview {
    AdvancedPreferencesView()
}
