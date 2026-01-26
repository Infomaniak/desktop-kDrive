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

public final class IconCircleView: NSView {
    static let iconSize = NSSize(width: 12, height: 12)

    public var color = ColorToken.Status.Medium.security.asNSColor {
        didSet {
            if #available(macOS 26.0, *) {
                let backgroundView = backgroundView as? NSGlassEffectView
                backgroundView?.tintColor = color
            } else {
                needsDisplay = true
            }
        }
    }

    let icon: NSImage

    private lazy var iconView: NSImageView = {
        let imageView = NSImageView(image: icon)
        imageView.translatesAutoresizingMaskIntoConstraints = false
        imageView.contentTintColor = .white
        return imageView
    }()

    private lazy var backgroundView: NSView? = {
        guard #available(macOS 26.0, *) else { return nil }

        let backgroundView = NSGlassEffectView()
        backgroundView.translatesAutoresizingMaskIntoConstraints = false
        backgroundView.tintColor = color
        backgroundView.cornerRadius = intrinsicContentSize.width / 2
        return backgroundView
    }()

    override public var intrinsicContentSize: NSSize {
        return NSSize(width: 20, height: 20)
    }

    public init(icon: NSImage) {
        self.icon = icon
        super.init(frame: .zero)

        setupView()
    }

    @available(*, unavailable)
    public required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override public func draw(_ dirtyRect: NSRect) {
        if #unavailable(macOS 26.0) {
            let path = NSBezierPath(ovalIn: NSRect(origin: .zero, size: intrinsicContentSize))
            color.setFill()
            path.fill()
        }
    }

    private func setupView() {
        if #available(macOS 26.0, *) {
            guard let backgroundView = backgroundView as? NSGlassEffectView else { return }

            let contentView = NSView()
            contentView.addSubview(iconView)

            backgroundView.contentView = contentView
            addSubview(backgroundView)

            NSLayoutConstraint.activate([
                backgroundView.widthAnchor.constraint(equalTo: widthAnchor),
                backgroundView.heightAnchor.constraint(equalTo: heightAnchor),
                iconView.widthAnchor.constraint(equalToConstant: IconCircleView.iconSize.width),
                iconView.heightAnchor.constraint(equalToConstant: IconCircleView.iconSize.height),
                iconView.centerXAnchor.constraint(equalTo: contentView.centerXAnchor),
                iconView.centerYAnchor.constraint(equalTo: contentView.centerYAnchor)
            ])
        } else {
            addSubview(iconView)
            NSLayoutConstraint.activate([
                iconView.widthAnchor.constraint(equalToConstant: IconCircleView.iconSize.width),
                iconView.heightAnchor.constraint(equalToConstant: IconCircleView.iconSize.height),
                iconView.centerXAnchor.constraint(equalTo: centerXAnchor),
                iconView.centerYAnchor.constraint(equalTo: centerYAnchor)
            ])
        }
    }
}

@available(macOS 14.0, *)
#Preview {
    IconCircleView(icon: KDriveResources.checkmark.image)
}
