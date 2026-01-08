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

public final class LoadingLabelView: NSView {
    public var stringValue: String {
        didSet {
            label.stringValue = stringValue
        }
    }

    private lazy var label: NSTextField = {
        let label = NSTextField(labelWithString: stringValue)
        label.translatesAutoresizingMaskIntoConstraints = false
        label.font = NSFont.Tokens.body
        label.textColor = ColorToken.Text.tertiary.asNSColor
        return label
    }()

    public init(text: String) {
        self.stringValue = text
        super.init(frame: .zero)
        setupView()
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    private func setupView() {
        let progressIndicator = NSProgressIndicator()
        progressIndicator.translatesAutoresizingMaskIntoConstraints = false
        progressIndicator.isIndeterminate = true
        progressIndicator.style = .spinning
        progressIndicator.controlSize = .small
        progressIndicator.startAnimation(nil)

        let stackView = NSStackView(views: [progressIndicator, label])
        stackView.translatesAutoresizingMaskIntoConstraints = false
        stackView.alignment = .centerY
        addSubview(stackView)

        NSLayoutConstraint.activate([
            stackView.topAnchor.constraint(equalTo: topAnchor),
            stackView.bottomAnchor.constraint(equalTo: bottomAnchor),
            stackView.leadingAnchor.constraint(equalTo: leadingAnchor),
            stackView.trailingAnchor.constraint(equalTo: trailingAnchor)
        ])
    }
}

@available(macOS 14.0, *)
#Preview {
    LoadingLabelView(text: "Hello, World!")
}
