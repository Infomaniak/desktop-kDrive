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
import Combine
import InfomaniakDI
import kDriveCore

@MainActor
final class UpdateModalPresenter {
    @LazyInjectService private var updaterCacheObservable: UpdaterCacheObservable

    private var bindStore = Set<AnyCancellable>()
    private var updateDialogWindowController: UpdateDialogWindowController?

    init() {
        observeShowUpdateDialog()
    }

    private func observeShowUpdateDialog() {
        updaterCacheObservable.showUpdateDialogPublisher
            .receiveOnMain(store: &bindStore) { [weak self] versionInfo in
                self?.showUpdateDialog(versionInfo: versionInfo)
            }
    }

    func showUpdateDialog(versionInfo: VersionInfo) {
        guard updateDialogWindowController == nil else { return }

        let windowController = UpdateDialogWindowController(versionInfo: versionInfo) { [weak self] in
            self?.updateDialogWindowController = nil
        }
        updateDialogWindowController = windowController
        updateDialogWindowController?.present()
    }
}
