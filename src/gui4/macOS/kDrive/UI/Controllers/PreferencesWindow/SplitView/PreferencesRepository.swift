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

import Combine
import Foundation
import InfomaniakDI
import kDriveCore
import kDriveCoreUI

@MainActor
public final class PreferencesRepository: ObservableObject {
    @LazyInjectService private var settingsCache: SettingsCaching

    @Published public private(set) var parametersInfo = UIParametersInfo()

    public init() {}

    public func refreshData() async throws {
        try await settingsCache.refresh()
        if let refreshedData = await settingsCache.getSettings() {
            parametersInfo = UIParametersInfo(parametersInfo: refreshedData)
        }
    }

    public func update<T>(_ keyPath: WritableKeyPath<UIParametersInfo, T>, value: T) async throws {
        let setting = Self.settingName(for: keyPath)
        IKLogger.general.info("[KD] Preference update requested setting=\(setting)")
        do {
            try await persist(keyPath, value: value)
        } catch {
            IKLogger.general.warning("[KD] Preference update failed setting=\(setting) retainingPreviousUIValue=true")
            throw error
        }
    }

    private func persist<T>(_ keyPath: WritableKeyPath<UIParametersInfo, T>, value: T) async throws {
        var updatedParameters = parametersInfo
        updatedParameters[keyPath: keyPath] = value
        if keyPath == \UIParametersInfo.isExtendedLogEnabled, updatedParameters.isExtendedLogEnabled {
            updatedParameters.logLevel = .debug
        }

        if await settingsCache.getSettings() == nil {
            try await settingsCache.refresh()
        }
        guard let currentData = await settingsCache.getSettings() else {
            IKLogger.general.warning("[KD] Preference update skipped setting=\(Self.settingName(for: keyPath)) reason=settingsUnavailable")
            return
        }

        let payload = updatedParameters.copyToParametersInfo(from: currentData)
        try await settingsCache.update(payload)

        if let refreshedData = await settingsCache.getSettings() {
            parametersInfo = UIParametersInfo(parametersInfo: refreshedData)
            IKLogger.general.info("[KD] Preference update completed setting=\(Self.settingName(for: keyPath))")
        } else {
            IKLogger.general.warning("[KD] Preference update sent but refreshed settings unavailable setting=\(Self.settingName(for: keyPath))")
        }
    }

    private static func settingName(for keyPath: AnyKeyPath) -> String {
        switch keyPath {
        case \UIParametersInfo.language: return "language"
        case \UIParametersInfo.launchOnStartup: return "launchOnStartup"
        case \UIParametersInfo.moveDeletedFilesToTrash: return "moveDeletedFilesToTrash"
        case \UIParametersInfo.notificationsState: return "notificationsState"
        case \UIParametersInfo.shouldUseLog: return "shouldUseLog"
        case \UIParametersInfo.logLevel: return "logLevel"
        case \UIParametersInfo.isExtendedLogEnabled: return "isExtendedLogEnabled"
        case \UIParametersInfo.shouldPurgeOldLogs: return "shouldPurgeOldLogs"
        case \UIParametersInfo.proxyConfiguration: return "proxyConfiguration"
        case \UIParametersInfo.distributionChannel: return "distributionChannel"
        case \UIParametersInfo.isSentryEnabled: return "isSentryEnabled"
        case \UIParametersInfo.isMatomoEnabled: return "isMatomoEnabled"
        default: return "unknown"
        }
    }
}
