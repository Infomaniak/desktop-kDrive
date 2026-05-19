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
import kDriveCore
import kDriveCoreUI

@MainActor
public final class ExclusionRepository: ObservableObject {
    @Published public var exclusionInfo = UIExclusionInfo()

    public init() {}

    public func refreshData() async throws {
        let userTemplates = try await ExclusionTemplateJobs().getExclusionTemplateList(default: false)
            .map { UIExclusionTemplateInfo(exclusionTemplateInfo: $0) }
        let defaultTemplates = try await ExclusionTemplateJobs().getExclusionTemplateList(default: true)
            .map { UIExclusionTemplateInfo(exclusionTemplateInfo: $0) }

        let userApps = try await ExclusionAppJobs().getExclusionAppList(default: false)
            .map { UIExclusionAppInfo(exclusionAppInfo: $0) }
        let defaultApps = try await ExclusionAppJobs().getExclusionAppList(default: true)
            .map { UIExclusionAppInfo(exclusionAppInfo: $0) }

        exclusionInfo = UIExclusionInfo(
            defaultExcludedTemplates: defaultTemplates,
            userExcludedTemplates: userTemplates,
            defaultExcludedApps: defaultApps,
            userExcludedApps: userApps
        )
    }

    public func updateTemplates(updatedTemplates: [UIExclusionTemplateInfo]) async throws {
        try await updateItems(
            \.exclusionInfo.userExcludedTemplates,
            items: updatedTemplates
        ) { items in
            try await ExclusionTemplateJobs().setUserExclusionTemplateList(items.toExclusionTemplateInfo())
        }
    }

    public func updateApps(updatedApps: [UIExclusionAppInfo]) async throws {
        try await updateItems(
            \.exclusionInfo.userExcludedApps,
            items: updatedApps
        ) { items in
            try await ExclusionAppJobs().setExclusionAppList(default: false, applicationList: items.toExclusionAppInfo())
        }
    }

    private func updateItems<T>(
        _ keyPath: ReferenceWritableKeyPath<ExclusionRepository, [T]>,
        items: [T],
        remote: ([T]) async throws -> Void
    ) async throws {
        let oldItems = self[keyPath: keyPath]
        do {
            self[keyPath: keyPath] = items
            try await remote(items)
            try? await refreshData()
        } catch {
            self[keyPath: keyPath] = oldItems
            throw error
        }
    }
}
