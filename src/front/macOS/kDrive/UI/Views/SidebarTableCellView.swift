/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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

final class SidebarTableCellView: NSTableCellView {
    var badge: Int? {
        didSet {
            updateBadge()
        }
    }

    private var badgeView: NSButton!

    convenience init() {
        self.init(frame: .zero)
    }

    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        setupCell()
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    private func setupCell() {
        let cellImageView = NSImageView()
        imageView = cellImageView
        addSubview(cellImageView)

        let cellTextField = NSTextField()
        cellTextField.isSelectable = false
        cellTextField.isBezeled = false
        cellTextField.drawsBackground = false
        cellTextField.maximumNumberOfLines = 1
        cellTextField.lineBreakMode = .byTruncatingTail
        textField = cellTextField
        addSubview(cellTextField)

        badgeView = BadgeButton()
        badgeView.isHidden = true
        badgeView.autoresizingMask = [.minXMargin, .minYMargin, .maxYMargin]
        addSubview(badgeView)
    }

    override func layout() {
        super.layout()
        layoutBadge()
    }

    private func updateBadge() {
        if let badge {
            badgeView.isHidden = false
            badgeView.title = "\(badge)"
        } else {
            badgeView.isHidden = true
        }
    }

    private func layoutBadge() {
        guard badge != nil else { return }

        let badgeSize = badgeView.intrinsicContentSize

        let x = bounds.width - badgeSize.width
        let y = (bounds.height - badgeSize.height) / 2

        badgeView.frame = NSRect(x: x, y: y, width: badgeSize.width, height: badgeSize.height)

        if let textField, textField.frame.maxX > badgeView.frame.minX {
            let margin: CGFloat = 8.0

            let maxWidth = textField.frame.width - badgeSize.width - margin
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
