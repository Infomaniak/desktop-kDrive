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

import Cocoa
import kDriveResources

public final class DriveSquareView: NSView {
    static let iconSize = NSSize(width: 12, height: 12)

    private let color: NSColor

    private let iconView: NSImageView = {
        let imageView = NSImageView(image: KDriveResources.kdriveFoldersStacked.image)
        imageView.contentTintColor = .white
        imageView.translatesAutoresizingMaskIntoConstraints = false
        return imageView
    }()

    override public var intrinsicContentSize: NSSize {
        return NSSize(width: 20, height: 20)
    }

    public init(color: NSColor) {
        self.color = color
        super.init(frame: .zero)
        setupView()
    }

    @available(*, unavailable)
    public required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override public func draw(_ dirtyRect: NSRect) {
        if #unavailable(macOS 26.0) {
            let path = NSBezierPath(roundedRect: bounds, xRadius: AppRadius.radius4, yRadius: AppRadius.radius4)
            color.setFill()
            path.fill()
        }
    }

    private func setupView() {
        if #available(macOS 26.0, *) {
            let contentView = NSView()
            contentView.addSubview(iconView)

            let backgroundView = NSGlassEffectView()
            backgroundView.translatesAutoresizingMaskIntoConstraints = false
            backgroundView.contentView = contentView
            backgroundView.tintColor = color
            backgroundView.cornerRadius = AppRadius.radius4
            addSubview(backgroundView)

            NSLayoutConstraint.activate([
                backgroundView.widthAnchor.constraint(equalTo: widthAnchor),
                backgroundView.heightAnchor.constraint(equalTo: heightAnchor),
                iconView.widthAnchor.constraint(equalToConstant: Self.iconSize.width),
                iconView.heightAnchor.constraint(equalToConstant: Self.iconSize.height),
                iconView.centerXAnchor.constraint(equalTo: contentView.centerXAnchor),
                iconView.centerYAnchor.constraint(equalTo: contentView.centerYAnchor)
            ])
        } else {
            addSubview(iconView)
            NSLayoutConstraint.activate([
                iconView.widthAnchor.constraint(equalToConstant: Self.iconSize.width),
                iconView.heightAnchor.constraint(equalToConstant: Self.iconSize.height),
                iconView.centerXAnchor.constraint(equalTo: centerXAnchor),
                iconView.centerYAnchor.constraint(equalTo: centerYAnchor)
            ])
        }
    }
}

@available(macOS 14.0, *)
#Preview {
    let driveSquareView = DriveSquareView(color: NSColor(red: 0, green: 150, blue: 136, alpha: 1))
    return driveSquareView
}
