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

import Foundation
import kDriveCore

public protocol UIExclusionItem: Identifiable, Hashable {
    var id: UUID { get set }
    var displayName: String { get }
}

public struct UIExclusionTemplateInfo: Sendable, UIExclusionItem {
    public var id = UUID()
    public var displayName: String
    public var `default`: Bool
    public var warning: Bool

    public init(exclusionTemplateInfo: ExclusionTemplateInfo) {
        displayName = exclusionTemplateInfo.template
        self.default = exclusionTemplateInfo.default
        warning = exclusionTemplateInfo.warning
    }

    public init(template: String, warning: Bool) {
        displayName = template
        self.warning = warning
        self.default = false
    }
}

public struct UIExclusionAppInfo: Sendable, UIExclusionItem {
    public var id = UUID()
    public var app: String
    public var `default`: Bool
    public var displayName: String

    public init(exclusionAppInfo: ExclusionAppInfo) {
        app = exclusionAppInfo.appId
        self.default = exclusionAppInfo.def
        displayName = exclusionAppInfo.description
    }

    public init(app: String, description: String) {
        self.app = app
        self.default = false
        displayName = description
    }
}

public extension [UIExclusionAppInfo] {
    func toExclusionAppInfo() -> [ExclusionAppInfo] {
        return map { ExclusionAppInfo(appId: $0.app, description: $0.displayName, def: $0.default) }
    }
}

public extension [UIExclusionTemplateInfo] {
    func toExclusionTemplateInfo() -> [ExclusionTemplateInfo] {
        return map { ExclusionTemplateInfo(template: $0.displayName, warning: $0.warning, default: $0.default) }
    }
}

public struct UIExclusionInfo: Sendable {
    public var defaultExcludedTemplates: [UIExclusionTemplateInfo]
    public var userExcludedTemplates: [UIExclusionTemplateInfo]
    public var defaultExcludedApps: [UIExclusionAppInfo]
    public var userExcludedApps: [UIExclusionAppInfo]

    public init() {
        defaultExcludedTemplates = []
        userExcludedTemplates = []
        defaultExcludedApps = []
        userExcludedApps = []
    }
}
