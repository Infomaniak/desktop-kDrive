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
    var receivedSettingsValues: AsyncStream<ParametersInfo> {
        AsyncStream { continuation in
            let cancellable = settingsPublisher
                .sink { value in continuation.yield(value) }

            continuation.onTermination = { _ in cancellable.cancel() }
        }
    }
}

@Suite("SettingsCache Test")
struct SettingsCacheTests {
    private static func makeParametersInfo(sentryEnabled: Bool) -> ParametersInfo {
        ParametersInfo(
            language: .English,
            monoIcons: false,
            autoStart: true,
            moveToTrash: true,
            notificationsDisabled: .Never,
            useLog: true,
            logLevel: .Debug,
            extendedLog: false,
            purgeOldLogs: true,
            proxyConfigInfo: ProxyConfigInfo(type: .None, hostName: "", port: 0, needsAuth: false, user: "", pwd: ""),
            darkTheme: false,
            maxAllowedCpu: 50,
            distributionChannel: .Prod,
            sentryEnabled: sentryEnabled,
            matomoEnabled: true,
            askBeforeDelete: true
        )
    }

    @Test(.timeLimit(.minutes(1)))
    func publishesAndStoresSettings() async throws {
        // GIVEN
        let cache = SettingsCache()
        let receivedValues = await cache.receivedSettingsValues // Start to save the received values

        var receivedSettings: ParametersInfo?
        var cancellables = Set<AnyCancellable>()

        let subscription = cache.settingsPublisher
            .sink { settings in receivedSettings = settings }
        subscription.store(in: &cancellables)

        // WHEN
        await cache.setSettings(Self.makeParametersInfo(sentryEnabled: false))

        // THEN
        _ = await receivedValues.first { _ in true }

        #expect(receivedSettings != nil, "Should have received a settings update")
        #expect(receivedSettings?.sentryEnabled == false, "Received settings should match expected")

        let storedSettings = await cache.getSettings()
        #expect(storedSettings?.sentryEnabled == false, "Cache should retain the last settings")
    }

    @Test(.timeLimit(.minutes(1)))
    func persistsLastKnownSentryEnabledFlag() async throws {
        // GIVEN
        let originalValue = UserDefaults.standard.lastKnownSentryEnabled
        defer { UserDefaults.standard.lastKnownSentryEnabled = originalValue }

        let cache = SettingsCache()

        // WHEN
        await cache.setSettings(Self.makeParametersInfo(sentryEnabled: false))

        // THEN
        #expect(UserDefaults.standard.lastKnownSentryEnabled == false, "Should persist the disabled flag")

        // WHEN
        await cache.setSettings(Self.makeParametersInfo(sentryEnabled: true))

        // THEN
        #expect(UserDefaults.standard.lastKnownSentryEnabled == true, "Should persist the enabled flag")
    }
}
