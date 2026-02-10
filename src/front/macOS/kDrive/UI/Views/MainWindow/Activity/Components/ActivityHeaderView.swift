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
import kDriveResources
import SwiftUI

extension VisibleActivities {
    var icon: Image {
        switch self {
        case .myActivityOnly:
            return KDriveResources.person.swiftUIImage
        case .allActivities:
            return KDriveResources.people.swiftUIImage
        }
    }

    var title: String {
        switch self {
        case .myActivityOnly:
            return KDriveLocalizable.activityTypeMyActivity
        case .allActivities:
            return KDriveLocalizable.activityTypeAllActivities
        }
    }
}

struct ActivityHeaderView: View {
    @Binding var visibleActivities: VisibleActivities

    let synchroStatus: UISynchroStatus
    let hasActivities: Bool

    var title: String {
        guard hasActivities else {
            return KDriveLocalizable.activityTitleNoActivity
        }

        switch synchroStatus {
        case .starting, .running:
            return KDriveLocalizable.activityTitleInProgress
        case .idle, .paused, .stopped, .error:
            return KDriveLocalizable.activityTitleIdle
        case .pauseAsked, .stopAsked:
            return "Placeholder string for loading"
        }
    }

    var isLoading: Bool {
        switch synchroStatus {
        case .pauseAsked, .stopAsked:
            return true
        default:
            return false
        }
    }

    var body: some View {
        HStack(spacing: AppPadding.padding8) {
            Text(title)
                .font(.Tokens.title2)
                .foregroundStyle(ColorToken.Text.primary.asColor)
                .frame(maxWidth: .infinity, alignment: .leading)
                .redacted(reason: isLoading ? .placeholder : [])

            Picker(KDriveLocalizable.accessibilityActivityTypePicker, selection: $visibleActivities) {
                ForEach(VisibleActivities.allCases) { activity in
                    Label { Text(activity.title) } icon: { activity.icon }
                        .tag(activity)
                }
            }
            .pickerStyle(.menu)
            .labelsHidden()
        }
    }
}

@available(macOS 14.0, *)
#Preview("In Progress") {
    @Previewable @State var visibleActivities: VisibleActivities = .myActivityOnly
    ActivityHeaderView(visibleActivities: $visibleActivities, synchroStatus: .running, hasActivities: true)
}

@available(macOS 14.0, *)
#Preview("No Activity") {
    @Previewable @State var visibleActivities: VisibleActivities = .myActivityOnly
    ActivityHeaderView(visibleActivities: $visibleActivities, synchroStatus: .idle, hasActivities: false)
        .padding()
}

@available(macOS 14.0, *)
#Preview("Idle") {
    @Previewable @State var visibleActivities: VisibleActivities = .myActivityOnly
    ActivityHeaderView(visibleActivities: $visibleActivities, synchroStatus: .idle, hasActivities: true)
}

@available(macOS 14.0, *)
#Preview("Loading") {
    @Previewable @State var visibleActivities: VisibleActivities = .myActivityOnly
    ActivityHeaderView(visibleActivities: $visibleActivities, synchroStatus: .stopAsked, hasActivities: true)
}
