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

struct SynchroSelectorItem: Identifiable, Equatable {
    var id: UISynchro.ID {
        synchro.id
    }

    let synchro: UISynchro

    let icon: Image
    let iconColor: Color

    let title: String
    let subtitle: String?

    static func itemForSynchro(_ context: UISynchroContext, highPrecision: Bool) -> SynchroSelectorItem {
        if context.synchro.targetNodeId == nil {
            return mainSynchro(context, highPrecision: highPrecision)
        } else {
            return advancedSynchro(context)
        }
    }

    static func mainSynchro(_ context: UISynchroContext, highPrecision: Bool = false) -> SynchroSelectorItem {
        let title = highPrecision ? context.synchro.localPath.lastPathComponent : context.drive.name
        let subtitle = highPrecision ? context.drive.name : nil

        return SynchroSelectorItem(
            synchro: context.synchro,
            icon: KDriveResources.kdriveFoldersStacked.swiftUIImage,
            iconColor: context.drive.color ?? ColorToken.Drive.defaultColor.asColor,
            title: title,
            subtitle: subtitle
        )
    }

    static func advancedSynchro(_ context: UISynchroContext) -> SynchroSelectorItem {
        return SynchroSelectorItem(
            synchro: context.synchro,
            icon: KDriveResources.folder.swiftUIImage,
            iconColor: ColorToken.Accent.primary.asColor,
            title: context.synchro.localPath.lastPathComponent,
            subtitle: context.drive.name
        )
    }
}

extension [UISynchroContext] {
    func selectorItems() -> [SynchroSelectorItem] {
        var mainSynchrosCountPerDrive: [Int: Int] = [:]
        for synchroContext in self where synchroContext.synchro.targetNodeId == nil {
            mainSynchrosCountPerDrive[synchroContext.drive.dbId, default: 0] += 1
        }

        return map { synchroContext in
            let hasSeveralMainSynchrosForDrive = (mainSynchrosCountPerDrive[synchroContext.drive.dbId] ?? 0) > 1
            return SynchroSelectorItem.itemForSynchro(synchroContext, highPrecision: hasSeveralMainSynchrosForDrive)
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

    func update(with contexts: [UISynchroContext]) {
        items = contexts.selectorItems()
    }
}
