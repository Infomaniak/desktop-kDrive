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
import OSLog

public extension IKLogger {
    private static let subsystem = Bundle.main.bundleIdentifier ?? "com.infomaniak.drive"

    static let view = IKLogger(subsystem: subsystem, category: "view")
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

    public func log(_ message: String) {
        if #available(macOS 11.0, *) {
            logger.log("\(message)")
        } else {
            os_log(.default, "%@", message)
        }
    }

    public func error(_ message: String) {
        if #available(macOS 11.0, *) {
            logger.error("\(message)")
        } else {
            os_log(.error, "%@", message)
        }
    }
}
