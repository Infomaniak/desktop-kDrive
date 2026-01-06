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

final class LargeButtonCell: NSButtonCell {
    enum ComponentTokens {
        static let iconSize = CGSize(width: 12, height: 12)
        static let accessorySize = CGSize(width: 8, height: 8)
        static let buttonHeight: CGFloat = 32

        static let spacing = AppPadding.padding8
        static let horizontalPadding = AppPadding.padding8

        static let radius = AppRadius.radius8

        static let backgroundColor = NSColor.Tokens.Surface.primary
        static let highlightedBackgroundColor = NSColor.Tokens.Surface.primary.withAlphaComponent(0.5)
        static let accentColor = NSColor.Tokens.Action.primary
    }

    private let icon: NSImage
    private let accessory: NSImage?

    init(icon: NSImage, title: String, accessory: NSImage? = nil) {
        self.icon = icon
        self.accessory = accessory
        super.init(textCell: title)
    }

    @available(*, unavailable)
    required init(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func drawBezel(withFrame frame: NSRect, in controlView: NSView) {
        let path = NSBezierPath(roundedRect: frame, xRadius: ComponentTokens.radius, yRadius: ComponentTokens.radius)
        if isHighlighted {
            ComponentTokens.highlightedBackgroundColor.setFill()
        } else {
            ComponentTokens.backgroundColor.setFill()
        }
        path.fill()
    }

    override func cellSize(forBounds rect: NSRect) -> NSSize {
        return NSSize(width: rect.width, height: 32)
    }

    override func drawInterior(withFrame cellFrame: NSRect, in controlView: NSView) {
        let insetRect = cellFrame.insetBy(dx: ComponentTokens.horizontalPadding, dy: 0)

        drawImage(
            at: NSPoint(x: insetRect.minX, y: insetRect.midY),
            image: icon,
            withColor: ComponentTokens.accentColor,
            size: ComponentTokens.iconSize
        )
        
        drawTitle(cellFrame: insetRect)
        
        if let accessory {
            drawImage(
                at: NSPoint(x: insetRect.maxX - ComponentTokens.iconSize.width, y: insetRect.midY),
                image: accessory,
                withColor: NSColor.Tokens.Text.tertiary,
                size: ComponentTokens.accessorySize
            )
        }
    }

    private func drawImage(at point: NSPoint, image: NSImage, withColor color: NSColor, size: CGSize) {
        let tintedImage = NSImage(size: size, flipped: true) { rect in
            color.setFill()
            rect.fill()

            image.draw(
                in: rect,
                from: NSRect(origin: .zero, size: image.size),
                operation: .destinationIn,
                fraction: 1.0
            )

            return true
        }

        let iconRect = NSRect(
            x: point.x,
            y: point.y - size.height / 2,
            width: size.width,
            height: size.height
        )
        tintedImage.draw(in: iconRect, from: .zero, operation: .sourceAtop, fraction: 1.0)
    }

    private func drawTitle(cellFrame: NSRect) {
        let xPosition = cellFrame.minX + ComponentTokens.iconSize.width + ComponentTokens.spacing
        let availableWidth = cellFrame.width - xPosition
        let titleRect = NSRect(
            x: xPosition,
            y: cellFrame.minY,
            width: availableWidth,
            height: cellFrame.height
        )

        let paragraphStyle = NSMutableParagraphStyle()
        paragraphStyle.alignment = .left
        paragraphStyle.lineBreakMode = .byTruncatingTail

        let attributes: [NSAttributedString.Key: Any] = [
            .font: NSFont.Tokens.body,
            .foregroundColor: NSColor.Tokens.Text.secondary,
            .paragraphStyle: paragraphStyle
        ]

        let titleSize = title.size(withAttributes: attributes)
        let titleDrawRect = NSRect(
            x: titleRect.origin.x,
            y: titleRect.midY - titleSize.height / 2,
            width: titleRect.width,
            height: titleSize.height
        )

        title.draw(in: titleDrawRect, withAttributes: attributes)
    }
}

public final class LargeButton: NSButton {
    public init(icon: NSImage, title: String, accessory: NSImage? = nil) {
        super.init(frame: .zero)
        cell = LargeButtonCell(icon: icon, title: title, accessory: accessory)
    }

    @available(*, unavailable)
    public required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
}

@available(macOS 14.0, *)
#Preview("No Accessory") {
    LargeButton(icon: KDriveResources.kdriveFoldersStacked.image, title: "Hello, World", accessory: nil)
}

@available(macOS 14.0, *)
#Preview("With Accessory") {
    LargeButton(icon: KDriveResources.kdriveFoldersStacked.image, title: "Hello, World", accessory: KDriveResources.house.image)
}
