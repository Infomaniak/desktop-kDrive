/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import Cocoa

public final class SidebarTableCellView: NSTableCellView {
    static let badgeSize: CGFloat = 8

    public var showsBadge = false {
        didSet {
            updateBadge()
        }
    }

    private var badgeView: CircleBadgeView!

    override public var backgroundStyle: NSView.BackgroundStyle {
        didSet {
            badgeView.isEmphasized = backgroundStyle == .emphasized
        }
    }

    public convenience init() {
        self.init(frame: .zero)
    }

    override public init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        setupCell()
    }

    @available(*, unavailable)
    public required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override public func layout() {
        super.layout()
        layoutBadge()
    }

    public func setupForItem(_ sidebarItem: SidebarItem, enabled: Bool = true) {
        imageView?.image = sidebarItem.icon
        textField?.stringValue = sidebarItem.title

        imageView?.contentTintColor = enabled ? nil : ColorToken.Action.Disabled.dimQuaternary.asNSColor
        textField?.textColor = enabled ? nil : ColorToken.Action.Disabled.dimQuaternary.asNSColor
    }

    private func setupCell() {
        let cellImageView = NSImageView()
        imageView = cellImageView
        addSubview(cellImageView)

        let cellTextField = NSTextField(labelWithString: "")
        cellTextField.maximumNumberOfLines = 1
        cellTextField.lineBreakMode = .byTruncatingTail
        textField = cellTextField
        addSubview(cellTextField)

        badgeView = CircleBadgeView()
        badgeView.isHidden = true
        badgeView.autoresizingMask = [.minXMargin, .minYMargin, .maxYMargin]
        addSubview(badgeView)
    }

    private func updateBadge() {
        badgeView.isHidden = !showsBadge
        needsLayout = true
    }

    private func layoutBadge() {
        guard showsBadge else { return }

        let trailingPadding = AppPadding.padding4
        let x = bounds.width - Self.badgeSize - trailingPadding
        let y = (bounds.height - Self.badgeSize) / 2

        badgeView.frame = NSRect(x: x, y: y, width: Self.badgeSize, height: Self.badgeSize)

        if let textField, textField.frame.maxX > badgeView.frame.minX {
            let spacing = AppPadding.padding8

            let maxWidth = textField.frame.width - Self.badgeSize - spacing - trailingPadding
            let currentFrame = textField.frame
            textField.frame = NSRect(
                x: currentFrame.minX,
                y: currentFrame.minY,
                width: maxWidth,
                height: currentFrame.height
            )
        }
    }
}

private final class CircleBadgeView: NSView {
    var isEmphasized = false {
        didSet {
            guard isEmphasized != oldValue else { return }
            needsDisplay = true
        }
    }

    override func draw(_ dirtyRect: NSRect) {
        super.draw(dirtyRect)
        let color = isEmphasized ? NSColor.white : ColorToken.Accent.primary.asNSColor
        color.setFill()
        NSBezierPath(ovalIn: bounds).fill()
    }
}
