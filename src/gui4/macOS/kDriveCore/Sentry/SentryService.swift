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
import Sentry

public final class SentryService {
    @LazyInjectService private var settingsCacheObservable: SettingsCacheObservable

    private var isSentryAuthorized: Bool
    private var cancellable: AnyCancellable?

    public init() {
        // Apply the last known value immediately at startup, before the server connection is ready.
        isSentryAuthorized = UserDefaults.standard.lastKnownSentryEnabled
        observeSettings()
    }

    public func initSentry() {
        SentrySDK.start { options in
            options.dsn = "https://b6784e934228fe7683d206898d9d0f1e@sentry-desktop.infomaniak.com/6"
            options.tracePropagationTargets = []
            options.enableNetworkTracking = false
            options.enableNetworkBreadcrumbs = false
            options.enableSwizzling = false // We can disable swizzling because we only used it for networking
            options.maxBreadcrumbs = 100
            if #available(macOS 12.0, *) {
                options.enableMetricKit = true
            }

            options.beforeSend = { [weak self] event in
                guard let self else { return nil }

                #if DEBUG || TEST
                return nil
                #else
                return isSentryAuthorized ? event : nil
                #endif
            }
        }
    }

    private func observeSettings() {
        cancellable = settingsCacheObservable.settingsPublisher
            .map(\.sentryEnabled)
            .removeDuplicates()
            .sink { [weak self] isEnabled in
                self?.isSentryAuthorized = isEnabled
            }
    }
}
