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
import MatomoTracker
import OSLog

public final class MatomoUtils {
    @LazyInjectService private var settingsCacheObservable: SettingsCacheObservable

    private let tracker: MatomoTracker
    private let enableLogger: Bool

    private var cancellable: AnyCancellable?

    public init(siteId: String, baseURL: URL, enableLogger: Bool = false) {
        tracker = MatomoTracker(siteId: siteId, baseURL: baseURL)

        #if DEBUG || TEST
        self.enableLogger = enableLogger
        optOut(true)
        #else
        self.enableLogger = false
        optOut(!UserDefaults.standard.lastKnownMatomoEnabled)
        #endif

        observePreferences()
    }

    public func optOut(_ optOut: Bool) {
        tracker.isOptedOut = optOut
    }

    public func track(eventWithCategory category: String, action: UserAction = .click, name: String, value: Float? = nil) {
        if enableLogger {
            Logger.matomo.trackedEvent(category: category, action: action, name: name, value: value)
        }
        tracker.track(eventWithCategory: category, action: action.rawValue, name: name, value: value)
    }

    public func track(eventWithCategory category: EventCategory, action: UserAction = .click, name: String, value: Float? = nil) {
        track(eventWithCategory: category.displayName, action: action, name: name, value: value)
    }

    public func track(eventWithCategory category: EventCategory, action: UserAction = .click, name: String, value: Bool) {
        track(eventWithCategory: category, action: action, name: name, value: value ? 1 : 0)
    }

    private func observePreferences() {
        cancellable = settingsCacheObservable.settingsPublisher
            .map(\.matomoEnabled)
            .removeDuplicates()
            .sink { [weak self] isEnabled in
                self?.optOut(!isEnabled)
            }
    }
}

public extension MatomoUtils {
    struct EventCategory {
        public let displayName: String

        public init(displayName: String) {
            self.displayName = displayName
        }

        public static let onboardingWelcomePage = EventCategory(displayName: "onboardingWelcomePage")
        public static let onboardingConnectionFailedPage = EventCategory(displayName: "onboardingConnectionFailedPage")
        public static let onboardingSyncConfigurationPage = EventCategory(displayName: "onboardingSyncConfigurationPage")
        public static let driveSetupDialog = EventCategory(displayName: "driveSetupDialog")
        public static let search = EventCategory(displayName: "search")
        public static let startPauseButton = EventCategory(displayName: "startPauseButton")
        public static let homePage = EventCategory(displayName: "homePage")
        public static let syncSelector = EventCategory(displayName: "syncSelector")
        public static let navBar = EventCategory(displayName: "navBar")
        public static let activityPage = EventCategory(displayName: "activityPage")
        public static let errorPage = EventCategory(displayName: "errorPage")
        public static let errors = EventCategory(displayName: "errors")
        public static let batchConflictResolutionPage = EventCategory(displayName: "batchConflictResolutionPage")
        public static let individualConflictResolutionPage = EventCategory(displayName: "individualConflictResolutionPage")
        public static let generalSettingsPage = EventCategory(displayName: "generalSettingsPage")
        public static let accountsSettingsPage = EventCategory(displayName: "accountsSettingsPage")
        public static let advancedSettingsPage = EventCategory(displayName: "advancedSettingsPage")
        public static let driveManagementPage = EventCategory(displayName: "driveManagementPage")
        public static let driveAdvancedSyncsPage = EventCategory(displayName: "driveAdvancedSyncsPage")
        public static let exclusionSelector = EventCategory(displayName: "exclusionSelector")
        public static let asleepErrorPage = EventCategory(displayName: "asleepErrorPage")
        public static let driveAccessDeniedPage = EventCategory(displayName: "driveAccessDeniedPage")
        public static let loginErrorPage = EventCategory(displayName: "loginErrorPage")
        public static let maintenanceErrorPage = EventCategory(displayName: "maintenanceErrorPage")
        public static let notRenewErrorPage = EventCategory(displayName: "notRenewErrorPage")
        public static let UpdateDialog = EventCategory(displayName: "UpdateDialog")
    }

    enum UserAction: String {
        case click, input, drag, longPress, data
    }
}

extension os.Logger {
    static let matomo = Logger(subsystem: "InfomaniakCoreCommonUI", category: "matomoUtils")

    func trackedEvent(category: String, action: MatomoUtils.UserAction, name: String, value: Float?) {
        var logMessage = "category: \(category), name: \(name)"
        if let value {
            logMessage += ", value: \(value)"
        }
        logMessage += " (action: \(action.rawValue))"

        matomo(type: "Event", content: logMessage)
    }

    private func matomo(type: String, content: String) {
        debug("[Matomo - \(type)] \(content)")
    }
}
