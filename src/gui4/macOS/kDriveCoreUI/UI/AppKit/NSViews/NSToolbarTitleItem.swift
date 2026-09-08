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

import AppKit

public final class NSToolbarTitleItem: NSToolbarItem {
    public var stringValue: String {
        get {
            return textField.stringValue
        }
        set {
            textField.stringValue = newValue
        }
    }

    private lazy var textField: NSTextField = {
        let label = NSTextField(labelWithString: "")
        label.font = .titleBarFont(ofSize: 13)
        label.alignment = .center
        return label
    }()

    public init() {
        super.init(itemIdentifier: .titleLabel)
        view = textField
    }
}
