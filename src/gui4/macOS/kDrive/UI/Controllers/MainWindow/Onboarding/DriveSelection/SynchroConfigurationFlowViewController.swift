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

import AppKit
import kDriveResources
import SwiftUI

final class SynchroConfigurationFlowViewController: NSHostingController<SynchroConfigurationFlowView> {
    init(
        userDbId: Int,
        configurations: [SynchroConfiguration],
        onConfirm: (([SynchroConfiguration]) async -> Void)? = nil,
        onCancel: (() -> Void)? = nil
    ) {
        super.init(rootView: SynchroConfigurationFlowView(
            userDbId: userDbId,
            configurations: configurations,
            onConfirm: onConfirm,
            onCancel: onCancel
        ))
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func viewDidAppear() {
        super.viewDidAppear()

        guard let window = view.window else {
            return
        }

        window.isMovable = false
        window.toolbarStyle = .unifiedCompact
        window.title = KDriveLocalizable.buttonAdvancedParameters

        window.setFrame(NSRect(origin: .zero, size: CGSize(width: 500, height: 350)), display: true)
    }
}
