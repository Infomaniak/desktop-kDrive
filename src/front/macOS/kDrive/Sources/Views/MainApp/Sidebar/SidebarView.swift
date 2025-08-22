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

    @StateObject private var synchronizedFolders: SequenceObserver<[UIFolder]>

    @Binding var currentTab: AppTab?

    init(currentTab: Binding<AppTab?>) {
        _currentTab = currentTab

        @InjectService var serverBridge: ServerBridgeable
        _synchronizedFolders = StateObject(wrappedValue: SequenceObserver(sequence: serverBridge.getSynchronizedFolders()))
    }

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

                if let folders = synchronizedFolders.value, !folders.isEmpty {
                    if #available(macOS 14.0, *) {
                        Section(.sidebarSectionAdvancedSync, isExpanded: $isSyncSectionExpanded) {
                            ForEach(folders) { folder in
                                Label { Text(folder.title) } icon: { Image(.folder) }
                            }
                        }
                    } else {
                        Section(.sidebarSectionAdvancedSync) {
                            ForEach(folders) { folder in
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
    }
}

#Preview {
    SidebarView(currentTab: .constant(.home))
}
