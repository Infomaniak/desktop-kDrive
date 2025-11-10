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

@testable import kDriveCore
import Testing
#if canImport(CoreGraphics)
import CoreGraphics
#endif

struct HexColorTests {
    // MARK: init

    @Test(arguments: [(1, 1, 1, 1, "#ffffffff"),
                      (0, 0, 0, 0, "#00000000"),
                      (1, 0, 1, 0, "#ff00ff00"),
                      (0, 1, 0, 1, "#00ff00ff"),
                      (0.5, 0.6, 0.7, 0.8, "#8099b3cc")])
    func testInitWithHex(
        expectedRed: CGFloat,
        expectedGreen: CGFloat,
        expectedBlue: CGFloat,
        expectedAlpha: CGFloat,
        inputHex: String
    ) {
        // GIVEN
        let colorFromRGBA = HexColor(red: expectedRed, green: expectedGreen, blue: expectedBlue, alpha: expectedAlpha)

        // WHEN
        let colorFromHex = HexColor(hex: inputHex)

        // THEN
        #expect(colorFromHex == colorFromRGBA)
    }

    // MARK: toHex()

    @Test(arguments: [(1, 1, 1, 1, "#ffffff"),
                      (0, 0, 0, 0, "#000000"),
                      (1, 0, 1, 0, "#ff00ff"),
                      (0, 1, 0, 1, "#00ff00"),
                      (0.5, 0.6, 0.7, 0.8, "#8099b3")])
    func testToHexStringExcludingAlpha(red: CGFloat, green: CGFloat, blue: CGFloat, alpha: CGFloat, expectedHex: String) {
        // GIVEN
        let color = HexColor(red: red, green: green, blue: blue, alpha: alpha)

        // WHEN
        let hexColor = color.toHex(includeAlpha: false)

        // THEN
        #expect(hexColor == expectedHex)
    }

    @Test(arguments: [(1, 1, 1, 1, "#ffffffff"),
                      (0, 0, 0, 0, "#00000000"),
                      (1, 0, 1, 0, "#ff00ff00"),
                      (0, 1, 0, 1, "#00ff00ff"),
                      (0.5, 0.6, 0.7, 0.8, "#8099b3cc")])
    func testToHexStringWithAlpha(red: CGFloat, green: CGFloat, blue: CGFloat, alpha: CGFloat, expectedHex: String) {
        // GIVEN
        let color = HexColor(red: red, green: green, blue: blue, alpha: alpha)

        // WHEN
        let hexColor = color.toHex(includeAlpha: true)

        // THEN
        #expect(hexColor == expectedHex)
    }

    // MARK: Description

    @Test(arguments: [(1, 1, 1, 1, "#ffffff"),
                      (0, 0, 0, 0, "#000000"),
                      (1, 0, 1, 0, "#ff00ff"),
                      (0, 1, 0, 1, "#00ff00"),
                      (0.5, 0.6, 0.7, 0.8, "#8099b3")])
    func testDescriptionString(red: CGFloat, green: CGFloat, blue: CGFloat, alpha: CGFloat, expectedHex: String) {
        // GIVEN
        let color = HexColor(red: red, green: green, blue: blue, alpha: alpha)

        // WHEN
        let hexColor = color.description

        // THEN
        #expect(hexColor == expectedHex)
    }

    // MARK: CGColor

    #if canImport(CoreGraphics)
    @Test(arguments: [(1, 1, 1, 1),
                      (0, 0, 0, 0),
                      (1, 0, 1, 0),
                      (0, 1, 0, 1)])
    func testCGColorConversion(red: CGFloat, green: CGFloat, blue: CGFloat, alpha: CGFloat) {
        // GIVEN
        let expectedCGColor = CGColor(red: red, green: green, blue: blue, alpha: alpha)
        let color = HexColor(red: red, green: green, blue: blue, alpha: alpha)

        // WHEN
        let cgColor = color.cgColor

        // THEN
        #expect(cgColor == expectedCGColor)
    }
    #endif
}
