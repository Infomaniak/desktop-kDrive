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

import Foundation
import OSLog

public extension IKLogger {
    private static let subsystem = Constants.bundleID

    static let view = IKLogger(subsystem: subsystem, category: "view")
    static let xpc = IKLogger(subsystem: subsystem, category: "XPC")
    static let data = IKLogger(subsystem: subsystem, category: "data")
    static let general = IKLogger(subsystem: subsystem, category: "general")
    static let debug = IKLogger(subsystem: subsystem, category: "debug")
}

public struct IKLogger: Sendable {
    let subsystem: String
    let category: String

    @available(macOS 11.0, *)
    private var logger: Logger {
        Logger(subsystem: subsystem, category: category)
    }

    public func log(_ message: String, file: StaticString = #fileID, line: UInt = #line) {
        logService.log(level: .debug, category: category, message: message, file: file, line: line)

        if #available(macOS 11.0, *) {
            logger.log("\(message)")
        } else {
            os_log(.default, "%@", message)
        }
    }

    public func debug(_ message: String, file: StaticString = #fileID, line: UInt = #line) {
        log(message, file: file, line: line)
    }

    public func info(_ message: String, file: StaticString = #fileID, line: UInt = #line) {
        logService.log(level: .info, category: category, message: message, file: file, line: line)

        if #available(macOS 11.0, *) {
            logger.info("\(message)")
        } else {
            os_log(.info, "%@", message)
        }
    }

    public func warning(_ message: String, file: StaticString = #fileID, line: UInt = #line) {
        logService.log(level: .warning, category: category, message: message, file: file, line: line)

        if #available(macOS 11.0, *) {
            logger.warning("\(message)")
        } else {
            os_log(.default, "%@", message)
        }
    }

    public func error(_ message: String, file: StaticString = #fileID, line: UInt = #line) {
        logService.log(level: .error, category: category, message: message, file: file, line: line)

        if #available(macOS 11.0, *) {
            logger.error("\(message)")
        } else {
            os_log(.error, "%@", message)
        }
    }

    public func fatal(_ message: String, file: StaticString = #fileID, line: UInt = #line) {
        logService.log(level: .fatal, category: category, message: message, file: file, line: line)

        if #available(macOS 11.0, *) {
            logger.fault("\(message)")
        } else {
            os_log(.fault, "%@", message)
        }
    }

    private var logService: LogService {
        LogService.shared
    }
}
