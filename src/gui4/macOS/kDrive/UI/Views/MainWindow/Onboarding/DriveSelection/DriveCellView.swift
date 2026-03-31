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

final class NonInteractiveButton: NSButton {
    override func hitTest(_ point: NSPoint) -> NSView? {
        return nil
    }
}

final class DriveCellView: NSView {
    enum Tokens {
        static let backgroundColor = ColorToken.Surface.secondary.asNSColor
        static let activatedBackgroundColor = ColorToken.Surface.tertiary.asNSColor
    }

    let drive: UIAvailableDrive
    var toggleDrive: ((UIAvailableDrive) -> Void)?

    var state: NSControl.StateValue {
        get { checkbox.state }
        set { checkbox.state = newValue }
    }

    var isEnabled = true {
        didSet {
            checkbox.isEnabled = isEnabled
        }
    }

    private var color: NSColor {
        return drive.nsColor ?? ColorToken.Drive.defaultColor.asNSColor
    }

    private var isActivated = false

    private var backgroundColor: NSColor {
        isActivated ? Tokens.activatedBackgroundColor : Tokens.backgroundColor
    }

    private lazy var checkbox: NSButton = {
        let checkboxButton = NonInteractiveButton(checkboxWithTitle: "", target: nil, action: nil)
        checkboxButton.translatesAutoresizingMaskIntoConstraints = false
        checkboxButton.refusesFirstResponder = true
        return checkboxButton
    }()

    private lazy var driveIcon: DriveSquareView = {
        let driveSquareView = DriveSquareView(color: color)
        driveSquareView.translatesAutoresizingMaskIntoConstraints = false
        return driveSquareView
    }()

    private lazy var titleLabel: NSTextField = {
        let textField = NSTextField(labelWithString: drive.name)
        textField.translatesAutoresizingMaskIntoConstraints = false
        textField.font = NSFont.Tokens.body
        textField.textColor = ColorToken.Text.secondary.asNSColor
        return textField
    }()

    init(drive: UIAvailableDrive) {
        self.drive = drive
        super.init(frame: .zero)
        setupView()
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func draw(_ dirtyRect: NSRect) {
        let path = NSBezierPath(roundedRect: bounds, xRadius: AppRadius.radius8, yRadius: AppRadius.radius8)
        backgroundColor.setFill()
        path.fill()
    }

    private func setupView() {
        addSubview(checkbox)
        addSubview(driveIcon)
        addSubview(titleLabel)

        NSLayoutConstraint.activate([
            checkbox.centerYAnchor.constraint(equalTo: titleLabel.centerYAnchor),
            checkbox.leadingAnchor.constraint(equalTo: leadingAnchor, constant: AppPadding.padding8),

            driveIcon.leadingAnchor.constraint(equalTo: checkbox.trailingAnchor, constant: AppPadding.padding8),
            driveIcon.topAnchor.constraint(equalTo: topAnchor, constant: AppPadding.padding8),
            driveIcon.bottomAnchor.constraint(equalTo: bottomAnchor, constant: -AppPadding.padding8),

            titleLabel.centerYAnchor.constraint(equalTo: driveIcon.centerYAnchor),
            titleLabel.leadingAnchor.constraint(equalTo: driveIcon.trailingAnchor, constant: AppPadding.padding8),
            titleLabel.trailingAnchor.constraint(lessThanOrEqualTo: trailingAnchor, constant: -AppPadding.padding8),

            widthAnchor.constraint(equalToConstant: 264)
        ])
    }

    override func mouseDown(with event: NSEvent) {
        guard isEnabled else { return }

        isActivated = true
        needsDisplay = true
    }

    override func mouseUp(with event: NSEvent) {
        guard isEnabled else { return }

        isActivated = false
        needsDisplay = true

        toggleDrive?(drive)
    }
}

@available(macOS 14.0, *)
#Preview("Drive") {
    DriveCellView(drive: PreviewHelper.availableDrive1)
}

@available(macOS 14.0, *)
#Preview("Activated == true / isEnabled == false") {
    let cell = DriveCellView(drive: PreviewHelper.availableDrive1)
    cell.state = .on
    cell.isEnabled = false
    return cell
}
