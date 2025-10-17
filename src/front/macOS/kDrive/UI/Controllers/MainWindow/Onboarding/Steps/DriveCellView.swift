//
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
import kDriveCore

final class DriveCellView: NSView {
    let color: NSColor
    let title: String
    let subtitle: String?

    init(color: NSColor, title: String, subtitle: String? = nil) {
        self.color = color
        self.title = title
        self.subtitle = subtitle

        super.init(frame: .zero)
        setupView()
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    private func setupView() {
        let checkbox = NSButton()
        checkbox.translatesAutoresizingMaskIntoConstraints = false
        checkbox.setButtonType(.switch)
        addSubview(checkbox)
    }
}

@available(macOS 14.0, *)
#Preview {
    let driveCell = DriveCellView(color: .systemBlue, title: "Mon entreprise")
    driveCell.frame = NSRect(x: 0, y: 0, width: 300, height: 35)
    return driveCell
}
