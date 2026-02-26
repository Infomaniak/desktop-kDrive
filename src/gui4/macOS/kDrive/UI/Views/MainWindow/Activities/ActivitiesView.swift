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

import Combine
import InfomaniakDI
import kDriveCore
import kDriveCoreUI
import kDriveResources
import OrderedCollections
import SwiftUI

enum VisibleActivities: String, Identifiable, CaseIterable {
    case myActivityOnly
    case allActivities

    var id: String {
        rawValue
    }
}

struct ActivitiesView: View {
    @InjectService private var synchroStateObserver: SynchroStateObserving
    @InjectService private var synchroNodesObserver: SynchroNodesObserving

    @State private var visibleActivities = VisibleActivities.allActivities

    @State private var synchroState = UISynchroState(errorCount: 0, status: .idle)
    @State private var synchroStatus = UISynchroStatus.idle
    @State private var nodeContexts = [UISynchroNodeContext]()

    private var visibleNodes: [UISynchroNodeContext] {
        switch visibleActivities {
        case .myActivityOnly:
            return nodeContexts.filter { $0.node.direction == .up }
        case .allActivities:
            return nodeContexts
        }
    }

    private var hasAnyActivity: Bool {
        return !visibleNodes.isEmpty
    }

    var body: some View {
        VStack(spacing: AppPadding.padding32) {
            ActivityHeaderView(
                visibleActivities: $visibleActivities,
                synchroStatus: synchroStatus,
                hasAnyActivity: hasAnyActivity
            )

            ActivitiesTable(contexts: visibleNodes)
                .opacity(hasAnyActivity ? 1 : 0)
                .overlay(alignment: .top) {
                    if !hasAnyActivity {
                        IKContentUnavailableView(
                            image: KDriveResources.mountainsTreesSunLight.swiftUIImage,
                            title: KDriveLocalizable.unavailableContentNoActivityTitle,
                            subtitle: KDriveLocalizable.unavailableContentNoActivityDescription
                        )
                    }
                }
        }
        .padding(AppPadding.page)
        .onAppear {
            synchroState = synchroStateObserver.synchroState
            nodeContexts = synchroNodesObserver.synchroNodes
        }
        .onReceive(synchroStateObserver.synchroStatePublisher) { output in
            withAnimation {
                synchroState = output
            }
        }
        .onReceive(synchroNodesObserver.synchroNodesPublisher) { output in
            withAnimation {
                nodeContexts = output
            }
        }
    }

    private func fetchSynchroNodeContexts() async {
        @InjectService var coherentCache: CoherentCache
        let synchroNodeContexts = await coherentCache.getSynchroNodeContexts()

        handleUpdatedSynchroNodes(synchroNodeContexts.map(UISynchroNodeContext.init(synchroNodeContext:)))
    }

    private func handleUpdatedSynchroNodes(_ nodes: [UISynchroNodeContext]) {
        nodeContexts = nodes
    }
}

#Preview {
    ActivitiesView()
}
