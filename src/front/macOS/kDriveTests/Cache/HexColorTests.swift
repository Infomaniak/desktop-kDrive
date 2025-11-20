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
    typealias TestCase = (red: CGFloat, green: CGFloat, blue: CGFloat, hex: String)
    static let testCases: [TestCase] = [(1, 1, 1, "#ffffff"),
                                        (0, 0, 0, "#000000"),
                                        (1, 0, 1, "#ff00ff"),
                                        (0, 1, 0, "#00ff00"),
                                        (0.5, 0.6, 0.7, "#8099b3")]

    // MARK: init

    @Test(arguments: testCases)
    func testInitWithHex(
        expectedRed: CGFloat,
        expectedGreen: CGFloat,
        expectedBlue: CGFloat,
        inputHex: String
    ) {
        // GIVEN
        let colorFromRGBA = HexColor(red: expectedRed, green: expectedGreen, blue: expectedBlue)

        // WHEN
        let colorFromHex = HexColor(hex: inputHex)

        // THEN
        #expect(colorFromHex == colorFromRGBA)
    }

    @Test(arguments: ["", "#", "##", "###", "####", "#####", "######", "#######",
                      "#f", "#ff", "#fff", "#ffff", "#fffff", "#fffffff",
                      "f", "ff", "fff", "ffff", "fffff", "#fffffff"])
    func testInitFailure(invalidInput: String) {
        // WHEN
        let colorFromHex = HexColor(hex: invalidInput)

        // THEN
        #expect(colorFromHex == nil, "Invalid input should not init a valid color struct")
    }

    // MARK: Description

    @Test(arguments: testCases)
    func testDescriptionString(red: CGFloat, green: CGFloat, blue: CGFloat, expectedHex: String) {
        // GIVEN
        let color = HexColor(red: red, green: green, blue: blue)

        // WHEN
        let hexColor = color.description

        // THEN
        #expect(hexColor == expectedHex)
    }

    // MARK: CGColor

    #if canImport(CoreGraphics)
    @Test(arguments: [(1, 1, 1),
                      (0, 0, 0),
                      (1, 0, 1),
                      (0, 1, 0)])
    func testCGColorConversion(red: CGFloat, green: CGFloat, blue: CGFloat) {
        // GIVEN
        let expectedCGColor = CGColor(red: red, green: green, blue: blue, alpha: 1.0)
        let color = HexColor(red: red, green: green, blue: blue)

        // WHEN
        let cgColor = color.cgColor

        // THEN
        #expect(cgColor == expectedCGColor)
    }
    #endif
}
