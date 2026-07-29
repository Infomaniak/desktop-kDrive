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

import Combine
import Foundation
import kDriveCoreUI
import kDriveResources
import SwiftUI

struct SynchroSelectorItem: Identifiable {
    var id: UISynchro.ID {
        synchro.id
    }

    let synchro: UISynchro

    let icon: Image
    let iconColor: Color

    let title: String
    let subtitle: String?

    let notification: Bool

    static func itemForSynchro(_ info: UISynchroInfo, highPrecision: Bool) -> SynchroSelectorItem {
        if info.context.synchro.targetNodeId == nil {
            return mainSynchro(info, highPrecision: highPrecision)
        } else {
            return advancedSynchro(info)
        }
    }

    static func mainSynchro(_ info: UISynchroInfo, highPrecision: Bool = false) -> SynchroSelectorItem {
        let title = highPrecision ? info.context.synchro.localPath.lastPathComponent : info.context.drive.name
        let subtitle = highPrecision ? info.context.drive.name : nil

        return SynchroSelectorItem(
            synchro: info.context.synchro,
            icon: KDriveResources.kdriveFoldersStackedFilled.swiftUIImage,
            iconColor: info.context.drive.color ?? ColorToken.Drive.defaultColor.asColor,
            title: title,
            subtitle: subtitle,
            notification: info.state.errorCount > 0
        )
    }

    static func advancedSynchro(_ info: UISynchroInfo) -> SynchroSelectorItem {
        return SynchroSelectorItem(
            synchro: info.context.synchro,
            icon: KDriveResources.folder.swiftUIImage,
            iconColor: ColorToken.Accent.primary.asColor,
            title: info.context.synchro.localPath.lastPathComponent,
            subtitle: info.context.drive.name,
            notification: info.state.errorCount > 0
        )
    }
}

extension [UISynchroInfo] {
    func selectorItems() -> [SynchroSelectorItem] {
        var mainSynchrosCountPerDrive: [Int: Int] = [:]
        for synchroInfo in self where synchroInfo.context.synchro.targetNodeId == nil {
            mainSynchrosCountPerDrive[synchroInfo.context.drive.dbId, default: 0] += 1
        }

        return map { synchroInfo in
            let hasSeveralMainSynchrosForDrive = (mainSynchrosCountPerDrive[synchroInfo.context.drive.dbId] ?? 0) > 1
            return SynchroSelectorItem.itemForSynchro(synchroInfo, highPrecision: hasSeveralMainSynchrosForDrive)
        }
    }
}

@MainActor
final class SynchroSelectorViewModel: ObservableObject {
    @Published private(set) var items: [SynchroSelectorItem] = []
    @Published var selectedSynchroId: UISynchro.ID?

    var onSelect: ((UISynchro) -> Void)?

    var selectedItem: SynchroSelectorItem? {
        items.first { $0.id == selectedSynchroId } ?? items.first
    }

    func update(with contexts: [UISynchroInfo]) {
        items = contexts.selectorItems()
    }
}
