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
import SwiftUI

// swiftlint:disable nesting
public struct ColorToken {
    let light: ColorAsset
    let dark: ColorAsset

    public var asNSColor: NSColor {
        NSColor(light: light, dark: dark)
    }

    public var asColor: Color {
        Color(light: light, dark: dark)
    }

    public var asCGColor: CGColor {
        return asNSColor.cgColor
    }

    // MARK: - Semantic tokens (T2)

    public enum Accent {
        public static let primary = ColorToken(light: KDriveColors.blue600, dark: KDriveColors.blue400)
        public static let secondary = ColorToken(light: KDriveColors.kDrive500, dark: KDriveColors.kDrive600)
    }

    public enum Action {
        public static let primary = ColorToken.Accent.primary
        public static let onPrimary = ColorToken(light: KDriveColors.neutralBlue100, dark: KDriveColors.neutralBlue800)

        public enum Disabled {
            public static let dimQuaternary = ColorToken(light: KDriveColors.gray400, dark: KDriveColors.gray400)
        }
    }

    public enum Text {
        public static let primary = ColorToken(light: KDriveColors.neutralBlue800, dark: KDriveColors.neutralBlue50)
        public static let secondary = ColorToken(light: KDriveColors.neutralBlue600, dark: KDriveColors.neutralBlue200)
        public static let tertiary = ColorToken(light: KDriveColors.neutralBlue500, dark: KDriveColors.neutralBlue300)
    }

    public enum Surface {
        public static let primary = ColorToken(light: KDriveColors.neutralBlue50, dark: KDriveColors.neutralBlue800)
        public static let secondary = ColorToken(light: KDriveColors.neutralBlue100, dark: KDriveColors.neutralBlue700)
        public static let tertiary = ColorToken(light: KDriveColors.neutralBlue200, dark: KDriveColors.neutralBlue600)
        public static let quaternary = ColorToken(light: KDriveColors.neutralBlue300, dark: KDriveColors.neutralBlue500)
    }

    public enum Status {
        public enum Strong {
            public static let warning = ColorToken(light: KDriveColors.orange800, dark: KDriveColors.orange300)
        }

        public enum Medium {
            public static let success = ColorToken(light: KDriveColors.green500, dark: KDriveColors.green400)
            public static let warning = ColorToken(light: KDriveColors.orange500, dark: KDriveColors.orange400)
            public static let security = ColorToken(light: KDriveColors.blue500, dark: KDriveColors.blue400)
            public static let neutral = ColorToken(light: KDriveColors.gray500, dark: KDriveColors.gray400)
        }

        public enum Light {
            public static let warning = ColorToken(light: KDriveColors.orange100, dark: KDriveColors.brown950)
            public static let security = ColorToken(light: KDriveColors.blue100, dark: KDriveColors.blue950)
            public static let neutral = ColorToken(light: KDriveColors.neutralBlue100, dark: KDriveColors.gray950)
        }
    }

    // MARK: Contextual tokens (T3)

    public enum Drive {
        public static let defaultColor = ColorToken(light: KDriveColors.infomaniak, dark: KDriveColors.infomaniak)
    }
}

// swiftlint:enable nesting

// MARK: - Helpers

extension Color {
    init(light: ColorAsset, dark: ColorAsset) {
        self.init(nsColor: NSColor(light: light, dark: dark))
    }
}

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
