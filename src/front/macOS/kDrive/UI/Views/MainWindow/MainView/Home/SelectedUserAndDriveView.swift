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
import kDriveCoreUI

final class SelectedUserAndDriveView: NSView {
    var user: UIUser {
        didSet {
            updateUser()
        }
    }

    var drive: UIDrive {
        didSet {
            updateDrive()
        }
    }

    init(user: UIUser, drive: UIDrive) {
        self.user = user
        self.drive = drive
        super.init(frame: .zero)
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    private func setupView() {

    }

    private func updateUser() {

    }

    private func updateDrive() {

    }
}

@available(macOS 14.0, *)
#Preview {
    SelectedUserAndDriveView(user: PreviewHelper.user, drive: PreviewHelper.drive1)
}
