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

public extension [Factory] {
    func registerFactoriesInDI() {
        forEach { SimpleResolver.sharedResolver.store(factory: $0) }
    }
}

/// Each target should subclass `TargetAssembly` and override `getTargetServices` to provide additional, target related, services.
open class TargetAssembly {
    public init(testing: Bool) {
        Self.setupDI(testing: testing)
    }

    open class func getCommonServices(testing: Bool) -> [Factory] {
        return [
            Factory(type: CoherentCache.self) { _, _ in
                ServerCoherentCache()
            },
            Factory(type: CoherentCacheObservable.self) { _, resolver in
                try resolver.resolve(type: CoherentCache.self,
                                     forCustomTypeIdentifier: nil,
                                     factoryParameters: nil,
                                     resolver: resolver)
            },
            Factory(type: UpdaterCache.self) { _, _ in
                UpdaterStateCache()
            },
            Factory(type: UpdaterCacheObservable.self) { _, resolver in
                try resolver.resolve(type: UpdaterCache.self,
                                     forCustomTypeIdentifier: nil,
                                     factoryParameters: nil,
                                     resolver: resolver)
            },
            Factory(type: XPCConnectionProvider.self) { _, _ in
                if testing {
                    return XPCServerMock()
                } else {
                    return XPCConnectionManager()
                }
            },
            Factory(type: XPCQueryFetcherProtocol.self) { _, _ in
                XPCQueryFetcher()
            },
            Factory(type: XPCSignalHandlerProtocol.self) { _, _ in
                XPCSignalHandler()
            },
            Factory(type: AutoIncrementIDGenerator.self) { _, _ in
                AutoIncrementIDGenerator()
            },
            Factory(type: SyncCreator.self) { _, _ in
                SyncCreationService()
            },
            Factory(type: MacOSPermissionHandling.self) { _, _ in
                MacOSPermissionHandler()
            },
            Factory(type: NodeURLGenerator.self) { _, _ in
                DriveNodeURLGenerator()
            },
            Factory(type: StorageDataProviding.self) { _, _ in
                StorageDataService()
            }
        ]
    }

    open class func getTargetServices() -> [Factory] {
        IKLogger.general.error("targetServices is not implemented in subclass ? Did you forget to override ?")
        return []
    }

    public static func setupDI(testing: Bool) {
        (getCommonServices(testing: testing) + getTargetServices()).registerFactoriesInDI()

        // Startup XPC connection
        @InjectService var xpc: XPCConnectionProvider
    }
}
