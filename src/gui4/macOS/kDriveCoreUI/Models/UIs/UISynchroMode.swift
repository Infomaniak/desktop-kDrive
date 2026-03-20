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

import kDriveResources
import SwiftUI

public enum UISynchroMode: String, CaseIterable, Identifiable, Sendable {
    case storeOnline
    case availableOffline

    public var id: String {
        return rawValue
    }

    public var icon: Image {
        switch self {
        case .storeOnline:
            return KDriveResources.cloud.swiftUIImage
        case .availableOffline:
            return KDriveResources.computer.swiftUIImage
        }
    }

    var title: String {
        switch self {
        case .storeOnline:
            return KDriveLocalizable.storedOnline
        case .availableOffline:
            return KDriveLocalizable.availableOffline
        }
    }

    public var description: String {
        switch self {
        case .storeOnline:
            return KDriveLocalizable.storedOnlineDescription
        case .availableOffline:
            return KDriveLocalizable.availableOfflineDescription
        }
    }
}
