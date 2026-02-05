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
import OrderedCollections
import SwiftUI

enum VisibleActivities: String, Identifiable, CaseIterable {
    case myActivityOnly
    case allActivities

    var id: String { rawValue }
}

struct ActivityView: View {
    @ObservedObject var mainViewModel: MainViewModel

    @State private var visibleActivities = VisibleActivities.myActivityOnly

    private var synchroStatus: UISynchroStatus {
        switch visibleActivities {
        case .myActivityOnly:
            return mainViewModel.currentSynchro?.progressInfo?.status ?? .idle
        case .allActivities:
            let firstRunningSynchro = mainViewModel.availableSynchros.first { context in
                let synchroStatus = context.synchro.progressInfo?.status
                return synchroStatus == .starting || synchroStatus == .running
            }

            if let firstRunningSynchroStatus = firstRunningSynchro?.synchro.progressInfo?.status {
                return firstRunningSynchroStatus
            }
            return mainViewModel.availableSynchros.first?.synchro.progressInfo?.status ?? .idle
        }
    }

    private var activities: OrderedDictionary<UISynchroNode.ID, UISynchroNode> {
        switch visibleActivities {
        case .myActivityOnly:
            return mainViewModel.currentSynchro?.nodes ?? [:]
        case .allActivities:
            return [:]
        }
    }

    private var hasActivities: Bool {
        return !activities.isEmpty
    }

    var body: some View {
        VStack(spacing: AppPadding.padding32) {
            ActivityHeaderView(visibleActivities: $visibleActivities, synchroStatus: synchroStatus, hasActivities: hasActivities)

            if hasActivities {
                ActivitiesTable(activities: activities)
            } else {
                IKContentUnavailableView(
                    image: KDriveResources.mountainsTreesSunLight.swiftUIImage,
                    title: KDriveLocalizable.unavailableContentNoActivityTitle,
                    subtitle: KDriveLocalizable.unavailableContentNoActivityDescription
                )
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
            }
        }
        .padding(AppPadding.page)
    }
}

#Preview {
    ActivityView(mainViewModel: MainViewModel())
}
