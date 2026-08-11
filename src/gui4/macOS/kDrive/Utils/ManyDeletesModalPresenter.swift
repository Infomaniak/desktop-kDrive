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

    private var pendingNotifications = [ManyDeletesNotification]()
    private var presentedNotification: ManyDeletesNotification?
    private var currentParentWindow: NSWindow?
    private var currentSheet: NSWindow?

    init() {
        observeManyDeletes()
    }

    private func observeManyDeletes() {
        manyDeletesCacheObservable.manyDeletesPublisher
            .receive(on: DispatchQueue.main)
            .sink { [weak self] notification in
                self?.enqueue(notification)
            }
            .store(in: &bindStore)
    }

    private func severity(of notificationType: KDC.TooManyDeletesNotificationType) -> Int {
        switch notificationType {
        case .HardLimit:
            return 2
        case .SoftLimit:
            return 1
        default:
            return 0
        }
    }

    private func enqueue(_ notification: ManyDeletesNotification) {
        guard severity(of: notification.notificationType) > 0 else { return }

        let syncDbId = notification.syncDbId
        if let presentedNotification, presentedNotification.syncDbId == syncDbId {
            guard severity(of: notification.notificationType) > severity(of: presentedNotification.notificationType) else {
                return
            }

            pendingNotifications.append(notification)
            abortCurrentModalIfNeeded()
            return
        }

        if let index = pendingNotifications.firstIndex(where: { $0.syncDbId == syncDbId }) {
            guard severity(of: notification.notificationType) > severity(of: pendingNotifications[index].notificationType) else {
                return
            }

            pendingNotifications[index] = notification
        } else {
            pendingNotifications.append(notification)
        }

        presentNextNotification()
    }

    private func abortCurrentModalIfNeeded() {
        guard let sheet = currentSheet else { return }

        currentSheet = nil
        if let window = currentParentWindow {
            window.endSheet(sheet, returnCode: .abort)
        }
        currentParentWindow = nil
    }

    private func presentNextNotification() {
        guard presentedNotification == nil, !pendingNotifications.isEmpty else { return }

        let notification = pendingNotifications.removeFirst()
        presentedNotification = notification

        Task {
            (NSApp.delegate as? AppDelegate)?.openMainWindow()
            await presentNotification(notification)

            presentedNotification = nil
            presentNextNotification()
        }
    }

    private func presentNotification(_ notification: ManyDeletesNotification) async {
        let syncDbId = notification.syncDbId

        guard let synchro = await coherentCache.getSynchro(synchroDbId: syncDbId) else {
            IKLogger.general.error("[KD] Synchro not found for syncDbId:\(syncDbId)")
            return
        }

        guard let driveId = await coherentCache.getDrive(driveDbId: synchro.driveDbId)?.driveId else { return }

        switch notification.notificationType {
        case .SoftLimit:
            await runSoftLimitAlert(nbFiles: notification.nbFiles, driveId: Int(driveId))
        case .HardLimit:
            guard let userChoice = await runHardLimitAlert(nbFiles: notification.nbFiles) else { return }

            await acknowledge(syncDbId: syncDbId, userChoice: userChoice)
        default:
            break
        }
    }

    private func runSoftLimitAlert(nbFiles: UInt64, driveId: Int) async {
        let alert = makeAlert(
            style: .warning,
            title: KDriveLocalizable.manyDeleteDialogTitle(nbFiles),
            message: KDriveLocalizable.manyDeleteDialogSoftLimitContent
        )
        alert.addButton(withTitle: KDriveLocalizable.buttonClose)
        alert.addButton(withTitle: KDriveLocalizable.buttonCloseDoNotAskAgain)
        alert.addButton(withTitle: KDriveLocalizable.buttonOpenTrash)

        let response = await runModal(for: alert)

        if response == .alertThirdButtonReturn {
            let trashUrl = WebFolder.trash.url(driveID: driveId)

            NSWorkspace.shared.open(trashUrl)
        } else if response == .alertSecondButtonReturn {
            closeAndDoNotAskAgain()
        }
    }

    private func runHardLimitAlert(nbFiles: UInt64) async -> KDC.TooManyDeletesUserChoice? {
        let alert = makeAlert(
            style: .critical,
            title: KDriveLocalizable.manyDeleteDialogTitle(nbFiles),
            message: KDriveLocalizable.manyDeleteDialogHardLimitContent
        )

        alert.addButton(withTitle: KDriveLocalizable.manyDeleteDialogHardLimitPrimary)
        let secondaryButton = alert.addButton(withTitle: KDriveLocalizable.manyDeleteDialogHardLimitSecondary)
        secondaryButton.hasDestructiveAction = true

        let response = await runModal(for: alert)

        guard response != .abort else { return nil }

        return response == .alertSecondButtonReturn ? .Continue : .Revert
    }

    private func runModal(for alert: NSAlert) async -> NSApplication.ModalResponse {
        guard let window = NSApp.mainWindow ?? NSApp.keyWindow else {
            return alert.runModal()
        }

        return await withCheckedContinuation { continuation in
            alert.beginSheetModal(for: window) { [weak self] response in
                self?.currentSheet = nil
                self?.currentParentWindow = nil
                continuation.resume(returning: response)
            }
            currentParentWindow = window
            currentSheet = window.attachedSheet
        }
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

    private func closeAndDoNotAskAgain() {
        @InjectService var settingsCache: SettingsCaching
        Task {
            do {
                guard let parametersInfo = await settingsCache.getSettings() else { return }
                let newParametersInfo = ParametersInfo(
                    language: parametersInfo.language,
                    monoIcons: parametersInfo.monoIcons,
                    autoStart: parametersInfo.autoStart,
                    moveToTrash: parametersInfo.moveToTrash,
                    notificationsDisabled: parametersInfo.notificationsDisabled,
                    useLog: parametersInfo.useLog,
                    logLevel: parametersInfo.logLevel,
                    extendedLog: parametersInfo.extendedLog,
                    purgeOldLogs: parametersInfo.purgeOldLogs,
                    proxyConfigInfo: parametersInfo.proxyConfigInfo,
                    darkTheme: parametersInfo.darkTheme,
                    maxAllowedCpu: parametersInfo.maxAllowedCpu,
                    distributionChannel: parametersInfo.distributionChannel,
                    sentryEnabled: parametersInfo.sentryEnabled,
                    matomoEnabled: parametersInfo.matomoEnabled,
                    askBeforeDelete: false
                )
                try await ParametersJobs().updateParameters(parametersInfo: newParametersInfo)
            } catch {
                SentrySDK.capture(error: error)
                IKLogger.general.error("[KD] Failed to disable askBeforeDelete: \(error)")
            }
        }
    }
}
