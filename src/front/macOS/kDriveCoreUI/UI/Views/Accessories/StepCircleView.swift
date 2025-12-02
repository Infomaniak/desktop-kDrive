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

public final class StepCircleView: NSView {
    public var color = NSColor.Tokens.Status.Medium.security {
        didSet {
            if #available(macOS 26.0, *) {
                let backgroundView = self.backgroundView as? NSGlassEffectView
                backgroundView?.tintColor = color
            } else {
                needsDisplay = true
            }
        }
    }

    public var step: Int {
        didSet {
            stepLabel.stringValue = "\(step)"
        }
    }

    private lazy var stepLabel: NSTextField = {
        let textField = NSTextField(labelWithString: "\(step)")
        textField.translatesAutoresizingMaskIntoConstraints = false
        textField.alignment = .center
        textField.maximumNumberOfLines = 1
        textField.font = NSFont.Tokens.subheadline
        textField.textColor = .white
        return textField
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

    public init(step: Int) {
        self.step = step
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
            contentView.addSubview(stepLabel)

            backgroundView.contentView = contentView
            addSubview(backgroundView)

            NSLayoutConstraint.activate([
                backgroundView.widthAnchor.constraint(equalTo: widthAnchor),
                backgroundView.heightAnchor.constraint(equalTo: heightAnchor),
                stepLabel.widthAnchor.constraint(equalToConstant: intrinsicContentSize.width),
                stepLabel.heightAnchor.constraint(lessThanOrEqualToConstant: intrinsicContentSize.height),
                stepLabel.centerXAnchor.constraint(equalTo: contentView.centerXAnchor),
                stepLabel.centerYAnchor.constraint(equalTo: contentView.centerYAnchor)
            ])
        } else {
            addSubview(stepLabel)
            NSLayoutConstraint.activate([
                stepLabel.widthAnchor.constraint(equalTo: widthAnchor),
                stepLabel.heightAnchor.constraint(lessThanOrEqualToConstant: intrinsicContentSize.height),
                stepLabel.centerXAnchor.constraint(equalTo: centerXAnchor),
                stepLabel.centerYAnchor.constraint(equalTo: centerYAnchor)
            ])
        }
    }
}

@available(macOS 14.0, *)
#Preview {
    StepCircleView(step: 1)
}
