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

import Cocoa
import kDriveResources

public final class BorderedProminentButton: NSButton {
    var backgroundColor = ColorToken.Action.primary.asNSColor
    var foregroundColor = ColorToken.Action.onPrimary.asNSColor

    override public var title: String {
        didSet {
            setupTitleColor()
        }
    }

    override public init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        setupColors()
    }

    public required init?(coder: NSCoder) {
        super.init(coder: coder)
        setupColors()
    }

    private func setupColors() {
        if #unavailable(macOS 14.0) {
            // Code-created buttons default to `.automatic` (a push button) from macOS 14 only. Earlier systems use a
            // style that ignores `bezelColor`, so the push style is set explicitly.
            bezelStyle = .push
        }
        bezelColor = backgroundColor
        setupTitleColor()
    }

    private func setupTitleColor() {
        attributedTitle = NSAttributedString(string: title, attributes: [.foregroundColor: foregroundColor])
    }
}

@available(macOS 14.0, *)
#Preview {
    let button = BorderedProminentButton()
    button.title = "Bordered Button"
    return button
}
