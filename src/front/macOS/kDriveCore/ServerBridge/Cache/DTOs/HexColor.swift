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

/// A lightweight exchange type mapping 1:1 the colors sent by the server (Hex RGB)
public struct HexColor: Sendable, CustomStringConvertible, Equatable, Hashable {
    let red: UInt8
    let green: UInt8
    let blue: UInt8

    public var description: String {
        // Server seems to run with lowercase Hex strings
        // Server exchange colors without alpha
        String(format: "#%02x%02x%02x", red, green, blue)
    }

    public init?(hex: String) {
        var hexString = hex.trimmingCharacters(in: .whitespacesAndNewlines)
        if hexString.hasPrefix("#") {
            hexString.removeFirst()
        }

        guard hexString.count == 6 || hexString.count == 8,
              let hexValue = UInt64(hexString, radix: 16) else {
            return nil
        }

        guard hexString.count == 6 else {
            return nil
        }
        red = UInt8((hexValue & 0xFF0000) >> 16)
        green = UInt8((hexValue & 0x00FF00) >> 8)
        blue = UInt8(hexValue & 0x0000FF)
    }

    public init(red: CGFloat, green: CGFloat, blue: CGFloat) {
        self.red = UInt8(round(red * 255))
        self.green = UInt8(round(green * 255))
        self.blue = UInt8(round(blue * 255))
    }

    #if canImport(CoreGraphics)
    public var cgColor: CGColor {
        CGColor(
            red: CGFloat(red) / 255.0,
            green: CGFloat(green) / 255.0,
            blue: CGFloat(blue) / 255.0,
            alpha: 1.0
        )
    }
    #endif
}
