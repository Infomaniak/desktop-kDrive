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

import kDriveCoreUI
import SwiftUI

extension SynchroStatus {
    var state: SynchroStatusView.State {
        switch self {
        case .upToDate:
            return .synchroUpToDate
        case .inProgress:
            return .synchroInProgress
        case .paused:
            return .synchroPaused
        }
    }
}

struct SynchroStatusView: View {
    struct State {
        let animation: String
        let title: String
        let subtitle: String

        static let synchroUpToDate = State(
            animation: "",
            title: "Pas d'activité en cours",
            subtitle: "Vos fichiers sont accessibles et synchronisés."
        )

        static let synchroInProgress = State(
            animation: "",
            title: "Synchronisation en cours",
            subtitle: "Vos fichiers récents sont en train d’être mis à jour."
        )

        static let synchroPaused = State(
            animation: "",
            title: "Pause activée",
            subtitle: "La synchronisation est temporairement arrêtée."
        )

        static let offline = State(
            animation: "",
            title: "Hors ligne",
            subtitle: "Vos fichiers locaux restent accessibles.La synchronisation reprendra automatiquement dès que vous serez reconnecté."
        )
    }

    let status: SynchroStatus

    private var state: State {
        return status.state
    }

    var body: some View {
        VStack(spacing: AppPadding.padding32) {
            // TODO: Animation here

            VStack(spacing: AppPadding.padding8) {
                Text(state.title)
                    .font(.Tokens.title3)

                Text(state.subtitle)
                    .font(.Tokens.body)
            }
            .foregroundStyle(ColorToken.Text.primary.asColor)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .background(ColorToken.Surface.primary.asColor)
        .clipShape(RoundedRectangle(cornerRadius: AppRadius.radius16))
    }
}

#Preview("Up To Date") {
    SynchroStatusView(status: .upToDate)
}

#Preview("In Progress") {
    SynchroStatusView(status: .inProgress)
}

#Preview("Pause") {
    SynchroStatusView(status: .paused)
}
