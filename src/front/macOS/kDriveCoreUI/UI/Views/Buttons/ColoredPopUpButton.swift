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

public final class ColoredPopUpButton: NSPopUpButton {
    public init() {
        super.init(frame: .zero, pullsDown: false)
        cell = ColoredPopUpButtonCell()
    }

    @available(*, unavailable)
    public required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    public func addItem(withTitle title: String, image: NSImage, color: NSColor) {
        menu?.addItem(ColoredMenuItem(title: title, image: image, color: color))
    }
}

@available(macOS 14.0, *)
#Preview {
    let button = ColoredPopUpButton()
    let symbols = ["house", "bell", "star", "heart"]
    for name in symbols {
        let image = NSImage(systemSymbolName: name, accessibilityDescription: nil)!
        button.addItem(withTitle: name, image: image, color: NSColor.orange)
    }

    return button
}
