/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import Cocoa
import InfomaniakDI
import kDriveCore
import kDriveCoreUI
import kDriveResources

@MainActor
final class AppDelegate: NSObject, NSApplicationDelegate {
    private lazy var mainWindow = MainWindowController()
    private var preferencesWindow: PreferencesWindowController?
    private var onboardingWindow: OnboardingWindowController?

    // periphery:ignore - We keep a strong reference on the statusBarManager
    private(set) var statusBarManager: StatusBarManager?

    // periphery:ignore - We keep a strong reference on the dockIconManager
    private(set) var dockIconManager: DockIconManager?

    // periphery:ignore - We keep a strong reference on the SentryService
    private(set) var sentryService: SentryService?

    // periphery:ignore - We keep a strong reference on the updateModalPresenter
    private(set) var updateModalPresenter: UpdateModalPresenter?

    private static var isRunningTests: Bool {
        ProcessInfo.processInfo.environment["XCTestConfigurationFilePath"] != nil
            || Bundle.allBundles.contains { $0.bundlePath.hasSuffix(".xctest") }
    }

    func applicationDidFinishLaunching(_ aNotification: Notification) {
        let testing = AppDelegate.isRunningTests
        DriveTargetAssembly.setupDI(testing: testing)

        guard !testing else {
            return
        }

        sentryService = SentryService()
        sentryService?.initSentry()

        statusBarManager = StatusBarManager()
        dockIconManager = DockIconManager()
        updateModalPresenter = UpdateModalPresenter()

        openMainWindow()
    }

    func applicationSupportsSecureRestorableState(_ app: NSApplication) -> Bool {
        return true
    }

    @objc func showAboutPanel() {
        let credits = NSAttributedString(
            string: KDriveLocalizable.aboutKDriveDescription,
            attributes: [
                .font: NSFont.systemFont(ofSize: 11),
                .foregroundColor: NSColor.secondaryLabelColor
            ]
        )

        let version = Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "—"
        let build = Bundle.main.infoDictionary?["CFBundleVersion"] as? String ?? "—"
        let year = Calendar.current.component(.year, from: Date())

        NSApplication.shared.orderFrontStandardAboutPanel(
            options: [
                .credits: credits,
                .applicationName: Constants.appName,
                .applicationVersion: KDriveLocalizable.aboutAppVersionCopyrightMac(
                    version, build, year
                ),
                .version: "",
                .applicationIcon: NSImage(named: "AppIcon")!
            ]
        )
    }

    @objc func openMainWindow() {
        dockIconManager?.showDockIconAndActivate()

        mainWindow.showWindow(nil)
        mainWindow.window?.orderFrontRegardless()
        mainWindow.window?.makeKey()
        mainWindow.window?.makeFirstResponder(nil)
    }

    @objc func bringAllWindowsToFront() {
        if #available(macOS 14.0, *) {
            NSApp.activate()
        } else {
            NSApp.activate(ignoringOtherApps: true)
        }

        if mainWindow.window?.isVisible == true {
            mainWindow.window?.orderFrontRegardless()
            mainWindow.window?.makeKey()
            mainWindow.window?.makeFirstResponder(nil)
        }

        if let preferencesWindow, preferencesWindow.window?.isVisible == true {
            preferencesWindow.window?.orderFrontRegardless()
        }
    }

    @objc func openPreferencesWindow() {
        @InjectService var matomo: MatomoUtils
        matomo.track(eventWithCategory: .navBar, name: "openSettings")
        if preferencesWindow?.window?.isVisible != true {
            @InjectService var preferencesRouter: PreferencesViewRouter
            preferencesRouter.resetToDefaultState()
        }

        if preferencesWindow == nil {
            preferencesWindow = PreferencesWindowController()
        }

        dockIconManager?.showDockIconAndActivate()
        preferencesWindow?.window?.makeKeyAndOrderFront(nil)
        preferencesWindow?.window?.isReleasedWhenClosed = false
    }

    func openOnboardingWindow() {
        if onboardingWindow == nil {
            let controller = OnboardingWindowController()
            controller.onClose = { [weak self] in
                self?.onboardingWindow = nil
            }
            onboardingWindow = controller
        }

        dockIconManager?.showDockIconAndActivate()
        onboardingWindow?.window?.makeKeyAndOrderFront(nil)
    }

    func applicationShouldHandleReopen(_ sender: NSApplication, hasVisibleWindows flag: Bool) -> Bool {
        if !flag {
            openMainWindow()
        }
        return true
    }

    @objc func quitApp() {
        NSApp.terminate(nil)
    }

    func applicationShouldTerminate(_ sender: NSApplication) -> NSApplication.TerminateReply {
        Task {
            #if !DEBUG
            try? await UtilityJobs().quit()
            #endif
            NSApp.reply(toApplicationShouldTerminate: true)
        }
        return .terminateLater
    }
}
