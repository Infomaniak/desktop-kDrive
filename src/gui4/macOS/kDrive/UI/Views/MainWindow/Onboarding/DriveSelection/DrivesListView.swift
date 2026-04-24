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
import kDriveCoreUI
import kDriveResources

class DrivesListView: NSView {
    private lazy var drivesStackView: NSStackView = {
        let stackView = NSStackView()
        stackView.translatesAutoresizingMaskIntoConstraints = false
        stackView.orientation = .vertical
        stackView.alignment = .leading
        stackView.spacing = AppPadding.padding8
        return stackView
    }()

    private(set) var cells = [Int: DriveCellView]()

    var selectedDrives = Set<UIAvailableDrive>() {
        didSet {
            updateSelectedCells(newValue: selectedDrives, oldValue: oldValue)
        }
    }

    var drives: [UIAvailableDrive] = [] {
        didSet {
            updateDrivesList()
        }
    }

    var toggleDrive: ((UIAvailableDrive) -> Void)?

    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        setupView()
    }

    required init?(coder: NSCoder) {
        super.init(coder: coder)
        setupView()
    }

    private func setupView() {
        let titleLabel = NSTextField(labelWithString: KDriveLocalizable.onboardingDriveSelectionSelectTitle)
        titleLabel.translatesAutoresizingMaskIntoConstraints = false
        titleLabel.font = NSFont.Tokens.subheadline
        titleLabel.textColor = ColorToken.Text.tertiary.asNSColor

        let stackView = NSStackView(views: [titleLabel, drivesStackView])
        stackView.translatesAutoresizingMaskIntoConstraints = false
        stackView.orientation = .vertical
        stackView.alignment = .leading
        stackView.spacing = AppPadding.padding12
        addSubview(stackView)

        NSLayoutConstraint.activate([
            stackView.leadingAnchor.constraint(equalTo: leadingAnchor),
            stackView.trailingAnchor.constraint(equalTo: trailingAnchor),
            stackView.topAnchor.constraint(equalTo: topAnchor),
            stackView.bottomAnchor.constraint(equalTo: bottomAnchor)
        ])
    }

    private func updateDrivesList() {
        for view in drivesStackView.views {
            drivesStackView.removeView(view)
            cells = [:]
        }

        for drive in drives {
            let cell = DriveCellView(drive: drive)
            cell.translatesAutoresizingMaskIntoConstraints = false
            cell.toggleDrive = toggleDrive
            drivesStackView.addArrangedSubview(cell)

            cells[drive.id] = cell
            if selectedDrives.contains(drive) {
                cell.state = .on
            } else {
                cell.state = .off
            }
        }
    }

    private func updateSelectedCells(newValue: Set<UIAvailableDrive>, oldValue: Set<UIAvailableDrive>) {
        let deselectedDrives = oldValue.subtracting(newValue)
        let selectedDrives = newValue.subtracting(oldValue)

        for drive in deselectedDrives {
            cells[drive.id]?.state = .off
        }

        for drive in selectedDrives {
            cells[drive.id]?.state = .on
        }
    }
}

@available(macOS 14.0, *)
#Preview {
    let drivesList = DrivesListView()
    drivesList.drives = [PreviewHelper.availableDrive1, PreviewHelper.availableDrive2]
    return drivesList
}
