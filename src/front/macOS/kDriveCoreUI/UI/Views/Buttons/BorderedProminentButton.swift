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

import Cocoa
import kDriveResources

public final class BorderedProminentButton: NSButton {
    var backgroundColor = NSColor.Tokens.Action.primary
    var foregroundColor = NSColor.Tokens.Action.onPrimary

    public override var title: String {
        didSet {
            setupTitleColor()
        }
    }

    public override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        setupColors()
    }

    public required init?(coder: NSCoder) {
        super.init(coder: coder)
        setupColors()
    }
    
    private func setupColors() {
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
