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
@testable import kDriveCore
import Testing

extension SettingsCache {
    var settingsEmissions: AsyncStream<Void> {
        AsyncStream { continuation in
            let cancellable = settingsPublisher
                .sink { _ in continuation.yield(()) }

            continuation.onTermination = { _ in cancellable.cancel() }
        }
    }
}

@MainActor
@Suite("SettingsCache Test")
struct SettingsCacheTests {
    // MARK: - Test Data

    private static func decodedResponse() throws -> CallbackMessage<ParametersInfoResponse> {
        let bundle = Bundle(for: TestBundleMarker.self)

        guard let url = bundle.url(forResource: "PARAMETERS_INFO", withExtension: "json") else {
            fatalError("Unable to find specified JSON file")
        }

        let data = try Data(contentsOf: url)
        return try JSONDecoder().decode(CallbackMessage<ParametersInfoResponse>.self, from: data)
    }

    // MARK: - Tests

    @Test(.timeLimit(.minutes(1)))
    func publishesAndStoresSettings() async throws {
        // GIVEN
        let settings = try Self.decodedResponse().body.parametersInfo
        let cache = SettingsCache()
        let emissions = await cache.settingsEmissions // Start observing before mutating

        var receivedSentryEnabled: Bool?
        var cancellables = Set<AnyCancellable>()

        let subscription = cache.settingsPublisher
            .sink { receivedSentryEnabled = $0.sentryEnabled }
        subscription.store(in: &cancellables)

        // WHEN
        await cache.setSettings(settings)

        // THEN
        _ = await emissions.first { _ in true }

        #expect(receivedSentryEnabled == false, "Should have published the settings")

        let storedSentryEnabled = await cache.getSettings()?.sentryEnabled
        #expect(storedSentryEnabled == false, "Cache should retain the last settings")
    }

    @Test(.timeLimit(.minutes(1)))
    func persistsLastKnownSentryEnabledFlag() async throws {
        // GIVEN
        let originalValue = UserDefaults.standard.lastKnownSentryEnabled
        defer { UserDefaults.standard.lastKnownSentryEnabled = originalValue }

        let settings = try Self.decodedResponse().body.parametersInfo
        let cache = SettingsCache()

        // Seed the opposite value to prove `setSettings` actively writes the flag.
        UserDefaults.standard.lastKnownSentryEnabled = true

        // WHEN
        await cache.setSettings(settings)

        // THEN
        #expect(
            UserDefaults.standard.lastKnownSentryEnabled == false,
            "Should persist the Sentry flag coming from the settings"
        )
    }

    @Test(.timeLimit(.minutes(1)))
    func persistsLastKnownFileLogLevel() async throws {
        // GIVEN
        let originalValue = UserDefaults.standard.lastKnownFileLogLevel
        defer { UserDefaults.standard.lastKnownFileLogLevel = originalValue }

        let settings = try Self.decodedResponse().body.parametersInfo

        let cache = SettingsCache()

        UserDefaults.standard.lastKnownFileLogLevel = .error

        // WHEN
        await cache.setSettings(settings)

        // THEN
        // The fixture carries `logLevel: 0` (KDC.LogLevel.Debug), which maps to `.debug`.
        #expect(
            UserDefaults.standard.lastKnownFileLogLevel == .debug,
            "Should persist the log level coming from the settings"
        )
    }
}
