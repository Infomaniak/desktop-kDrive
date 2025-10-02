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
import InfomaniakDI
import OSLog

extension [Factory] {
    func registerFactoriesInDI() {
        forEach { SimpleResolver.sharedResolver.store(factory: $0) }
    }
}

/// Each target should subclass `TargetAssembly` and override `getTargetServices` to provide additional, target related, services.
open class TargetAssembly {
    public init() {
        Self.setupDI()
    }

    open class func getCommonServices() -> [Factory] {
        return [
            Factory(type: ServerBridgeable.self) { _, _ in
                ServerBridge()
            }
        ]
    }

    open class func getTargetServices() -> [Factory] {
        IKLogger.general.error("targetServices is not implemented in subclass ? Did you forget to override ?")
        return []
    }

    public static func setupDI() {
        (getCommonServices() + getTargetServices()).registerFactoriesInDI()
    }
}
