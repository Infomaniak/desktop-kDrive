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

    private lazy var scrollView: NSScrollView = {
        let scrollView = NSScrollView()
        scrollView.translatesAutoresizingMaskIntoConstraints = false
        scrollView.hasVerticalScroller = true
        scrollView.hasHorizontalScroller = false
        scrollView.autohidesScrollers = true
        scrollView.borderType = .noBorder
        scrollView.drawsBackground = false
        return scrollView
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

        // Configure scroll view with document view
        scrollView.documentView = drivesStackView

        let stackView = NSStackView(views: [titleLabel, scrollView])
        stackView.translatesAutoresizingMaskIntoConstraints = false
        stackView.orientation = .vertical
        stackView.alignment = .leading
        stackView.spacing = AppPadding.padding12
        addSubview(stackView)

        // Calculate height for approximately 2.5 cells
        // Each cell is ~36pt tall (DriveSquareView 20pt + 8pt top + 8pt bottom padding)
        // Plus spacing of 8pt between cells
        // For 2.5 cells: (36 * 2.5) + (8 * 1.5) = 102pt
        let preferredScrollViewHeight: CGFloat = 110

        NSLayoutConstraint.activate([
            stackView.leadingAnchor.constraint(equalTo: leadingAnchor),
            stackView.trailingAnchor.constraint(equalTo: trailingAnchor),
            stackView.topAnchor.constraint(equalTo: topAnchor),
            stackView.bottomAnchor.constraint(equalTo: bottomAnchor),

            // ScrollView height constraint to show ~2.5 cells
            scrollView.heightAnchor.constraint(equalToConstant: preferredScrollViewHeight),

            // Ensure drivesStackView fits within scrollView width
            drivesStackView.leadingAnchor.constraint(equalTo: scrollView.leadingAnchor),
            drivesStackView.trailingAnchor.constraint(equalTo: scrollView.trailingAnchor)
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

        scrollToTop()
    }

    private func scrollToTop() {
        guard let documentView = scrollView.documentView else { return }

        layoutSubtreeIfNeeded()
        documentView.layoutSubtreeIfNeeded()

        let yPosition = documentView.isFlipped
            ? 0
            : max(0, documentView.bounds.height - scrollView.contentView.bounds.height)

        let topPoint = NSPoint(x: 0, y: yPosition)
        scrollView.contentView.scroll(to: topPoint)
        scrollView.reflectScrolledClipView(scrollView.contentView)
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
