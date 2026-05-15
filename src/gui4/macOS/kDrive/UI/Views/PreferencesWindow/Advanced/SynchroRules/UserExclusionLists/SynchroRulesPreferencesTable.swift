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

struct RuleHeader: View {
    let text: String
    @Binding var isShowingAlert: Bool

    var body: some View {
        HStack {
            Text(text)
            InformationButton {
                isShowingAlert = true
            }
        }
    }
}

struct AppTableRow: View {
    let item: UIExclusionAppInfo
    let list: [UIExclusionAppInfo]
    @Binding var selectedRows: Set<UIExclusionAppInfo.ID>

    var body: some View {
        HStack {
            Toggle(isOn: Binding(
                get: { selectedRows.contains(item.id) },
                set: { isSelected in
                    if isSelected {
                        selectedRows.insert(item.id)
                    } else {
                        selectedRows.remove(item.id)
                    }
                }

            )) {
                EmptyView()
            }
            .labelsHidden()

            Text(item.app)
        }
    }
}

struct TemplateTableRow: View {
    let item: UIExclusionTemplateInfo
    @Binding var list: [UIExclusionTemplateInfo]
    @Binding var selectedRows: Set<UIExclusionAppInfo.ID>

    var body: some View {
        HStack {
            Toggle(isOn: Binding(
                get: { selectedRows.contains(item.id) },
                set: { isSelected in
                    if isSelected {
                        selectedRows.insert(item.id)
                    } else {
                        selectedRows.remove(item.id)
                    }
                }

            )) {
                EmptyView()
            }
            .labelsHidden()
            .frame(width: 24)
            TextWithIcon(icon: KDriveResources.file.swiftUIImage, text: item.template)
                .frame(maxWidth: .infinity, alignment: .leading)

            HStack {
                Toggle("", isOn: Binding(
                    get: { item.warning },
                    set: { isEnabled in
                        let index = list.firstIndex { $0.id == item.id }!
                        list[index].warning = isEnabled
                    }
                ))
                .controlSize(.small)
            }

            .frame(maxWidth: .infinity, alignment: .leading)
            .toggleStyle(.switch)
        }
    }
}

struct TemplateTable: View {
    @Binding var list: [UIExclusionTemplateInfo]
    @Binding var selectedRows: Set<UIExclusionTemplateInfo.ID>
    @Binding var isShowingAlert: Bool

    var body: some View {
        VStack(alignment: .leading) {
            HStack {
                Toggle(isOn: Binding(
                    get: { selectedRows.count == list.count },
                    set: { isSelected in
                        if isSelected {
                            selectedRows = Set(list.map { $0.id })
                        } else {
                            selectedRows.removeAll()
                        }
                    }

                )) {
                    EmptyView()
                }
                .labelsHidden()
                .frame(width: 24)

                Text("Rules")
                    .frame(maxWidth: .infinity, alignment: .leading)

                RuleHeader(text: "Notifications", isShowingAlert: $isShowingAlert)
                    .frame(maxWidth: .infinity, alignment: .leading)
            }

            ScrollView {
                ForEach(list) { item in
                    TemplateTableRow(item: item, list: $list, selectedRows: $selectedRows)
                }
            }
        }
        .frame(maxWidth: .infinity)
    }
}

struct AppTable: View {
    @Binding var list: [UIExclusionAppInfo]
    @Binding var selectedRows: Set<UIExclusionAppInfo.ID>

    var body: some View {
        VStack(alignment: .leading) {
            HStack {
                Toggle(isOn: Binding(
                    get: { selectedRows.count == list.count },
                    set: { isSelected in
                        if isSelected {
                            selectedRows = Set(list.map { $0.id })
                        } else {
                            selectedRows.removeAll()
                        }
                    }

                )) {
                    EmptyView()
                }
                .labelsHidden()
                .frame(width: 24)

                Text(KDriveLocalizable.labelName)
                    .frame(maxWidth: .infinity, alignment: .leading)
            }
            ForEach(list) { item in
                AppTableRow(item: item, list: list, selectedRows: $selectedRows)
            }
        }
    }
}
