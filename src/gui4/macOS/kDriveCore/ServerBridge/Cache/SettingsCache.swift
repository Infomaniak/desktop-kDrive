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

public typealias SettingsPublisher = AnyPublisher<ParametersInfo, Never>

public protocol SettingsCacheObservable: Sendable {
    var settingsPublisher: SettingsPublisher { get }
}

public protocol SettingsCaching: Sendable {
    func getSettings() async -> ParametersInfo?
    func refresh() async throws
    func update(_ parametersInfo: ParametersInfo) async throws
}

public actor SettingsCache: SettingsCaching, SettingsCacheObservable {
    private var settings: ParametersInfo?

    private nonisolated let settingsSubject = CurrentValueSubject<ParametersInfo?, Never>(nil)

    public nonisolated var settingsPublisher: SettingsPublisher {
        settingsSubject
            .compactMap { $0 }
            .eraseToAnyPublisher()
    }

    public init() {}

    public func getSettings() -> ParametersInfo? {
        settings
    }

    public func refresh() async throws {
        let refreshedSettings = try await ParametersJobs().parametersInfo()
        setSettings(refreshedSettings)
    }

    public func update(_ parametersInfo: ParametersInfo) async throws {
        try await ParametersJobs().updateParameters(parametersInfo: parametersInfo)
        try await refresh()
    }

    func setSettings(_ settings: ParametersInfo) {
        self.settings = settings

        UserDefaults.standard.lastKnownSentryEnabled = settings.sentryEnabled
        UserDefaults.standard.lastKnownMatomoEnabled = settings.matomoEnabled

        settingsSubject.send(settings)
    }
}
