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
import kDriveCoreUI

@MainActor
public final class PreferencesRepository: ObservableObject {
    @LazyInjectService private var settingsCache: SettingsCaching

    @Published public private(set) var parametersInfo = UIParametersInfo()

    public init() {}

    public func refreshData() async throws {
        try await settingsCache.refresh()
        if let refreshedData = await settingsCache.getSettings() {
            parametersInfo = UIParametersInfo(parametersInfo: refreshedData)
        }
    }

    public func update<T>(_ keyPath: WritableKeyPath<UIParametersInfo, T>, value: T) async throws {
        var updatedParameters = parametersInfo
        updatedParameters[keyPath: keyPath] = value

        if await settingsCache.getSettings() == nil {
            try await settingsCache.refresh()
        }
        guard let currentData = await settingsCache.getSettings() else { return }

        let payload = updatedParameters.copyToParametersInfo(from: currentData)
        try await settingsCache.update(payload)

        if let refreshedData = await settingsCache.getSettings() {
            parametersInfo = UIParametersInfo(parametersInfo: refreshedData)
        }
    }
}
