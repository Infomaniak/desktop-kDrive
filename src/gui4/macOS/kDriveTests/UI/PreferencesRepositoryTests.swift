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
@testable import InfomaniakDI
@testable import kDrive
@testable import kDriveCore
import kDriveCoreUI
import Testing

extension SharedDITests {
    @MainActor
    @Suite(.timeLimit(.minutes(1)))
    struct PreferencesRepositoryTests {
        @Test("Extended logging saves Debug verbosity in the same update", arguments: UILogLevel.allCases)
        func enablingExtendedLogging(logLevel: UILogLevel) async throws {
            let bundle = Bundle(for: TestBundleMarker.self)
            let url = try #require(bundle.url(forResource: "PARAMETERS_INFO", withExtension: "json"))
            let response = try JSONDecoder().decode(CallbackMessage<ParametersInfoResponse>.self, from: Data(contentsOf: url))
            var parameters = UIParametersInfo(parametersInfo: response.body.parametersInfo)
            parameters.logLevel = logLevel
            parameters.isExtendedLogEnabled = false
            let initialSettings = parameters.copyToParametersInfo(from: response.body.parametersInfo)
            let cache = MockSettingsCache(settings: initialSettings)

            let resolver = SimpleResolver.sharedResolver
            let identifier = resolver.buildIdentifier(type: SettingsCaching.self)
            let previousFactory = resolver.factories[identifier]
            let previousService = resolver.store[identifier]
            defer {
                resolver.factories[identifier] = previousFactory
                resolver.store[identifier] = previousService
            }
            resolver.store.removeValue(forKey: identifier)
            resolver.store(factory: Factory(type: SettingsCaching.self) { _, _ in cache })

            let repository = PreferencesRepository()
            try await repository.refreshData()
            try await repository.update(\.isExtendedLogEnabled, value: true)

            let updates = await cache.updates
            #expect(updates.count == 1)
            let savedSettings = try #require(updates.first)
            #expect(savedSettings.extendedLog)
            #expect(UIParametersInfo(parametersInfo: savedSettings).logLevel == .debug)
            #expect(repository.parametersInfo.isExtendedLogEnabled)
            #expect(repository.parametersInfo.logLevel == .debug)
            var expectedParameters = parameters
            expectedParameters.isExtendedLogEnabled = true
            expectedParameters.logLevel = .debug
            #expect(repository.parametersInfo == expectedParameters)

            try await repository.update(\.isExtendedLogEnabled, value: false)
            #expect(!repository.parametersInfo.isExtendedLogEnabled)
            #expect(repository.parametersInfo.logLevel == .debug)

            try await repository.update(\.logLevel, value: UILogLevel.info)
            #expect(repository.parametersInfo.logLevel == .info)
        }
    }
}

private actor MockSettingsCache: SettingsCaching {
    private var settings: ParametersInfo
    private(set) var updates: [ParametersInfo] = []

    init(settings: ParametersInfo) {
        self.settings = settings
    }

    func getSettings() -> ParametersInfo? {
        settings
    }

    func refresh() {}

    func update(_ parametersInfo: ParametersInfo) {
        updates.append(parametersInfo)
        settings = parametersInfo
    }
}
