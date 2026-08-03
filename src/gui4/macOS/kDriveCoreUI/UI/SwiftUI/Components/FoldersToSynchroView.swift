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

import kDriveResources
import SwiftUI

public struct FoldersToSynchroView: View {
    @State private var root = [FileTreeItem]()

    @Binding var blackList: Set<String>

    let initialBlackList: Set<String>
    let childrenFetcher: FileTreeChildrenFetcher

    public init(
        blackList: Binding<Set<String>>,
        initialBlackList: Set<String>,
        userDbId: Int,
        driveDbId: Int,
        rootNodeId: String? = nil
    ) {
        _blackList = blackList
        self.initialBlackList = initialBlackList
        childrenFetcher = FileTreeChildrenFetcher(userDbId: userDbId, driveDbId: driveDbId, rootNodeId: rootNodeId)
    }

    public var body: some View {
        VStack(alignment: .leading, spacing: AppPadding.padding24) {
            VStack(alignment: .leading, spacing: AppPadding.padding8) {
                Text(KDriveLocalizable.onboardingAdvancedSettingsDriveExclusionDescription)
                    .font(.Tokens.headline)
                    .foregroundStyle(ColorToken.Text.primary.asColor)

                Text(KDriveLocalizable.selectFoldersToSyncDescription)
                    .font(.Tokens.subheadline)
                    .foregroundStyle(ColorToken.Text.tertiary.asColor)
            }

            FileTreeView(
                rootItems: root,
                initialBlacklist: initialBlackList,
                childrenFetcher: childrenFetcher
            ) {
                blackList = $0
            }
            .frame(minHeight: 200)
        }
        .task {
            root = await childrenFetcher.fetchChildren(for: nil)
        }
    }
}

#Preview {
    FoldersToSynchroView(blackList: .constant(Set()), initialBlackList: Set(), userDbId: 0, driveDbId: 0)
}
