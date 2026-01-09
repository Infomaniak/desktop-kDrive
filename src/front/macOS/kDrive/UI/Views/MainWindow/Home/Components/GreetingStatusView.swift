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

import SwiftUI
import kDriveResources
import kDriveCoreUI

enum SynchroStatus {
    case inProgress
    case paused
    case upToDate
}

extension SynchroStatus {
    var label: String {
        switch self {
        case .inProgress:
            return KDriveLocalizable.synchroInProgress
        case .paused:
            return KDriveLocalizable.synchroPaused
        case .upToDate:
            return KDriveLocalizable.synchroUpToDate
        }
    }
}

struct GreetingStatusView: View {
    var name: String
    var synchroStatus: SynchroStatus

    private var attributedString: AttributedString {
        var attributedString = AttributedString(KDriveLocalizable.greetingLabel(name, synchroStatus.label))

        attributedString.font = .Tokens.title2
        if let range = attributedString.range(of: synchroStatus.label) {
            attributedString[range].font = .Tokens.title2Emphasized
        }

        return attributedString
    }

    var body: some View {
        Text(attributedString)
    }
}

#Preview {
    GreetingStatusView(name: "Valentin", synchroStatus: .upToDate)
}
