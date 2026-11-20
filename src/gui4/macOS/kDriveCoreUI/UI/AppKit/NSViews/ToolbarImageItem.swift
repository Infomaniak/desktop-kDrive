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

/// Toolbar item displaying a non-symbol image (e.g. an asset catalog SVG).
///
/// Before macOS 14, AppKit stretches non-symbol toolbar images to fill the legacy 32 pt item size.
/// On those systems the item hosts a toolbar-styled button that draws the image at symbol size instead.
/// macOS 14 and later keep the native image-based item.
public final class ToolbarImageItem: NSToolbarItem {
    private nonisolated static let legacyImageSize = NSSize(width: 16, height: 16)

    private var legacyButton: NSButton?

    override public var image: NSImage? {
        get {
            return legacyButton?.image ?? super.image
        }
        set {
            guard let legacyButton else {
                super.image = newValue
                return
            }
            legacyButton.image = newValue.map(Self.resizedForLegacyToolbar)
        }
    }

    override public var label: String {
        get {
            return super.label
        }
        set {
            super.label = newValue
            toolTip = newValue
            legacyButton?.setAccessibilityLabel(newValue)
            menuFormRepresentation?.title = newValue
        }
    }

    override public var isEnabled: Bool {
        get {
            return legacyButton?.isEnabled ?? super.isEnabled
        }
        set {
            super.isEnabled = newValue
            legacyButton?.isEnabled = newValue
            menuFormRepresentation?.isEnabled = newValue
        }
    }

    public init(itemIdentifier: NSToolbarItem.Identifier, label: String, target: AnyObject?, action: Selector) {
        super.init(itemIdentifier: itemIdentifier)

        if #unavailable(macOS 14.0) {
            let button = NSButton(title: "", target: target, action: action)
            button.bezelStyle = .texturedRounded
            button.showsBorderOnlyWhileMouseInside = true
            button.imagePosition = .imageOnly
            view = button
            legacyButton = button

            let menuItem = NSMenuItem(title: label, action: action, keyEquivalent: "")
            menuItem.target = target
            menuFormRepresentation = menuItem
        } else {
            self.target = target
            self.action = action
        }

        self.label = label
    }

    private nonisolated static func resizedForLegacyToolbar(_ image: NSImage) -> NSImage {
        guard let resizedImage = image.copy() as? NSImage else { return image }
        resizedImage.size = legacyImageSize
        resizedImage.isTemplate = image.isTemplate
        return resizedImage
    }
}
