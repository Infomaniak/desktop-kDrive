/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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

import Foundation

protocol ParametersInfoProtocol {
    var language: NSString { get }
    var monoIcons: Bool { get }
    var autoStart: Bool { get }
    var moveToTrash: Bool { get }
    var darkTheme: Bool { get }
    var showShortcuts: Bool { get }

    var notificationsDisabled: Bool { get }

    var useLog: Bool { get }
    var logLevel: NSInteger { get }
    var extendedLog: Bool { get }
    var purgeOldLogs: Bool { get }

    var useBigFolderSizeLimit: Bool { get }
    var bigFolderSizeLimit: Int64 { get }
    var maxAllowedCpu: Int { get }
    var distributionChannel: NSInteger { get }

    init(
        language: NSString,
        monoIcons: Bool,
        autoStart: Bool,
        moveToTrash: Bool,
        darkTheme: Bool,
        showShortcuts: Bool,
        notificationsDisabled: Bool,
        useLog: Bool,
        logLevel: NSInteger,
        extendedLog: Bool,
        purgeOldLogs: Bool,
        useBigFolderSizeLimit: Bool,
        bigFolderSizeLimit: Int64,
        maxAllowedCpu: Int,
        distributionChannel: NSInteger
    )
}

@objc public class ParametersInfo: NSObject, NSSecureCoding, ParametersInfoProtocol {
    public static let supportsSecureCoding = true

    public let language: NSString
    public let monoIcons: Bool
    public let autoStart: Bool
    public let moveToTrash: Bool
    public let darkTheme: Bool
    public let showShortcuts: Bool
    public let notificationsDisabled: Bool
    public let useLog: Bool
    public let logLevel: NSInteger
    public let extendedLog: Bool
    public let purgeOldLogs: Bool
    public let useBigFolderSizeLimit: Bool
    public let bigFolderSizeLimit: Int64
    public let maxAllowedCpu: Int
    public let distributionChannel: NSInteger

    public required init(
        language: NSString,
        monoIcons: Bool,
        autoStart: Bool,
        moveToTrash: Bool,
        darkTheme: Bool,
        showShortcuts: Bool,
        notificationsDisabled: Bool,
        useLog: Bool,
        logLevel: NSInteger,
        extendedLog: Bool,
        purgeOldLogs: Bool,
        useBigFolderSizeLimit: Bool,
        bigFolderSizeLimit: Int64,
        maxAllowedCpu: Int,
        distributionChannel: NSInteger
    ) {
        self.language = language
        self.monoIcons = monoIcons
        self.autoStart = autoStart
        self.moveToTrash = moveToTrash
        self.darkTheme = darkTheme
        self.showShortcuts = showShortcuts
        self.notificationsDisabled = notificationsDisabled
        self.useLog = useLog
        self.logLevel = logLevel
        self.extendedLog = extendedLog
        self.purgeOldLogs = purgeOldLogs
        self.useBigFolderSizeLimit = useBigFolderSizeLimit
        self.bigFolderSizeLimit = bigFolderSizeLimit
        self.maxAllowedCpu = maxAllowedCpu
        self.distributionChannel = distributionChannel
    }

    public required init?(coder: NSCoder) {
        guard let language: NSString = coder.decodeObject(forKey: "language") as? NSString else {
            return nil
        }

        let bigFolderSizeLimit: Int64 = coder.decodeInt64(forKey: "bigFolderSizeLimit")
        let maxAllowedCpu: Int = coder.decodeInteger(forKey: "maxAllowedCpu")
        let logLevel: Int = coder.decodeInteger(forKey: "logLevel")
        let distributionChannel: Int = coder.decodeInteger(forKey: "distributionChannel")

        let monoIcons = coder.decodeBool(forKey: "monoIcons")
        let autoStart = coder.decodeBool(forKey: "autoStart")
        let moveToTrash = coder.decodeBool(forKey: "moveToTrash")
        let darkTheme = coder.decodeBool(forKey: "darkTheme")
        let showShortcuts = coder.decodeBool(forKey: "showShortcuts")
        let notificationsDisabled = coder.decodeBool(forKey: "notificationsDisabled")
        let useLog = coder.decodeBool(forKey: "useLog")
        let extendedLog = coder.decodeBool(forKey: "extendedLog")
        let purgeOldLogs = coder.decodeBool(forKey: "purgeOldLogs")
        let useBigFolderSizeLimit = coder.decodeBool(forKey: "useBigFolderSizeLimit")

        self.language = language
        self.monoIcons = monoIcons
        self.autoStart = autoStart
        self.moveToTrash = moveToTrash
        self.darkTheme = darkTheme
        self.showShortcuts = showShortcuts
        self.notificationsDisabled = notificationsDisabled
        self.useLog = useLog
        self.logLevel = logLevel
        self.extendedLog = extendedLog
        self.purgeOldLogs = purgeOldLogs
        self.useBigFolderSizeLimit = useBigFolderSizeLimit
        self.bigFolderSizeLimit = bigFolderSizeLimit
        self.maxAllowedCpu = maxAllowedCpu
        self.distributionChannel = distributionChannel
    }

    public func encode(with coder: NSCoder) {
        coder.encode(language, forKey: "language")
        coder.encode(monoIcons, forKey: "monoIcons")
        coder.encode(autoStart, forKey: "autoStart")
        coder.encode(moveToTrash, forKey: "moveToTrash")
        coder.encode(darkTheme, forKey: "darkTheme")
        coder.encode(showShortcuts, forKey: "showShortcuts")
        coder.encode(notificationsDisabled, forKey: "notificationsDisabled")
        coder.encode(useLog, forKey: "useLog")
        coder.encode(logLevel, forKey: "logLevel")
        coder.encode(extendedLog, forKey: "extendedLog")
        coder.encode(purgeOldLogs, forKey: "purgeOldLogs")
        coder.encode(useBigFolderSizeLimit, forKey: "useBigFolderSizeLimit")
        coder.encode(bigFolderSizeLimit, forKey: "bigFolderSizeLimit")
        coder.encode(maxAllowedCpu, forKey: "maxAllowedCpu")
        coder.encode(distributionChannel, forKey: "distributionChannel")
    }
}
