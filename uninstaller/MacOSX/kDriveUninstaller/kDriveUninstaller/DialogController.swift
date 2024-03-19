/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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
import OSLog

class DialogController {
    
    let kDriveBundleId = "com.infomaniak.drive.desktopclient"
    lazy var kDriveAppPath: URL? = NSWorkspace.shared.urlForApplication(withBundleIdentifier: kDriveBundleId)?.appendingPathComponent("Contents").appendingPathComponent("MacOS").appendingPathComponent("kDrive")
    
    // uninstall functions
    
    func forceQuitApp() {
        let runningApplications = NSWorkspace.shared.runningApplications
        if let kdriveApp = runningApplications.first(where: { (application) in
            return application.bundleIdentifier == kDriveBundleId
        }) {
            kill(kdriveApp.processIdentifier, SIGTERM)
        }
    }
    
    func deleteConfigFolder() {
        let appSupportOpt = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask).first?.appendingPathComponent("kDrive")
        let preferencesOpt = FileManager.default.urls(for: .libraryDirectory, in: .userDomainMask).first?.appendingPathComponent("Preferences").appendingPathComponent("kDrive")
        do {
            if let appSupport = appSupportOpt {
                try FileManager.default.removeItem(at: appSupport)
            }
        } catch CocoaError.fileNoSuchFile {
            print("No such folder : " + (appSupportOpt?.absoluteString ?? "unknown"))
        } catch CocoaError.fileWriteNoPermission {
            print("Error can't delete this folder : " + (appSupportOpt?.absoluteString ?? "unknown"))
        } catch {
            print("Unknown error can't delete this folder : " + (appSupportOpt?.absoluteString ?? "unknown"))
        }
        
        do {
            if let preferences = preferencesOpt {
                try FileManager.default.removeItem(at: preferences)
            }
        } catch CocoaError.fileNoSuchFile {
            print("No such folder : " + (preferencesOpt?.absoluteString ?? "unknown"))
        } catch CocoaError.fileReadNoPermission {
            print("Error can't delete this folder : " + (preferencesOpt?.absoluteString ?? "unknown"))
        } catch {
            print("Unknown error can't delete this folder : " + (preferencesOpt?.absoluteString ?? "unknown"))
        }
    }
    
    func deleteLogFiles() {
        let task = Process()
        task.launchPath = "/usr/bin/env"
        task.arguments = ["find", "/private/var/folders", "-name", "kDrive-logdir"]
        
        let pipe = Pipe()
        let errorPipe = Pipe()
        task.standardOutput = pipe
        task.standardError = errorPipe
        task.launch()
       
        let data = pipe.fileHandleForReading.readDataToEndOfFile()
        if let output = String(data: data, encoding: .utf8) {
            let result = output.filter { !$0.isWhitespace }
            let fileManager = FileManager.default
            let directoryURL = URL(fileURLWithPath: result)
            do {
                try fileManager.removeItem(at: directoryURL)
            } catch {
                print("Log folder can't be accessed or deleted")
            }
        }
    }
    
    func clearKeychainsKeys() {
        let task = Process()
        task.launchPath = kDriveAppPath?.path
        task.arguments = ["--clearKeychainKeys"]
        
        let pipe = Pipe()
        let errorPipe = Pipe()
        task.standardOutput = pipe
        task.standardError = errorPipe
        task.launch()
    }
        
    func clearSyncNodes() {
        let task = Process()
        task.launchPath = kDriveAppPath?.path
        task.arguments = ["--clearSyncNodes"]
        
        let pipe = Pipe()
        let errorPipe = Pipe()
        task.standardOutput = pipe
        task.standardError = errorPipe
        task.launch()
    }
    
    func deleteApp() {
        let script = """
            tell application "Finder"
                activate
                try
                    set deleteCmd to "rm -vrf " & "~/.Trash/kDrive.app"
                    do shell script deleteCmd with administrator privileges
                end try

                try
                    set deleteCmd to "rm -vrf " & "~/.Trash/kDrive Uninstaller.app"
                    do shell script deleteCmd with administrator privileges
                end try

                try
                    move application file "kDrive" of folder "kDrive" of folder "Applications" of startup disk to trash
                    move application file "kDrive Uninstaller" of folder "kDrive" of folder "Applications" of startup disk to trash
                    move folder "kDrive" of folder "Applications" of startup disk to trash
                end try
            end tell
        """
        
        var error: NSDictionary?
        if let scriptObject = NSAppleScript(source: script) {
            scriptObject.executeAndReturnError(&error)
            if let err = error {
                print(err)
                alertError(message: NSLocalizedString("kDrive can't be moved to the trash", comment: ""), style: NSAlert.Style.informational)
            }
        }
    }

    func alertError(message: String, style: NSAlert.Style) {
        let alertError = NSAlert()
        alertError.icon = NSImage(named: "AppIcon")
        alertError.messageText = NSLocalizedString("Error when uninstalling", comment: "")
        alertError.informativeText = message
        alertError.alertStyle = style

        let okButton = alertError.addButton(withTitle: "Ok")
        okButton.target = self
        okButton.action = #selector(DialogController.cancel)
        alertError.runModal()
    }
    
    func isKdrivePathExist() -> Bool {
        return kDriveAppPath != nil;
    }
    
    func deleteConfiguration() {
        if (isKdrivePathExist()) {
            clearKeychainsKeys()
        }
        deleteLogFiles()
        deleteConfigFolder()
    }
    
    // logic functions
    
    @objc
    func cancel(_ sender: NSButton) {
        NSApplication.shared.terminate(nil)
    }
    
    @objc
    func uninstallAppAndRemoveConfig(_ sender: NSButton) {
        uninstall(removeConfig:true)
    }

    @objc
    func uninstallApp(_ sender: NSButton) {
        uninstall(removeConfig:false)
    }
    
    func uninstall(removeConfig : Bool) {
        
        forceQuitApp()
        // wait the app quitting
        sleep(1)
        if (self.isKdrivePathExist()) {

            if (removeConfig) {
                deleteConfiguration()
            }
            else {
                clearSyncNodes()
                // wait for app to clear database
                sleep(1)
            }

            deleteApp()
        }
        else {
            alertError(message: "kDrive is not installed", style: NSAlert.Style.informational)
        }
        NSApplication.shared.terminate(nil)
    }
    
        
    
    func showDialog() {
        let uninstallAlert = NSAlert()
        uninstallAlert.icon = NSImage(named: "AppIcon")
        uninstallAlert.messageText = NSLocalizedString("Uninstall kDrive?", comment: "")
        uninstallAlert.informativeText = NSLocalizedString("kDrive app will be removed", comment: "")

        let cancelButton = uninstallAlert.addButton(withTitle: NSLocalizedString("Cancel", comment: ""))
        let acceptButton = uninstallAlert.addButton(withTitle: NSLocalizedString("Uninstall", comment: ""))

        if #available(macOS 11.0, *) {
            acceptButton.hasDestructiveAction = true
        }
        acceptButton.target = self
        acceptButton.action = #selector(DialogController.uninstallAppAndRemoveConfig)

        cancelButton.target = self
        cancelButton.action = #selector(DialogController.cancel)

        uninstallAlert.runModal()
    }
}
