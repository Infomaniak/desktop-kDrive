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
import kDriveResources
import Sentry

@MainActor
final class ManyDeletesModalPresenter {
    @LazyInjectService private var manyDeletesCacheObservable: ManyDeletesCacheObservable
    @LazyInjectService private var coherentCache: CoherentCache

    private var bindStore = Set<AnyCancellable>()
    private var displayedSyncDbIds = Set<Int32>()

    init() {
        observeManyDeletes()
    }

    private func observeManyDeletes() {
        manyDeletesCacheObservable.manyDeletesPublisher
            .receiveOnMain(store: &bindStore) { [weak self] notification in
                self?.showManyDeletesAlert(notification)
            }
    }

    private func showManyDeletesAlert(_ notification: ManyDeletesNotification) {
        let syncDbId = notification.syncDbId

        guard notification.notificationType == .SoftLimit || notification.notificationType == .HardLimit else { return }

        guard !displayedSyncDbIds.contains(syncDbId) else { return }
        displayedSyncDbIds.insert(syncDbId)

        Task {
            defer { displayedSyncDbIds.remove(syncDbId) }

            guard let synchro = await coherentCache.getSynchro(synchroDbId: syncDbId) else {
                IKLogger.general.error("[KD] Synchro not found for syncDbId:\(syncDbId)")
                return
            }

            switch notification.notificationType {
            case .SoftLimit:
                runSoftLimitAlert(nbFiles: notification.nbFiles, localPath: synchro.localPath)
            case .HardLimit:
                let userChoice = runHardLimitAlert(nbFiles: notification.nbFiles, localPath: synchro.localPath)
                await acknowledge(syncDbId: syncDbId, userChoice: userChoice)
            default:
                break
            }
        }
    }

    private func runSoftLimitAlert(nbFiles: UInt64, localPath: String) {
        let alert = makeAlert(
            style: .warning,
            title: KDriveLocalizable.manyDeleteDialogTitle(nbFiles),
            message: KDriveLocalizable.manyDeleteDialogSoftLimitContent
        )
        alert.addButton(withTitle: KDriveLocalizable.buttonClose)
        alert.addButton(withTitle: KDriveLocalizable.buttonOpenTrash)

        let response = alert.runModal()

        if response == .alertSecondButtonReturn {
            guard let trashUrl = try? FileManager.default.url(
                for: .trashDirectory,
                in: .userDomainMask,
                appropriateFor: nil,
                create: false
            ) else { return }

            NSWorkspace.shared.open(trashUrl)
        }
    }

    private func runHardLimitAlert(nbFiles: UInt64, localPath: String) -> KDC.TooManyDeletesUserChoice {
        let alert = makeAlert(
            style: .critical,
            title: KDriveLocalizable.manyDeleteDialogTitle(nbFiles),
            message: KDriveLocalizable.manyDeleteDialogHardLimitContent
        )

        let primaryButton = alert.addButton(withTitle: KDriveLocalizable.manyDeleteDialogHardLimitPrimary)
        primaryButton.hasDestructiveAction = true

        alert.addButton(withTitle: KDriveLocalizable.manyDeleteDialogHardLimitSecondary)

        return alert.runModal() == .alertFirstButtonReturn ? .Revert : .Continue
    }

    private func makeAlert(style: NSAlert.Style, title: String, message: String) -> NSAlert {
        let alert = NSAlert()
        alert.alertStyle = style
        alert.messageText = title
        alert.informativeText = message

        (NSApp.delegate as? AppDelegate)?.dockIconManager?.showDockIconAndActivate()

        return alert
    }

    private func acknowledge(syncDbId: Int32, userChoice: KDC.TooManyDeletesUserChoice) async {
        do {
            try await SyncJobs().acknowledgeManyDeletes(syncDbId: syncDbId, userChoice: userChoice)
        } catch {
            SentrySDK.capture(error: error)
            IKLogger.general.error("[KD] Failed to acknowledge many deletes for syncDbId:\(syncDbId): \(error)")
        }
    }
}
