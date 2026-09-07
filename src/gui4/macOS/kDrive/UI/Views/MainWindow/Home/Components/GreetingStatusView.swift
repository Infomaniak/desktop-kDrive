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

extension HomeState {
    var greetingLabel: String {
        switch self {
        case .loading, .synchroIsUpToDate, .offline:
            return KDriveLocalizable.synchroUpToDate
        case .synchroIsRunning:
            return KDriveLocalizable.synchroInProgress
        case .synchroIsPaused:
            return KDriveLocalizable.synchroPaused
        }
    }
}

struct GreetingStatusView: View {
    var name: String
    var state: HomeState

    private var attributedString: AttributedString {
        var attributedString = AttributedString(KDriveLocalizable.greetingLabel(name, state.greetingLabel))

        attributedString.font = .Tokens.title2
        if let range = attributedString.range(of: state.greetingLabel) {
            attributedString[range].font = .Tokens.title2Emphasized
        }

        return attributedString
    }

    var body: some View {
        Text(attributedString)
            .redacted(reason: state.isRedacted ? .placeholder : [])
    }
}

#Preview("Up To Date") {
    GreetingStatusView(name: "Tim", state: .synchroIsUpToDate)
}

#Preview("Running") {
    GreetingStatusView(name: "Tim", state: .synchroIsRunning)
}

#Preview("Paused") {
    GreetingStatusView(name: "Tim", state: .synchroIsPaused)
}

#Preview("Offline") {
    GreetingStatusView(name: "Tim", state: .offline)
}

#Preview("Loading") {
    GreetingStatusView(name: "Tim", state: .loading)
}
