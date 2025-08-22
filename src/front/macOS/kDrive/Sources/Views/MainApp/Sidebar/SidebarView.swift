/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import InfomaniakDI
import kDriveCore
import kDriveCoreUI
import SwiftUI

struct SidebarView: View {
    @AppStorage("isSyncSectionExpanded") private var isSyncSectionExpanded = true

    @State private var synchronizedFolders = [UIFolder]()

    @Binding var currentTab: AppTab?

    var body: some View {
        VStack(alignment: .leading, spacing: 18) {
            SidebarHeaderView()
                .padding(.horizontal)

            List(selection: $currentTab) {
                Section {
                    ForEach(AppTab.allTabs) { tab in
                        NavigationLink(value: tab) {
                            Label { Text(tab.title) } icon: { tab.icon }
                        }
                    }
                    Label { Text(FolderItem.kDrive.title) } icon: { FolderItem.kDrive.icon }
                }

                if !synchronizedFolders.isEmpty {
                    if #available(macOS 14.0, *) {
                        Section(.sidebarSectionAdvancedSync, isExpanded: $isSyncSectionExpanded) {
                            ForEach(synchronizedFolders) { folder in
                                Label { Text(folder.title) } icon: { Image(.folder) }
                            }
                        }
                    } else {
                        Section(.sidebarSectionAdvancedSync) {
                            ForEach(synchronizedFolders) { folder in
                                Label { Text(folder.title) } icon: { Image(.folder) }
                            }
                        }
                        .collapsible(true)
                    }
                }
            }
            .scrollBounceBehavior(.basedOnSize)
        }
        .ikBackport.toolbar(removing: .sidebarToggle)
        .task {
            @InjectService var serverBridge: ServerBridgeable
            guard let synchronizedFoldersStream = try? await serverBridge.getSynchronizedFolders() else { return }

            for await folders in synchronizedFoldersStream {
                synchronizedFolders = folders
            }
        }
    }
}

#Preview {
    SidebarView(currentTab: .constant(.home))
}
