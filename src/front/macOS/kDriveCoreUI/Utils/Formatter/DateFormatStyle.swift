/*
 Infomaniak kDrive - Desktop
 Copyright (C) 2023-2025 Infomaniak Network SA

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
import kDriveResources

public extension Date {
    struct FriendlyRelativeFormatStyle: Foundation.FormatStyle {
        public init() {}

        public func format(_ value: Date) -> String {
            if Date.now.addingTimeInterval(-60) > value {
                return value.formatted(.relative(presentation: .numeric, unitsStyle: .abbreviated))
            } else {
                return KDriveLocalizable.labelJustNow
            }
        }
    }
}

public extension FormatStyle where Self == Date.FriendlyRelativeFormatStyle {
    static var friendlyRelative: Self {
        return Date.FriendlyRelativeFormatStyle()
    }
}
