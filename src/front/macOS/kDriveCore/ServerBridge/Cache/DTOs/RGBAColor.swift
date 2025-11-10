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
#if canImport(CoreGraphics)
import CoreGraphics
#endif

public struct RGBAColor: Sendable, CustomStringConvertible, Equatable {
    let red: CGFloat
    let green: CGFloat
    let blue: CGFloat
    let alpha: CGFloat

    public init?(hex: String) {
        var hexString = hex.trimmingCharacters(in: .whitespacesAndNewlines)
        if hexString.hasPrefix("#") {
            hexString.removeFirst()
        }

        guard hexString.count == 6 || hexString.count == 8,
              let hexValue = UInt64(hexString, radix: 16) else {
            return nil
        }

        if hexString.count == 6 {
            red = CGFloat((hexValue & 0xFF0000) >> 16) / 255.0
            green = CGFloat((hexValue & 0x00FF00) >> 8) / 255.0
            blue = CGFloat(hexValue & 0x0000FF) / 255.0
            alpha = 1.0
        } else {
            red = CGFloat((hexValue & 0xFF00_0000) >> 24) / 255.0
            green = CGFloat((hexValue & 0x00FF_0000) >> 16) / 255.0
            blue = CGFloat((hexValue & 0x0000_FF00) >> 8) / 255.0
            alpha = CGFloat(hexValue & 0x0000_00FF) / 255.0
        }
    }

    public init(red: CGFloat, green: CGFloat, blue: CGFloat, alpha: CGFloat = 1) {
        self.red = red
        self.green = green
        self.blue = blue
        self.alpha = alpha
    }

    func toHex(includeAlpha: Bool = true) -> String {
        let rInt = Int(round(red * 255))
        let gInt = Int(round(green * 255))
        let bInt = Int(round(blue * 255))
        let aInt = Int(round(alpha * 255))

        if includeAlpha && aInt <= 255 && aInt >= 0 {
            return String(format: "#%02x%02x%02x%02x", rInt, gInt, bInt, aInt)
        } else {
            return String(format: "#%02x%02x%02x", rInt, gInt, bInt)
        }
    }

    public var description: String {
        toHex(includeAlpha: false)
    }

    #if canImport(CoreGraphics)
    public var cgColor: CGColor {
        CGColor(red: red, green: green, blue: blue, alpha: alpha)
    }
    #endif
}
