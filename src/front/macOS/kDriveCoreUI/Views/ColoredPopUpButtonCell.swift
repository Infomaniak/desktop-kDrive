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

public final class ColoredPopUpButtonCell: NSPopUpButtonCell {
    public static let customSpacing: CGFloat = 4.0

    override public func drawTitle(_ title: NSAttributedString, withFrame frame: NSRect, in controlView: NSView) -> NSRect {
        let updatedFrame = frame.offsetBy(dx: Self.customSpacing, dy: 0)
        return super.drawTitle(title, withFrame: updatedFrame, in: controlView)
    }

    override public func drawImage(_ image: NSImage, withFrame frame: NSRect, in controlView: NSView) {
        guard let selectedItem = menuItem as? ColoredMenuItem,
              let cgImage = image.cgImage(forProposedRect: nil, context: nil, hints: nil),
              let cgContext = NSGraphicsContext.current?.cgContext
        else {
            super.drawImage(image, withFrame: frame, in: controlView)
            return
        }

        cgContext.saveGState()

        let bounds = controlView.bounds
        cgContext.translateBy(x: 0, y: bounds.height)
        cgContext.scaleBy(x: 1, y: -1)

        cgContext.clip(to: frame, mask: cgImage)

        cgContext.setFillColor(selectedItem.color.cgColor)
        cgContext.fill(frame)

        cgContext.restoreGState()
    }
}
