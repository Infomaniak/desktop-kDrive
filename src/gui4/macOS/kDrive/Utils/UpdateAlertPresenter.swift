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
final class UpdateAlertPresenter {
    @LazyInjectService private var updaterCacheObservable: UpdaterCacheObservable

    private var bindStore = Set<AnyCancellable>()
    private var isShowingAlert = false

    init() {
        observeShowUpdateDialog()
    }

    private func observeShowUpdateDialog() {
        updaterCacheObservable.showUpdateDialogPublisher
            .receiveOnMain(store: &bindStore) { [weak self] versionInfo in
                self?.showUpdateAlert(versionInfo: versionInfo)
            }
    }

    private func showUpdateAlert(versionInfo: VersionInfo) {
        guard !isShowingAlert else { return }
        isShowingAlert = true

        let alert = NSAlert()
        alert.alertStyle = .informational
        alert.messageText = "Update available"
        alert.informativeText = "kDrive \(versionInfo.tag) is available. Installation takes less than a minute."
        alert.icon = NSImage(named: "AppIcon")

        alert.addButton(withTitle: "Install now")
        alert.addButton(withTitle: "Remind me later")
        alert.addButton(withTitle: "Ignore this version")

        let response = alert.runModal()
        isShowingAlert = false

        switch response {
        case .alertFirstButtonReturn:
            installUpdate()
        case .alertThirdButtonReturn:
            skipVersion(versionInfo.tag)
        default:
            break
        }
    }

    private func installUpdate() {
        Task {
            do {
                try await UpdaterJobs().startInstaller()
            } catch {
                IKLogger.general.error("[KD] Failed to start installer: \(error)")
            }
        }
    }

    private func skipVersion(_ version: String) {
        Task {
            do {
                try await UpdaterJobs().skipVersion(version: version)
            } catch {
                IKLogger.general.error("[KD] Failed to skip version: \(error)")
            }
        }
    }
}
