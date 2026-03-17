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
import kDriveCoreUI
import kDriveResources

extension MacOSPermissionState {
    var color: NSColor {
        switch self {
        case .neutral:
            return ColorToken.Status.Medium.security.asNSColor
        case .warning:
            return ColorToken.Status.Medium.warning.asNSColor
        case .done:
            return ColorToken.Status.Medium.success.asNSColor
        }
    }
}

class PermissionInstructionCell: NSView {
    var state: MacOSPermissionState = .neutral {
        didSet {
            updateState(state: state)
        }
    }

    var step: Int {
        didSet {
            stepCircleView.step = step
        }
    }

    var title: NSMutableAttributedString {
        didSet {
            titleLabel.attributedStringValue = title
        }
    }

    var hint: String? {
        didSet {
            hintLabel.stringValue = hint ?? ""
        }
    }

    private lazy var stepCircleView: StepCircleView = {
        let view = StepCircleView(step: step)
        view.translatesAutoresizingMaskIntoConstraints = false
        view.color = state.color
        return view
    }()

    private lazy var iconCircleView: IconCircleView = {
        let view = IconCircleView(icon: KDriveResources.checkmark.image)
        view.translatesAutoresizingMaskIntoConstraints = false
        view.color = state.color
        return view
    }()

    private lazy var titleLabel: NSTextField = {
        let textField = NSTextField(labelWithAttributedString: title)
        textField.translatesAutoresizingMaskIntoConstraints = false
        textField.usesSingleLineMode = false
        textField.allowsEditingTextAttributes = true
        textField.isSelectable = true
        return textField
    }()

    lazy var hintLabel: NSTextField = {
        let textField = NSTextField(labelWithString: hint ?? "")
        textField.translatesAutoresizingMaskIntoConstraints = false
        textField.usesSingleLineMode = false
        textField.textColor = ColorToken.Text.tertiary.asNSColor
        textField.font = NSFont.Tokens.subheadline
        textField.isHidden = hint == nil
        return textField
    }()

    init(step: Int, title: NSMutableAttributedString) {
        self.step = step
        self.title = title
        super.init(frame: .zero)

        setupView()
        updateState(state: state)
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    private func setupView() {
        let labelsStack = NSStackView(views: [titleLabel, hintLabel])
        labelsStack.orientation = .vertical
        labelsStack.alignment = .leading
        labelsStack.spacing = AppPadding.padding2

        let stackView = NSStackView(views: [stepCircleView, iconCircleView, labelsStack])
        stackView.translatesAutoresizingMaskIntoConstraints = false
        stackView.spacing = AppPadding.padding8
        stackView.alignment = .centerY
        addSubview(stackView)

        NSLayoutConstraint.activate([
            stackView.heightAnchor.constraint(greaterThanOrEqualToConstant: 42),
            stackView.leadingAnchor.constraint(equalTo: leadingAnchor),
            stackView.trailingAnchor.constraint(equalTo: trailingAnchor),
            stackView.topAnchor.constraint(equalTo: topAnchor),
            stackView.bottomAnchor.constraint(equalTo: bottomAnchor)
        ])
    }

    private func updateState(state: MacOSPermissionState) {
        stepCircleView.color = state.color
        iconCircleView.color = state.color

        switch state {
        case .neutral, .warning:
            stepCircleView.isHidden = false
            iconCircleView.isHidden = true
        case .done:
            stepCircleView.isHidden = true
            iconCircleView.isHidden = false
        }
    }
}

@available(macOS 14.0, *)
#Preview("Neutral") {
    PermissionInstructionCell(step: 1, title: .init("Sélectionnez Ouverture et extensions > Extensions de sécurité"))
}

@available(macOS 14.0, *)
#Preview("Warning") {
    let permissionCell = PermissionInstructionCell(
        step: 1,
        title: .init("Sélectionnez Ouverture et extensions > Extensions de sécurité")
    )
    permissionCell.hint = "Vous devez activer les autorisations avant de continuer"
    permissionCell.hintLabel.isHidden = false
    permissionCell.state = .warning
    return permissionCell
}

@available(macOS 14.0, *)
#Preview("Done") {
    let permissionCell = PermissionInstructionCell(
        step: 1,
        title: .init("Sélectionnez Ouverture et extensions > Extensions de sécurité")
    )
    permissionCell.state = .done
    return permissionCell
}
