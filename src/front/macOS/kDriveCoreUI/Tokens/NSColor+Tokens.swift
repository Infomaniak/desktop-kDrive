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

import Cocoa
import kDriveResources

// swiftlint:disable nesting
extension NSColor {
    public enum Tokens {
        public enum Action {
            public static let primary = NSColor(light: KDriveColors.drive600, dark: KDriveColors.drive400)
            public static let onPrimary = NSColor(light: KDriveColors.neutralBlue100, dark: KDriveColors.neutralBlue800)
        }

        public enum Text {
            public static let primary = NSColor(light: KDriveColors.neutralBlue800, dark: KDriveColors.neutralBlue50)
            public static let secondary = NSColor(light: KDriveColors.neutralBlue600, dark: KDriveColors.neutralBlue200)
        }

        public enum Surface {
            public static let secondary = NSColor(light: KDriveColors.neutralBlue50, dark: KDriveColors.neutralBlue700)
        }
    }
}
// swiftlint:enable nesting

extension NSColor {
    convenience init(light: ColorAsset, dark: ColorAsset) {
        self.init(light: light.color, dark: dark.color)
    }

    convenience init(light: NSColor, dark: NSColor) {
        self.init(name: nil) { appearance in
            appearance.bestMatch(from: [.aqua, .darkAqua]) == .darkAqua ? dark : light
        }
    }
}
