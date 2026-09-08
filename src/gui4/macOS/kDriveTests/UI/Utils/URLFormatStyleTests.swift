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
@testable import kDriveCoreUI
import Testing

@Suite("URLFormatStyle Tests")
struct URLFormatStyleTests {
    @Test("Format URL with file path component")
    func formatWithFilePathComponent() throws {
        let driveFolderName = "My Drive"
        let url = URL(fileURLWithPath: "/test/Documents/example.txt")
        let formatter = URL.NodeFormatStyle(driveFolderName: driveFolderName)

        let result = formatter.format(url)

        #expect(result == "example.txt")
    }

    @Test("Format URL with folder path component")
    func formatWithFolderPathComponent() throws {
        let driveFolderName = "My Drive"
        let url = URL(fileURLWithPath: "/test/Documents/Folder")
        let formatter = URL.NodeFormatStyle(driveFolderName: driveFolderName)

        let result = formatter.format(url)

        #expect(result == "Folder")
    }

    @Test("Format URL with root path returns drive name")
    func formatWithRootPath() throws {
        let driveFolderName = "My Drive"
        let url = URL(fileURLWithPath: "/")
        let formatter = URL.NodeFormatStyle(driveFolderName: driveFolderName)

        let result = formatter.format(url)

        #expect(result == driveFolderName)
    }

    @Test("Format URL without drive name returns last path component")
    func formatWithoutDriveNameReturnsPath() throws {
        let url = URL(fileURLWithPath: "/Users/test/Documents/example.txt")
        let formatter = URL.NodeFormatStyle()

        let result = formatter.format(url)

        #expect(result == "example.txt")
    }

    @Test("Format URL with root path and no drive name returns slash")
    func formatRootPathWithoutDriveName() throws {
        let url = URL(fileURLWithPath: "/")
        let formatter = URL.NodeFormatStyle()

        let result = formatter.format(url)

        #expect(result == "/")
    }

    @Test("Format URL with special characters in filename")
    func formatWithSpecialCharacters() throws {
        let driveFolderName = "My Drive"
        let url = URL(fileURLWithPath: "/test/Documents/file with spaces & symbols.txt")
        let formatter = URL.NodeFormatStyle(driveFolderName: driveFolderName)

        let result = formatter.format(url)

        #expect(result == "file with spaces & symbols.txt")
    }

    @Test("Format URL with unicode characters in filename")
    func formatWithUnicodeCharacters() throws {
        let driveFolderName = "My Drive"
        let url = URL(fileURLWithPath: "/Users/test/Documents/fichier_été_🎉.txt")
        let formatter = URL.NodeFormatStyle(driveFolderName: driveFolderName)

        let result = formatter.format(url)

        #expect(result == "fichier_été_🎉.txt")
    }
}

@Suite("URLFormatStyle Convenience Extension Tests")
struct URLFormatStyleConvenienceTests {
    @Test("Use convenience static method for formatting")
    func useConvenienceMethod() throws {
        let driveFolderName = "My Drive"
        let url = URL(fileURLWithPath: "/test/Documents/example.txt")

        let result = url.formatted(.node(driveFolderName: driveFolderName))

        #expect(result == "example.txt")
    }

    @Test("Use convenience method with root path")
    func useConvenienceMethodWithRootPath() throws {
        let driveFolderName = "My Drive"
        let url = URL(fileURLWithPath: "/")

        let result = url.formatted(.node(driveFolderName: driveFolderName))

        #expect(result == driveFolderName)
    }
}
