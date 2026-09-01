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

import Foundation
import InfomaniakDI
import UserNotifications

struct UtilitySignalHandler {
    private let decoder = JSONDecoder()
    @LazyInjectService private var coherentCache: CoherentCache
    @LazyInjectService private var logUploadStatusCache: LogUploadStatusCaching

    func handleShowNotification(_ signal: Data) async throws {
        guard let notificationSignal = try? decoder.decode(SignalMessage<NotificationSignal>.self, from: signal) else {
            throw SignalError.unableToGetNotificationFromSignal
        }

        let content = UNMutableNotificationContent()
        content.title = notificationSignal.body.title
        content.body = notificationSignal.body.message

        let requestId = "kdrive_notification_\(notificationSignal.num)_\(notificationSignal.id)"
        let request = UNNotificationRequest(identifier: requestId, content: content, trigger: nil)
        do {
            try await UNUserNotificationCenter.current().add(request)
        } catch {
            IKLogger.xpc.error("[KD] Failed to post user notification: \(error)")
        }
    }

    func handleError(_ signal: Data) async throws {
        let errorInfoSignal: SignalMessage<ErrorInfoSignal>
        do {
            errorInfoSignal = try decoder.decode(SignalMessage<ErrorInfoSignal>.self, from: signal)
        } catch {
            IKLogger.xpc.error("[KD] Failed to decode error-added signal: \(error)")
            throw SignalError.unableToGetErrorInfoFromSignal
        }

        let errorMetadata = errorInfoSignal.body.errorInfo
        IKLogger.xpc.info(
            "[KD] [Signal ←] #\(errorInfoSignal.id) error added: syncDbId=\(errorMetadata.syncDbId) " +
                "level=\(errorMetadata.level) code=\(errorMetadata.exitCode) cause=\(errorMetadata.exitCause)"
        )

        let errorInfo = ErrorInfo(errorInfoMetadata: errorMetadata)
        try await coherentCache.addOrUpdateError(errorInfo)
    }

    func handleErrorRemoved(_ signal: Data) async throws {
        guard let errorRemovedSignal = try? decoder.decode(SignalMessage<ErrorRemovedSignal>.self, from: signal) else {
            throw SignalError.unableToGetErrorRemovedFromSignal
        }

        let errorDbId = errorRemovedSignal.body.errorDbId
        try await coherentCache.removeError(errorDbId)
    }

    func handleErrorCleared() async throws {
        await coherentCache.clearErrors()
    }

    func handleLogUploadStatusUpdated(_ signal: Data) async throws {
        guard let statusSignal = try? decoder.decode(SignalMessage<LogUploadStatusSignal>.self, from: signal) else {
            throw SignalError.unableToGetLogUploadStatusFromSignal
        }

        let status = LogUploadStatus(signal: statusSignal.body)
        IKLogger.xpc.log("[KD] Log upload status changed: \(status.state.rawValue) (\(status.percentage)%)")

        await logUploadStatusCache.setLogUploadStatus(status)
    }

    func handleShowSynthesis() async throws {
        await MainActor.run {
            NotificationCenter.default.post(name: .bringAllWindowsToFront, object: nil)
        }
    }

    func handleShowSettings() async throws {
        await MainActor.run {
            NotificationCenter.default.post(name: .bringSettingsToFront, object: nil)
        }
    }
}
