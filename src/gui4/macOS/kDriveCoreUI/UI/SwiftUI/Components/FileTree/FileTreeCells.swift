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
import kDriveResources

final class FileTreeHeaderView: NSTableHeaderView {
    let checkbox = NSButton()

    var checkboxColumnIndex = 0

    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        checkbox.setButtonType(.switch)
        checkbox.allowsMixedState = true
        checkbox.title = ""
        addSubview(checkbox)
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func layout() {
        super.layout()
        guard tableView != nil, checkboxColumnIndex >= 0 else { return }

        let columnRect = headerRect(ofColumn: checkboxColumnIndex)
        let size = checkbox.intrinsicContentSize
        checkbox.frame = NSRect(
            x: columnRect.midX - size.width / 2,
            y: columnRect.midY - size.height / 2,
            width: size.width,
            height: size.height
        )
    }
}

final class FileTreeCheckboxCell: NSTableCellView {
    private let checkbox = NSButton()

    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        checkbox.setButtonType(.switch)
        checkbox.allowsMixedState = true
        checkbox.title = ""
        checkbox.translatesAutoresizingMaskIntoConstraints = false
        addSubview(checkbox)

        NSLayoutConstraint.activate([
            checkbox.centerXAnchor.constraint(equalTo: centerXAnchor),
            checkbox.centerYAnchor.constraint(equalTo: centerYAnchor)
        ])
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    func configure(state: NSControl.StateValue, isEnabled: Bool, target: AnyObject, action: Selector) {
        checkbox.state = state
        checkbox.isEnabled = isEnabled
        checkbox.target = target
        checkbox.action = action
    }
}

final class FileTreeNameCell: NSTableCellView {
    private let icon = NSImageView()
    private let label = NSTextField(labelWithString: "")

    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)

        icon.imageScaling = .scaleProportionallyUpOrDown
        icon.translatesAutoresizingMaskIntoConstraints = false

        label.lineBreakMode = .byTruncatingMiddle
        label.translatesAutoresizingMaskIntoConstraints = false
        label.font = .systemFont(ofSize: NSFont.systemFontSize)

        addSubview(icon)
        addSubview(label)

        imageView = icon
        textField = label

        NSLayoutConstraint.activate([
            icon.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 2),
            icon.centerYAnchor.constraint(equalTo: centerYAnchor),
            icon.widthAnchor.constraint(equalToConstant: AppIconSize.iconSize16.width),
            icon.heightAnchor.constraint(equalToConstant: AppIconSize.iconSize16.height),

            label.leadingAnchor.constraint(equalTo: icon.trailingAnchor, constant: AppPadding.padding8),
            label.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -AppPadding.padding4),
            label.centerYAnchor.constraint(equalTo: centerYAnchor)
        ])
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    func configure(with item: FileTreeItem) {
        if item.isFolder {
            icon.image = KDriveResources.folderFilled.image
        } else {
            let pathExtension = (item.name as NSString).pathExtension
            let fileRepresentation = FileTypeRepresentation(filenameExtension: pathExtension)
            icon.image = fileRepresentation.icon.image.withSymbolConfiguration(.init(paletteColors: [
                fileRepresentation.color.color
            ]))
        }
        label.stringValue = item.name
        label.textColor = item.isEnabled ? .labelColor : .disabledControlTextColor
        alphaValue = item.isEnabled ? 1 : 0.5
    }
}

final class FileTreeSizeCell: NSTableCellView {
    private let label = NSTextField(labelWithString: "")

    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)

        label.translatesAutoresizingMaskIntoConstraints = false
        label.font = .systemFont(ofSize: NSFont.systemFontSize)

        addSubview(label)
        textField = label

        NSLayoutConstraint.activate([
            label.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 4),
            label.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -4),
            label.centerYAnchor.constraint(equalTo: centerYAnchor)
        ])
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    func configure(with item: FileTreeItem) {
        if let size = item.size {
            label.stringValue = ByteCountFormatter.string(fromByteCount: size, countStyle: .file)
        } else {
            label.stringValue = "-"
        }
        label.textColor = item.isEnabled ? .secondaryLabelColor : .disabledControlTextColor
    }
}

final class FileTreeShimmerCell: NSTableCellView {
    private let shimmer = ShimmerView()

    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)

        shimmer.translatesAutoresizingMaskIntoConstraints = false
        addSubview(shimmer)

        NSLayoutConstraint.activate([
            shimmer.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 2),
            shimmer.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -8),
            shimmer.centerYAnchor.constraint(equalTo: centerYAnchor),
            shimmer.heightAnchor.constraint(equalToConstant: 12)
        ])
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
}

final class ShimmerView: NSView {
    private let gradientLayer = CAGradientLayer()

    private static let animationKey = "shimmer"

    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        wantsLayer = true
        layer?.cornerRadius = 6
        layer?.masksToBounds = true

        gradientLayer.startPoint = CGPoint(x: 0, y: 0.5)
        gradientLayer.endPoint = CGPoint(x: 1, y: 0.5)
        gradientLayer.locations = [0, 0.5, 1]
        layer?.addSublayer(gradientLayer)
        updateColors()
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func layout() {
        super.layout()
        gradientLayer.frame = bounds
    }

    override func viewDidChangeEffectiveAppearance() {
        super.viewDidChangeEffectiveAppearance()
        updateColors()
    }

    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()
        if window == nil {
            gradientLayer.removeAnimation(forKey: Self.animationKey)
        } else {
            startAnimating()
        }
    }

    private func updateColors() {
        effectiveAppearance.performAsCurrentDrawingAppearance {
            let base = NSColor.quaternaryLabelColor.cgColor
            let highlight = NSColor.tertiaryLabelColor.cgColor
            gradientLayer.colors = [base, highlight, base]
        }
    }

    private func startAnimating() {
        guard gradientLayer.animation(forKey: Self.animationKey) == nil else { return }
        let animation = CABasicAnimation(keyPath: "locations")
        animation.fromValue = [-1.0, -0.5, 0.0]
        animation.toValue = [1.0, 1.5, 2.0]
        animation.duration = 1.2
        animation.repeatCount = .infinity
        gradientLayer.add(animation, forKey: Self.animationKey)
    }
}
