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

public extension UserDefaults {
    enum Key: Sendable {
        public static let isFirstLaunch = "isFirstLaunch"
        public static let shouldPresentOnboarding = "shouldPresentOnboarding"
        public static let selectedSynchroDbId = "selectedSynchroDbId"
    }
}

public extension UserDefaults {
    var selectedSynchroDbId: Int {
        get {
            integer(forKey: Key.selectedSynchroDbId)
        }
        set {
            set(newValue, forKey: Key.selectedSynchroDbId)
        }
    }
}
