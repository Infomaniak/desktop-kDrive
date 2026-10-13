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
@testable import InfomaniakDI
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

extension SharedDITests {
    @MainActor
    @Suite("SettingsCache Test")
    struct SettingsCacheTests {
        private let defaultsRestoration = SettingsDefaultsRestoration()
    }
}

private final class SettingsDefaultsRestoration {
    private let keys = [UserDefaults.Key.lastKnownFileLoggingEnabled, UserDefaults.Key.lastKnownFileLogLevel,
                        UserDefaults.Key.lastKnownSentryEnabled, UserDefaults.Key.lastKnownMatomoEnabled]
    private let values: [Any?]

    init() {
        values = keys.map { UserDefaults.standard.object(forKey: $0) }
    }

    deinit {
        for (key, value) in zip(keys, values) {
            if let value {
                UserDefaults.standard.set(value, forKey: key)
            } else {
                UserDefaults.standard.removeObject(forKey: key)
            }
        }
    }
}

extension SharedDITests.SettingsCacheTests {
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

    @Test("Server settings control file logging, extended verbosity, and restoration of the selected threshold")
    func appliesFileLoggingConfiguration() async throws {
        let defaults = UserDefaults.standard
        let resolver = SimpleResolver.sharedResolver
        let identifier = resolver.buildIdentifier(type: LogService.self)
        let previousFactory = resolver.factories[identifier]
        let previousService = resolver.store[identifier]
        defer {
            resolver.factories[identifier] = previousFactory
            resolver.store[identifier] = previousService
        }

        let writer = InMemoryLogFileWriter()
        let service = LogService(fileWriter: writer, sentryReporter: SpySentryLogReporter())
        resolver.store.removeValue(forKey: identifier)
        resolver.store(factory: Factory(type: LogService.self) { _, _ in service })

        let fixture = try Self.decodedResponse().body.parametersInfo
        var payload = try #require(JSONSerialization.jsonObject(with: JSONEncoder().encode(fixture)) as? [String: Any])
        payload["useLog"] = false
        payload["logLevel"] = KDC.LogLevel.Error.rawValue
        let disabledSettings = try JSONDecoder().decode(
            ParametersInfo.self,
            from: JSONSerialization.data(withJSONObject: payload)
        )
        let cache = SettingsCache()
        await cache.setSettings(disabledSettings)
        service.log(level: .fatal, category: "general", message: "disabled by server")
        service.flush()

        #expect(!defaults.lastKnownFileLoggingEnabled)
        #expect(defaults.lastKnownFileLogLevel == .error)
        #expect(writer.lines.isEmpty)

        payload["useLog"] = true
        let enabledSettings = try JSONDecoder().decode(ParametersInfo.self, from: JSONSerialization.data(withJSONObject: payload))
        await cache.setSettings(enabledSettings)
        service.log(level: .warning, category: "general", message: "below server threshold")
        service.log(level: .error, category: "general", message: "enabled by server")
        service.flush()

        #expect(defaults.lastKnownFileLoggingEnabled)
        #expect(writer.lines.count == 2)
        #expect(writer.lines.last?.contains("enabled by server") == true)

        payload["extendedLog"] = true
        let extendedSettings = try JSONDecoder().decode(
            ParametersInfo.self,
            from: JSONSerialization.data(withJSONObject: payload)
        )
        await cache.setSettings(extendedSettings)
        service.log(level: .debug, category: "general", message: "extended debug")
        service.flush()

        #expect(defaults.lastKnownFileLogLevel == .debug)
        #expect(await cache.getSettings()?.logLevel == .Error)
        #expect(writer.lines.last?.contains("extended debug") == true)

        payload["logLevel"] = KDC.LogLevel.Fatal.rawValue
        let updatedSettings = try JSONDecoder().decode(ParametersInfo.self, from: JSONSerialization.data(withJSONObject: payload))
        await cache.setSettings(updatedSettings)
        service.log(level: .debug, category: "general", message: "still extended debug")
        service.flush()

        #expect(defaults.lastKnownFileLogLevel == .debug)
        #expect(await cache.getSettings()?.logLevel == .Fatal)
        #expect(writer.lines.last?.contains("still extended debug") == true)

        payload["useLog"] = false
        let disabledExtendedSettings = try JSONDecoder().decode(
            ParametersInfo.self,
            from: JSONSerialization.data(withJSONObject: payload)
        )
        let countBeforeDisabling = writer.lines.count
        await cache.setSettings(disabledExtendedSettings)
        service.log(level: .fatal, category: "general", message: "disabled extended fatal")
        service.flush()

        #expect(!defaults.lastKnownFileLoggingEnabled)
        #expect(defaults.lastKnownFileLogLevel == .debug)
        #expect(writer.lines.count == countBeforeDisabling)

        payload["useLog"] = true
        payload["extendedLog"] = false
        let restoredSettings = try JSONDecoder().decode(
            ParametersInfo.self,
            from: JSONSerialization.data(withJSONObject: payload)
        )
        await cache.setSettings(restoredSettings)
        service.log(level: .error, category: "general", message: "below restored threshold")
        service.log(level: .fatal, category: "general", message: "restored fatal")
        service.flush()

        #expect(defaults.lastKnownFileLogLevel == .fatal)
        #expect(writer.lines.count == countBeforeDisabling + 2)
        #expect(writer.lines.last?.contains("restored fatal") == true)
    }
}
